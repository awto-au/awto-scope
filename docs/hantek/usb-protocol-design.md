# New-firmware USB Protocol Design — Fluke-inspired, streaming-first

**STATUS: design proposal, nothing implemented.** This document specifies a protocol for the
**clean-room Zephyr reimplementation** (`code/app/`, `code/a2k_board/`, `code/lib/`) — it
is a fresh design, not a compatibility layer for existing Hantek software.

**Not to be confused with** [`../hardware/usb-protocol.md`](usb-protocol.md), which
documents the **existing vendor Hantek 2D82AUTO protocol** (RE'd from `HTHardDll.dll`/`Scope.exe`):
fixed 10-byte poll/request-response packets, one command per packet, ~20 Hz main-loop poll rate,
no true streaming — waveform data is pulled via hundreds of small repeated requests rather than
pushed. That document is reference material for interoperating with the OEM app/hardware during
RE; this document is what the *new* firmware should speak instead. Where useful this doc
cross-references specific vendor-protocol behaviours as "what we are deliberately not doing."

---

## 1. Executive summary / goals

The new firmware's host protocol should feel like a modern bench instrument, not a game
controller polled at 20 Hz. Three goals, in priority order:

1. **Fluke-like UX** — self-describing (`*IDN?`-style capability queries), human-typeable and
   scriptable command set, clear structured error reporting, predictable framing. A user (or a
   `tio`/`minicom` session, or a Python script) should be able to type a command and get a
   sensible answer or a sensible error, not silence or a magic byte pattern that only the vendor
   GUI understands.
2. **Streaming-first, not poll-first** — the device should be able to *push* waveform samples,
   GPIO transitions, and other continuous data to the host as they occur, without the host
   re-requesting on a timer. The vendor protocol's request/reply-per-64-bytes model (see
   `usb-protocol.md` §"Waveform Data") is the thing being replaced, not emulated.
3. **Small packets are a feature, not a limitation** — USB Full Speed bulk transfers on the
   STM32F103 (12 Mbit/s, this MCU has no High-Speed PHY — confirmed from `vendors/hantek/2d82/ghidra/STM32F103.svd`,
   which names the peripheral `USB_OTG_FS`) top out at 64-byte max packet size for bulk endpoints.
   Rather than fighting this, the design leans into small, fixed-size, self-framed records that
   stream continuously — the same pattern used by USB logic analyzers and other
   continuously-streaming FS instruments (§2.4). §2.5/§4.3 checked this specifically against how
   Keysight/Tektronix/Rigol bench scopes handle large waveform transfers on their (much faster,
   High-Speed/SuperSpeed) USB links, and confirm rather than overturn this goal: those vendors'
   "request everything in one large block" pattern is a property of their faster USB link, not a
   better idiom this device should import — this device's small-record design is the right choice
   *because of*, not despite, its Full-Speed ceiling.

Two concrete sub-features are in scope and specified in full below: a USB CDC-ACM shell port
(§5) and a GPIO state-streaming protocol for bring-up/debug (§6).

---

## 2. Survey of relevant existing protocols

### 2.1 SCPI (Standard Commands for Programmable Instruments)

SCPI is a public, mature, IEEE-488.2-derived text command language standard used across the test
and measurement industry (Keysight, Tektronix, Rohde & Schwarz, and — per publicly available
Fluke programmer manuals, see §2.2 — Fluke). Core, verifiable facts about the standard itself:

- Commands are ASCII text, hierarchical with colon-separated nodes (e.g. `MEASure:VOLTage:DC?`),
  mnemonics have a mandatory short form (upper-case in docs) and optional long form.
- IEEE-488.2 defines a mandatory *common command* subset every SCPI instrument must implement,
  including `*IDN?` (identification query — manufacturer, model, serial, firmware rev, comma
  separated), `*RST` (reset), `*CLS` (clear status), `*OPC` / `*OPC?` (operation complete),
  `*ESR?` / `*STB?` (status/event registers), `*TST?` (self-test).
  Source: IEEE Std 488.2-1992, "IEEE Standard Codes, Formats, Protocols, and Common Commands for
  Use with ANSI/IEEE Std 488.1-1987"; the SCPI Consortium's *SCPI-99* specification volumes 1-4
  (the standards body was later folded into the IVI Foundation, https://www.ivifoundation.org/).
- SCPI itself does not mandate a transport — it is commonly carried over GPIB, RS-232, USBTMC, or
  raw TCP/telnet (as in LXI/VXI-11 instruments). This is directly useful for us: nothing about
  SCPI syntax presumes GPIB-style framing.
- Query commands end in `?`, responses are text (often self-describing units/formats), errors are
  queryable via the standard `SYSTem:ERRor?` queue rather than being silently dropped or requiring
  the host to infer failure from a timeout.

### 2.2 Fluke's published use of SCPI — what's actually verifiable

Fluke's instrument firmware internals are not open source. Direct research into Fluke's own
published manuals (not secondary summaries) shows a clear **stratification by product tier** —
this is more nuanced, and more useful for this design, than a blanket "Fluke uses SCPI" claim:

- **Bench/reference multimeters — real SCPI, officially documented, but not native USB.** The
  Fluke 8845A/8846A precision multimeters are controlled via "Standard Commands for Programmable
  Instruments (SCPI) or Fluke 45 commands" (legacy emulation mode), per their official Programmers
  Manual. Verified example commands: `*IDN?`, `*RST`, `*CLS`, `*OPC`/`*OPC?`, `SYST:REM` (required
  on RS-232 to enable remote mode — unlike GPIB, it isn't automatic), `CONF:VOLT:DC 0.1`,
  `MEAS:VOLT:AC? 10`, `READ?`, `INIT`, `TRIG:SOUR BUS`, `FETCH?`. Interfaces are RS-232, GPIB, and
  Ethernet — **no native USB**; Fluke's own manual describes a USB-to-RS-232 adapter cable
  (Fluke PN 2675479) for USB-only PCs, i.e. USB carries SCPI ASCII over an RS-232 tunnel, not
  USBTMC. Source: [Fluke 8845A/8846A Programmers Manual](https://assets.fluke.com/manuals/8845a___pmeng0100.pdf).
  A real-world friction point is documented independently: an EEVblog user's SCPI commands failed
  over RS-232 until they sent `SYST:REM` first — a manual-clarity gap, not a protocol flaw.
  Source: [EEVblog — Fluke 8845A SCPI help thread](https://www.eevblog.com/forum/testgear/help-needed-with-fluke-8845a-scpi-commands/).
  The higher-end **8588A** reference calibration meter explicitly lists **USBTMC** support
  alongside Ethernet/GPIB ("Keysight 3458A SCPI command compatibility"). Source:
  [Fluke 8588A product page](https://www.fluke.com/en-us/product/calibration-tools/electrical-calibration/bench-multimeters/8588a).
  So real USBTMC-over-USB is a genuine, if top-tier-only, Fluke pattern — not merely a plausible
  inference.
- **Handheld DMMs (Fluke 289/287) — proprietary, explicitly NOT SCPI.** Fluke's own "289/287
  Remote Interface Specification" (an informally-released document — its own text warns "Product
  Support Service will not be able to answer any questions concerning this document") specifies a
  short 2-4-letter ASCII command set over an IR-optical RS-232 link (115200 8N1): `ID`, `QM`
  (Query Measurement, e.g. reply `-0.023E-3,VDC,NORMAL,NONE`), `QDDA`, `RI`, `RMP`, `DS` — **not**
  SCPI syntax, **not** USBTMC. USB connectivity is via a proprietary Fluke USB-to-IR adapter built
  on an FTDI FT232 chip (i.e. USB-CDC serial carrying this proprietary ASCII protocol).
  Sources: [Fluke 289/287 Remote Interface Specification](https://www.uniteng.com/neildocs/facilities/Fluke289_remote_spec28X.pdf),
  [sigrok Fluke 287/289 wiki](https://sigrok.org/wiki/Fluke_287/289). Notably, Fluke documents only
  the basic query commands — the deeper FlukeView Forms data-logging/programming protocol is
  explicitly undocumented ("not available"), which forced open-source reverse engineering
  (the `sigrok` `fluke-dmm` driver) to fill the gap — directly analogous to this project's own
  position relative to Hantek's undocumented `HTHardDll.dll` protocol.
  Source: [EEVblog — Fluke custom USB-IR cable thread](https://www.eevblog.com/forum/testgear/fluke-custom-usb-ir-cable/).
- **ScopeMeter (190-series) — proprietary "CPL" protocol over USB-CDC virtual COM port.** Fluke's
  own 4000+-line "Remote Control and Programming Reference for all Fluke 190 family" names the
  protocol explicitly as **CPL** throughout. USB is a **virtual RS-232/COM port** (USB-CDC, needing
  a "Fluke USB Serial Port" driver) — not USBTMC. The command set borrows a handful of IEEE-488.2
  common commands (`*IDN?`, `*RST`, `*CLS`, `*OPC`/`*OPC?`) as a thin self-ID/status veneer, but the
  bulk of the command set is proprietary CPL with its own status/error word (`ST` — e.g. bit 8 =
  "Invalid command," bit 16 = "Command not implemented") rather than a SCPI hierarchical command
  tree. Source: [Fluke 190-family Remote Control and Programming Reference](https://media.fluke.com/d602147f-0db6-43c4-8d91-b10800c14f4e_original%20file.pdf).
  This is the Fluke product closest in form factor to the 2D82AUTO (a handheld combo scope/meter)
  — and it is the proprietary-CPL-over-CDC pattern, not full SCPI, that is its closest real-world
  precedent.
- **Conclusion for this doc:** "Fluke does SCPI" is **confirmed, but only for the bench/reference
  metrology tier** (8845A/8846A/8588A-class) — and even there, native USBTMC is reserved for the
  top-tier 8588A, with 8845A/8846A only reaching SCPI via a USB-to-RS232 adapter. The product
  class actually closest to this device (a handheld combo scope/meter) uses a **proprietary
  ASCII command set over USB-CDC serial, informally documented for basic operations only, with
  the richer protocol left undocumented and reverse-engineered by the community** — i.e. the
  *opposite* of "Fluke-like" if that phrase implies full SCPI compliance. Given this, the
  "Fluke-like" goal in this doc is reframed deliberately: it means *emulating the best of what
  Fluke's documentation shows across the tiers* (self-describing `*IDN?`-style identification, a
  status/error-word or error-queue mechanism instead of silence, human-typeable ASCII commands
  usable from a terminal) — a synthesis of Fluke's bench-meter SCPI polish and its handheld
  product line's pragmatic proprietary-ASCII-over-CDC transport choice — rather than a claim that
  full SCPI/USBTMC compliance is what "being like Fluke" would actually mean for a device in this
  product class. §3 reflects this: SCPI *syntax* (public standard, freely adoptable) carried over
  a lightweight custom framing, not full USBTMC.

### 2.3 USBTMC — the standard USB transport for SCPI

USBTMC (**USB Test and Measurement Class**) is a USB-IF-published device class specification,
still commonly cited as the standard way SCPI instruments talk over USB:

- Full title (confirmed via the Linux kernel's own ABI docs, which cite it by name and section):
  *"Universal Serial Bus Test and Measurement Class Specification (USBTMC)"*, **Revision 1.0**,
  USB Implementers Forum. Listed at [usb.org's Test & Measurement Class Specification
  page](https://www.usb.org/document-library/test-measurement-class-specification); the
  authoritative secondary source used here is [kernel.org's `sysfs-driver-usb-usbtmc` ABI
  doc](https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-driver-usb-usbtmc) (the primary
  USB-IF PDF itself returned 403/404 on direct fetch during this research pass — noted as a gap,
  not filled with invention; the Linux kernel driver source and ABI docs quote the spec's own
  section numbers directly, which is why they're treated as reliable here).
- Defines a **bulk-only transport**: a bulk OUT and bulk IN endpoint, every transfer prefixed with
  a **12-byte message header**: `MsgID` (message type), **`bTag`** (1-byte transaction tag,
  host-incremented, wraps 1-255), **`bTagInverse`** (`bTag XOR 0xFF`, a corruption check baked
  into the header itself), a reserved byte, then a message-specific body — commonly a 4-byte
  little-endian transfer size plus an **EOM** (end-of-message) flag bit and padding. Core message
  types: `DEV_DEP_MSG_OUT` (host→device data), `REQUEST_DEV_DEP_MSG_IN` (host asks for a
  response, itself sent via bulk-OUT), `DEV_DEP_MSG_IN` (the actual response, via bulk-IN) — i.e.
  exactly the "small fixed header + payload, self-framed over bulk, with a tag for matching
  request/response" pattern this project's §3 framing borrows. Source:
  [python-usbtmc reference implementation](https://github.com/python-ivi/python-usbtmc/blob/master/usbtmc/usbtmc.py),
  cross-checked against [Linux kernel `usbtmc.c`](https://github.com/torvalds/linux/blob/master/drivers/usb/class/usbtmc.c).
- The **USBTMC-USB488 subclass** (full title, cross-confirmed by two independent third-party PDF
  mirrors with matching title/date: *"Universal Serial Bus Test and Measurement Class, Subclass
  USB488 Specification (USBTMC-USB488)"*, Revision 1.0, dated 2003-04-14 — see
  [mirror 1](http://sites.science.oregonstate.edu/~hetheriw/whiki/py/topics/usb/files/usb_instruments/USBTMC_usb488_subclass_1_00.pdf),
  [mirror 2](http://sdpha2.ucsd.edu/Lab_Equip_Manuals/usbtmc_usb488_subclass_1_00.pdf)) layers
  IEEE-488.1/488.2 (GPIB) semantics on top: a `GET_CAPABILITIES` bitfield (e.g. SR1 = service-
  request capable), `REN_CONTROL`/`GO_TO_LOCAL`/`LOCAL_LOCKOUT` control requests mirroring GPIB's
  REN/GTL/LLO, a `TRIGGER` message (Group Execute Trigger equivalent), and `READ_STATUS_BYTE`
  exposing the IEEE-488.2 status byte.
- **Directly relevant precedent for this doc's streaming-vs-control split (§4):** a USB488 device
  claiming SR1 capability must expose a separate **Interrupt-IN endpoint** purely for async
  Service Request notification (device pushes a `bNotify1` packet with the status byte on an SRQ
  condition) — a spec-documented example of splitting an asynchronous/out-of-band signal onto its
  own endpoint, distinct from the main bulk command/response pair. Source:
  [NI forums — USBTMC and the USB488 subclass](https://forums.ni.com/t5/Instrument-Control-GPIB-Serial/USBTMC-and-the-USB488-subclass/td-p/334947),
  corroborated by [element14 — Understanding TinyUSB USBTMC protocols](https://community.element14.com/technologies/test-and-measurement/b/blog/posts/understand-tinyusb-usbtmc-protocols).
- **VISA** (Virtual Instrument Software Architecture, IVI Foundation) is the host-side abstraction
  that lets the same application code address GPIB/USBTMC/LXI/serial instruments interchangeably
  via resource strings like `USB0::0x0483::0x2D42::<serial>::INSTR`
  ([VISA resource syntax reference](https://documentation.help/NI-VISA/VISAResourceSyntaxandExamples.html)).
  Keysight's own developer documentation states Keysight "and other test equipment vendors
  co-developed" USBTMC/USB488 — independent confirmation this is a real cross-vendor industry
  standard, not a single-vendor scheme. Source:
  [Keysight System Developer Guide](https://keysight.zinfi.net/concierge/OEMs/keysight/wwwcontent/Attachments/pdf/6773_5989-1417EN.pdf).
- **Why this project does not adopt USBTMC wholesale:** USBTMC's framing is designed around
  discrete, on-demand request/response *messages* of arbitrary length (a SCPI query and its
  reply), not a continuous low-latency push stream — its interrupt-IN "notify" path is for rare
  asynchronous events, not sustained sample-rate data. It's also a nontrivial amount of descriptor
  and state-machine complexity for a bring-up-stage MCU project. This design borrows USBTMC's
  *ideas* — small self-framed headers with a tag, message-boundary flags, a value in exposing
  something SCPI-like — without implementing the full USBTMC class/descriptor machinery. This is
  a pragmatic choice for an MCU-hosted, single-developer clean-room firmware, not a claim that
  USBTMC is unsuitable in general.
- **How USBTMC actually fragments a large `DEV_DEP_MSG_IN` response across multiple bulk-IN
  packets — read directly from source, not just the spec summary.** This matters specifically for
  §2.5/§4 below (large waveform transfers), so it's worth being precise about the wire-level
  mechanics rather than the higher-level message picture in the bullets above. Source: the Linux
  in-tree USBTMC class driver,
  [`drivers/usb/class/usbtmc.c`](https://github.com/torvalds/linux/blob/master/drivers/usb/class/usbtmc.c)
  (`send_request_dev_dep_msg_in()` and `usbtmc_read()`), which implements the spec's Table 4/8
  message flow byte-for-byte and is a more precise reference than any secondary summary:
  - The host first sends a 12-byte `REQUEST_DEV_DEP_MSG_IN` message on the bulk-**OUT** endpoint,
    specifying a `TransferSize` (how many bytes the host is willing to accept this round) and a
    `bTag`/`bTagInverse` pair for matching the response.
  - The device's response — one logical `DEV_DEP_MSG_IN` **message** — is **not** re-framed with a
    header on every USB packet. Only the *first* bulk-IN packet of the response carries the
    12-byte USBTMC header (`MsgID=2`, echoed `bTag`, and a 4-byte little-endian `N_characters`
    field giving the *total* payload length for this message); every subsequent bulk-IN packet in
    the same response is **raw, unframed payload** — the host driver just keeps issuing bulk-IN
    reads (each capped at the endpoint's `wMaxPacketSize`) until it has received `N_characters`
    bytes total (plus header on the first packet, plus optional zero-pad to end the transfer
    cleanly on an exact multiple of `wMaxPacketSize`, per USB bulk transfer termination rules).
  - **Concretely: fragmentation across "one bulk transfer's worth" of data is handled entirely by
    the ordinary USB bulk-transfer chunking (`wMaxPacketSize`-sized packets, terminated by a
    short/zero-length packet) — USBTMC itself does not re-header every chunk.** One
    `REQUEST_DEV_DEP_MSG_IN` → one long bulk-IN byte stream → the *host* (not the device) is
    responsible for knowing when to stop reading, using the `N_characters` count from the single
    leading header. This is a materially different, and simpler, design than the record-per-packet
    scheme already specified in §4.1 below (every streaming record in §4.1 carries its own
    sequence/timestamp header) — see §2.5 for why §4.1's record-per-packet approach is still the
    right call for *this* device's very different transport (continuous push, not a discrete
    on-demand pull).
  - **Abort mechanics** (relevant to point 4 in this pass's task and to §4.1's overrun handling):
    USBTMC defines four **control-endpoint** (EP0) requests, confirmed in the same kernel source
    (`usbtmc_ioctl_abort_bulk_in_tag()` / `usbtmc_ioctl_abort_bulk_out_tag()`):
    `INITIATE_ABORT_BULK_IN` / `INITIATE_ABORT_BULK_OUT` (host tells the device "stop sending/
    accepting the message tagged `bTag`, discard it"), each paired with a
    `CHECK_ABORT_BULK_IN_STATUS` / `CHECK_ABORT_BULK_OUT_STATUS` control request the host polls
    afterward until the device reports the abort actually completed (the abort is asynchronous —
    a single control transfer doesn't guarantee the in-flight bulk data is already flushed). Using
    the **control endpoint** for this (not the bulk pipe carrying the data itself) is the important
    design point: an in-flight multi-packet bulk transfer can be cancelled out-of-band without
    needing the bulk pipe itself to be free or synchronized, and without the device needing to
    parse an abort *command* out of the same stream it's mid-transfer on. Real client software
    exercises this: the Linux driver auto-invokes `usbtmc_ioctl_abort_bulk_in`/`_out` whenever a
    bulk transfer errors, times out, or returns a malformed header (wrong `bTag`, undersized
    header, device claiming more bytes than requested) — i.e. abort is the documented recovery
    path for exactly the "transfer interrupted mid-stream" scenario this pass was asked to check,
    not an edge case. Keysight's own programmer's guides (§2.5 below) don't talk about this
    control-endpoint mechanism directly — VISA/USBTMC driver layers hide it — but *do* document an
    equivalent host-visible abort primitive, **Device Clear** (`myScope.IO.Clear` in their VISA COM
    examples), used pervasively through their example code and explicitly documented as the way to
    "halt a `:DIGitize` in progress." Device Clear is IEEE-488/USBTMC's older, coarser
    "reset everything" primitive (clears *all* pending I/O, not just one tagged transfer);
    `INITIATE_ABORT_BULK_IN`/`_OUT` is the finer-grained, tag-scoped USBTMC-specific mechanism.
    Both are real, standards-documented answers to "the host wants to bail out of a large transfer
    in flight" — this project's own abort design (§4.1, revised below) borrows the *shape* of the
    finer-grained one (control-plane-driven, tag/id-scoped, host polls for confirmation) without
    needing the two-tier IEEE-488/USBTMC vocabulary.

### 2.4 Streaming-USB-instrument patterns (logic analyzers, USB scopes)

For continuous-push design, the relevant prior art is **USB logic analyzers and low-to-mid-range
USB oscilloscopes**, which — like this device — stream at moderate rates over USB Full Speed
bulk, not isochronous. Direct research into an actual open-source implementation (rather than
just general USB theory) confirms the pattern with a concrete example:

- **Bulk vs isochronous vs interrupt, for this class of device:**
  - *Isochronous* gets guaranteed bandwidth/latency but **no retry** — a corrupted/lost packet is
    just dropped, by design (retries would break the fixed-latency guarantee). Fine for audio/
    video where a glitch is perceptually masked; wrong for sample data where a dropped byte
    corrupts the decode. Source: [USB in a NutShell — transfer types](https://www.beyondlogic.org/usbnutshell/usb4.shtml).
  - *Bulk* gets CRC error detection **with guaranteed retransmission**, using spare bus bandwidth
    opportunistically rather than a fixed reservation — the standard choice for "large bursty
    correctness-critical data," and already what this device uses today (`usb-protocol.md` Bulk
    EP OUT `0x02` / EP IN `0x81`). Same source, corroborated by
    [USB for microcontrollers pt.4 — handling large amounts of data](https://medium.com/@manuel.bl/usb-for-microcontrollers-part-4-handling-large-amounts-of-data-f577565c4c7d).
  - *Interrupt* is for small, latency-sensitive, infrequent notifications (status changes), not
    bulk payload — matches this design's proposed stream-lifecycle notification use (§4's `0x7F`
    record type folds this into the bulk stream instead of a 4th endpoint, for STM32F103
    endpoint-budget reasons, §5.2), not the sample data itself.
  - **Concrete reference implementation** — the Cypress FX2LP-based `fx2lafw` firmware (used by
    many sigrok-supported USB logic analyzers, the closest real-world analog to this project's
    GPIO-streaming goal): EP2 is configured bulk-IN, quad-buffered, with the FX2's hardware GPIF
    engine feeding sampled GPIO data directly into the EP2 FIFO with **no per-sample CPU
    involvement** — this is specifically what lets an 8-bit MCU sustain full-rate streaming, and
    is a directly relevant precedent for keeping the STM32F103's CPU off the per-sample critical
    path (DMA-fed USB buffers, not CPU-copied). Source:
    [sigrok-firmware-fx2lafw, fx2lafw.c](https://github.com/sigrokproject/sigrok-firmware-fx2lafw/blob/master/fx2lafw.c).
  - **STM32-specific endpoint guidance** (same MCU family as this project): ST's own community
    documentation recommends Bulk IN + Bulk OUT for data plus a separate Interrupt IN for control/
    status "preventing one type from blocking the other," confirms Full-Speed max packet sizes
    (64 B for control/interrupt/bulk, 1023 B for isochronous), and notes flow control is via USB's
    own ACK/NAK — firmware NAKs when its endpoint buffer is full, which pauses the host without
    data loss (this, not application-level acknowledgement, is what makes bulk reliable
    end-to-end — directly informs §4.1's "no per-record ACK needed" design). Source:
    [ST community — selecting USB endpoints for STM32](https://community.st.com/t5/stm32-mcus/how-to-select-suitable-endpoints-for-your-stm32-usb-application/ta-p/719823).
  - No single authoritative FS bulk throughput number is cited here — actual throughput is host/
    driver/OS/firmware-buffering dependent. Treat "roughly 1 MB/s achievable, low hundreds of
    KB/s realistic for a small-buffer MCU device" as an unverified planning envelope only (§7
    tracks benchmarking this on real hardware as follow-up work, not a citable fact here).
  - This matches the existing analysis in `usb-protocol.md` ("MCU main loop polls USB at ~20 Hz")
    — the current bottleneck is **firmware-side polling cadence**, not USB FS bandwidth. Moving to
    an interrupt/DMA-driven USB stack with a dedicated streaming endpoint removes that bottleneck
    without needing High Speed silicon.
- **Small-packet framing — the real-world pattern is actually leaner than expected.** The
  fx2lafw/libsigrok reference implementation uses **no sequence numbers and no sync/escape bytes
  in the raw sample stream at all**: logic-capture data is raw sample bytes with framing implicit
  from the fixed record layout already negotiated by the preceding START control-transfer command
  (sample width, channel count) — not from in-band markers.
  Source: [libsigrok fx2lafw/protocol.c](https://github.com/sigrokproject/libsigrok/blob/master/src/hardware/fx2lafw/protocol.c).
  Other sigrok-documented devices follow the same minimal-framing pattern: the **Openbench Logic
  Sniffer** uses fixed 4-byte-per-sample blocks with no start/end markers, relying on an
  inter-byte timeout gap to infer end-of-transmission
  ([sigrok OLS wiki](https://sigrok.org/wiki/Openbench_Logic_Sniffer/Info)). **This project
  deliberately diverges from that minimal-framing precedent** by including a sequence number and
  timestamp in every streamed record (§4.1) — the reference devices above are single-purpose
  logic analyzers with one stream and one host driver revision to keep in sync; this device
  multiplexes waveform and GPIO data on one endpoint (§4.1) and wants overrun/gap detection to be
  a first-class, explicit signal (§4.1's `0x7F` overrun record) rather than relying on an
  inter-byte timeout heuristic — a deliberate, reasoned departure, not an oversight.
- **Command channel vs stream channel coexistence — confirmed pattern.** fx2lafw uses **EP0
  control transfers** for commands (START, GET_FW_VERSION, etc. via `libusb_control_transfer()`)
  and **EP2 bulk IN exclusively** for sample data — a clean control-plane/data-plane split with
  zero interleaving risk. Source: [libsigrok fx2lafw/protocol.c](https://github.com/sigrokproject/libsigrok/blob/master/src/hardware/fx2lafw/protocol.c).
  This project's §3/§4 split (a bulk OUT/IN *pair* for commands, since SCPI responses can be
  larger/more varied than fx2lafw's simple control-transfer replies, rather than EP0 control
  transfers) plus a dedicated bulk IN for streaming is the same principle applied with a richer
  command channel. USBTMC-USB488's separate interrupt-IN-for-SRQ (§2.3) is the standards-body
  version of the same idea. This is the model recommended in §4 below.
- **PicoScope precedent (partial):** Pico Technology's documented "(fast) streaming mode" API
  (distinct from block-capture mode) has the device sample continuously into a circular
  "overview buffer" that the host polls/copies from, with overrun explicitly signalled (a
  sentinel value and an explicit buffer-status query) rather than silently dropped — a real-world
  precedent for this doc's explicit overrun-notice record (§4.1's `0x7F`). Source:
  [PS2000 Programmer's Guide](https://www.picotech.com/download/manuals/ps2000pg.en-10.pdf),
  [Pico forum — overrun signalling](https://www.picotech.com/support/viewtopic.php?t=11641).
  Which USB transfer type PicoScope firmware actually uses at the wire level could not be
  confirmed from available documentation (the programmer's guides describe the host buffer/API
  model, not endpoint configuration) — noted as a gap, not assumed to be bulk by extension of the
  general pattern above.

### 2.5 Large-transfer patterns from bench-scope vendors (Keysight/Tektronix/Rigol)

**This section's central finding, stated up front: real bench-scope vendors' "large transfer"
patterns are sized for USB speed classes roughly 40x (USB 2.0 High-Speed, 480 Mbit/s) to over 400x
(USB 3.0 SuperSpeed, 5 Gbit/s) faster than this device's USB peripheral, so their patterns are
useful for the *idiom* (self-framed block headers, pre-transfer size negotiation, batched requests
for deep memory) but not for the *numbers* (chunk sizes, point counts per request). §4.3 below
works out what those numbers should actually be for this hardware.**

**USB speed-class confirmation — the gap is real, not assumed.** This device's MCU (STM32F103)
exposes only a **USB Full-Speed (12 Mbit/s) peripheral** — confirmed directly from
`vendors/hantek/2d82/ghidra/STM32F103.svd`, which names the peripheral block `USB_OTG_FS` / `OTG_FS_DEVICE`, described
in-file as *"USB on the go full speed"*; there is no High-Speed PHY on this silicon, matching this
doc's §1 claim. By contrast:

- **Keysight InfiniiVision 3000 X-Series**: the Programmer's Guide's "Using the USB (Device)
  Interface" setup section states plainly: *"Connect a USB cable from the controller PC's USB port
  to the 'USB DEVICE' port on the back of the oscilloscope. This is a USB 2.0 high-speed port."*
  Source: [Keysight InfiniiVision 3000 X-Series Programmer's
  Guide](https://www.batronix.com/files/Keysight/Oszilloskope/3000XT/3000XT-Programming.pdf)
  (Batronix-hosted mirror of Keysight document 9018-06894; Keysight's own site gates the PDF
  behind a marketing-contact-info form, so a public mirror is cited instead — content verified
  against the gated landing page's own metadata, same publication number/date). **High-Speed is
  480 Mbit/s — 40x this device's Full-Speed 12 Mbit/s ceiling.**
- **Tektronix 4/5/6 Series MSO**: explicitly a USBTMC device (*"This oscilloscope is a USB-TMC
  device… install the 'USB Test and Measurement Device (IVI)' driver"*) with *"both USB 2.0 and
  3.0 host ports on the instrument"* for peripherals; the device port itself is documented
  USB 2.0-class hardware in the same family. Source: [4/5/6 Series MSO Programmer
  Interface Manual](https://download.tek.com/manual/4-5-6Series-MSO-Programmer-Interface-Manual_EN-US_077130508.pdf)
  (verified genuine Tektronix `download.tek.com` PDF, 2075 pages, publication 077-1305-08).
  Higher-end Tektronix models (4/5/6 Series, DPO70000 class) commonly ship **USB 3.0 (5 Gbit/s)**
  device connectivity — over **400x** this device's Full-Speed ceiling on those models.
- **Rigol MSO1000Z/DS1000Z**: documented as a USB-TMC device requiring the "USB Test and
  Measurement Device (IVI)" driver, same class-driver model as Tektronix/Keysight. Source:
  [MSO1000Z/DS1000Z Programming Guide](https://www.batronix.com/files/Rigol/Oszilloskope/_DS%26MSO1000Z/MSO_DS1000Z_ProgrammingGuide_EN.pdf)
  (Batronix-hosted mirror; exact device-port USB speed class not stated in the excerpted text
  reachable from this guide — Rigol's entry/mid-tier scopes are widely reported elsewhere as
  USB 2.0 High-Speed, but that specific claim is not re-cited here since it wasn't found in the
  primary document itself; treat as a gap, not asserted).

**Conclusion on speed class: confirmed and significant.** Every vendor checked in this pass that
states a device-port USB speed explicitly states High-Speed (480 Mbit/s) or faster — a full order
of magnitude (or two) above this device's Full-Speed 12 Mbit/s ceiling. This is not a minor detail
to footnote; it means any "how many bytes per transfer" or "how many points per request" number
documented by these vendors is answering a different physics question than the one this project
needs answered, and importing those numbers directly (rather than the underlying *idiom*) would be
a design error. §4.3 works the numbers for *this* device's actual ceiling.

**Keysight's `:WAVeform:DATA?` block-transfer format — read directly from the Programmer's Guide,
not a summary.** Source for this whole subsection: [Keysight InfiniiVision 3000 X-Series
Oscilloscopes Programmer's Guide](https://www.batronix.com/files/Keysight/Oszilloskope/3000XT/3000XT-Programming.pdf),
publication 9018-06894, verified genuine (title page, table of contents, and full command
reference all present and internally consistent across 1000+ pages; cross-checked against the
gated official landing page's own metadata for the same publication number).

- **Format**: standard IEEE 488.2 definite-length arbitrary block: `#<n><n digits of decimal
  length><data>`, e.g. for 1000 bytes: `#800001000<1000 bytes><NL>` — the guide's own worked
  example (`8` = "8 length-digits follow", `00001000` = 1000 bytes). This is the exact `#<n><length
  ><data>` convention this task description named, confirmed as Keysight's real, current,
  documented wire format — not an inference.
- **Pre-transfer size negotiation — real and exactly the pattern this task asked to check for.**
  The documented workflow is: set `:WAVeform:SOURce`, `:WAVeform:FORMat`, then **`:WAVeform:POINts
  <n>`** *before* `:WAVeform:DATA?` — the guide states plainly that "you must specify the
  :WAVeform parameters for the SOURce channel, the FORMat type, and the number of POINts *prior
  to* sending the :WAVeform:DATA? query." `:WAVeform:POINts?` (query form) lets the host read back
  how many points it will actually get (the instrument may cap/round the requested count).
  `:WAVeform:PREamble?` separately returns X/Y scale-and-origin metadata needed to interpret the
  raw bytes — i.e. Keysight's protocol has the host ask "how much data, and how do I interpret it"
  *before* asking "give me the data," letting the host precisely size a receive buffer ahead of
  the actual bulk transfer. This is a genuinely reusable idiom regardless of speed class.
- **Documented max single-request size**: with `:WAVeform:POINts MAXimum` and the instrument
  stopped, the guide states this "can be as many as 4,000,000 [points] in some operating modes or
  as many as 8,000,000 for a digital channel on the mixed signal oscilloscope" — multi-megabyte
  single-shot transfers, requested and returned via one `:WAVeform:DATA?` call. No explicit
  vendor-documented wire-level fragmentation guidance is given in the Programmer's Guide itself
  (VISA/USBTMC — §2.3 above — handles chunking transparently below the SCPI layer; the guide never
  discusses `wMaxPacketSize` or per-transfer byte limits, because at High-Speed USBTMC's normal
  request/response model, a multi-megabyte transfer is "just" a very long single bulk-IN read from
  the host's point of view, per §2.3's fragmentation analysis above).
- **Abort mechanism**: the guide repeatedly documents `myScope.IO.Clear` (VISA COM's Device Clear)
  as the way to "halt a `:DIGitize` in progress" or recover a stuck transfer — used in dozens of
  the guide's own example code snippets, essentially the standard "get back to a known state"
  idiom threaded through the whole document. See §2.3 above for how this relates to USBTMC's
  finer-grained `INITIATE_ABORT_BULK_IN/OUT`.

**Real practical numbers**: no explicit "N MB/s achieved" throughput figure is stated in the
Keysight guide itself (expected — VISA/driver/OS-dependent, same caveat this doc's §2.4 already
gives for the FS-bulk case). The concrete, vendor-stated number that matters most for sizing is
the **4-8 million point maximum single transfer** at up to 2 bytes/point (WORD format) — i.e.
multi-megabyte to low-tens-of-megabyte single logical transfers are Keysight's normal deep-memory
case, feasible specifically *because* the underlying USB link is 40x+ faster than this device's.

**Rigol's documented multi-batch pattern — a genuinely different and more directly relevant
idiom.** Source: [MSO1000Z/DS1000Z Programming Guide](https://www.batronix.com/files/Rigol/Oszilloskope/_DS%26MSO1000Z/MSO_DS1000Z_ProgrammingGuide_EN.pdf),
`:WAVeform:DATA?` command reference (page 2-219 onward). Rigol's block header is the same IEEE
488.2 idiom with a fixed digit count instead of Keysight's variable one: `#9<9-digit decimal
length><data>` (`#` denoter, literal `9`, always exactly 9 length-digits — a minor but real format
variant from Keysight's `#8<8 digits>`). More importantly, **Rigol's guide documents a hard,
format-dependent per-request point cap and an explicit multi-batch reading procedure for anything
beyond it** — this is the closest published precedent to what this project's Full-Speed device
actually needs:

| `:WAVeform:FORMat` | Max points per single `:WAV:DATA?` call |
|---|---|
| BYTE | 250,000 |
| WORD | 125,000 |
| ASCii | 15,625 |

When the oscilloscope's memory depth exceeds that per-call cap, Rigol's guide gives a worked
3-batch example (300,000 WORD-format points, 125,000-point cap → batches of 1-125000,
125001-250000, 250001-300000 via `:WAVeform:STARt`/`:WAVeform:STOP` windowing before each
`:WAV:DATA?` call), explicitly instructing the host to concatenate the batches in sequence
afterward. **This is precedent, from a real, current, shipping bench-scope product line, for
exactly the "size-capped request + explicit multi-batch host-driven pagination" pattern** — as
opposed to Keysight's "one call, up to 8M points, let USBTMC/VISA handle the rest" approach. Both
are real, current SCPI-idiom implementations; Rigol's is the one whose *shape* (bounded
per-request size, host explicitly pages through a deep buffer) transfers far more directly to a
Full-Speed, small-buffer MCU than Keysight's "just ask for everything" pattern does.

**Tektronix — same idiom, one real syntax variant.** Source: [4/5/6 Series MSO Programmer
Interface Manual](https://download.tek.com/manual/4-5-6Series-MSO-Programmer-Interface-Manual_EN-US_077130508.pdf),
`CURVe?`/`WFMOutpre?` command reference and Appendix D worked examples. Tektronix's `CURVe?` query
returns `#<x><yyy><data><newline>` — same block idiom, but **`<x>` (the length-of-length field) is
documented as hexadecimal**, not decimal like Keysight/Rigol's `#8`/`#9` (the guide's own note:
"`<x>` is hexadecimal format. The letters A-F denote several y bytes between 10 and 15 digits.") —
a real, if minor, third variant of the same convention, confirming the task's expectation that
"very standard SCPI idiom, likely yes [same across vendors]" is correct at the block-header level
but not perfectly uniform in every digit-count detail. Pre-transfer size negotiation is via
`DATa:STARt`/`DATa:STOP` (point-range window, same idea as Rigol's `:WAV:STARt`/`:WAV:STOP`) and
`WFMOutpre:NR_Pt?` (points-that-will-be-returned query) — Appendix D's worked example explicitly
sets a 10,000-point window (`:DATa:STARt 1` / `:DATa:STOP 10000`) before calling `:CURVe?`, which
then returns `:CURVe #510000<10,000 binary bytes>` — the `5` there is "5 length-digits follow,"
`10000` is the byte count, matching the general pattern exactly. Abort is not surveyed in equal
depth for Tektronix in this pass (time-boxed per the task's "quick comparison is enough" scope for
Tek/Rigol) — no evidence found or claimed that it differs materially from the Device-Clear /
USBTMC-abort mechanisms already covered in §2.3.

**Summary comparison table:**

| Vendor/line | Block header | Pre-size query | Per-request cap? | Documented device-port speed |
|---|---|---|---|---|
| Keysight InfiniiVision 3000 X | `#8<8 digits>` | `:WAVeform:POINts?` (informational, not a hard cap) | No — up to 4-8M points in one call | USB 2.0 High-Speed (480 Mbit/s), explicit |
| Tektronix 4/5/6 Series MSO | `#<hex digit><n digits>` | `WFMOutpre:NR_Pt?` / `DATa:STARt`/`STOP` window | No hard cap found in this pass's excerpt | USB 2.0/3.0 class hardware, explicit (USBTMC) |
| Rigol MSO1000Z/DS1000Z | `#9<9 digits>` | `:WAVeform:POINts?` + explicit format-dependent cap table | **Yes — 250k(BYTE)/125k(WORD)/15.6k(ASCII) points/call, host must batch beyond that** | Not stated in excerpt (USBTMC, elsewhere reported High-Speed) |
| This device (proposed) | see §4.3 | n/a — streaming push, not pull-per-request | n/a by design (continuous stream, not request-capped pull) | **USB Full-Speed (12 Mbit/s) only — confirmed from SVD, no High-Speed PHY** |

---

## 3. Command/control channel design

**Recommendation: a SCPI-flavoured ASCII text command language, carried over a bulk OUT/IN pair
using small self-framed records (a "USBTMC-lite" framing), *and* mirrored on the CDC-ACM shell
(§5) as plain human-typeable text.**

Reasoning:

- SCPI syntax itself is free to adopt (public standard, no licensing issue) and gets us most of
  the "Fluke-like" goal for free: `*IDN?`, hierarchical commands, `?`-suffixed queries, a
  `SYSTem:ERRor?` queue instead of silent failure. It is also directly human-typeable over the
  CDC-ACM shell with no separate "debug command set" needed — one command grammar for both the
  machine channel and the interactive shell, which halves the surface to design/test/document.
- Full USBTMC is not adopted (§2.3) — instead, each command/response is wrapped in a minimal
  fixed 8-byte header modelled on USBTMC's `bTag`/length/end-of-message fields:

  ```
  Host -> Device (bulk OUT):
  [0]     0xA5              sync byte (cheap desync detection)
  [1]     tag               request tag, host-incremented, echoed in response
  [2:4]   length (LE u16)   ASCII command length, not including this header
  [4]     flags             bit0: EOM (more OUT packets follow if 0)
  [5:8]   reserved (0x00)
  [8:]    ASCII command text, e.g. "MEAS:VOLT:DC? CH1\n"

  Device -> Host (bulk IN):
  [0]     0xA6              sync byte (device->host, distinct value from OUT's 0xA5)
  [1]     tag               echoes request tag
  [2:4]   length (LE u16)   ASCII response length
  [4]     status            0=OK, 1=error (error text follows as the payload), 2=continuation
  [5:8]   reserved (0x00)
  [8:]    ASCII response text
  ```

  This is deliberately close to "USBTMC's good ideas, minus the descriptor/subclass machinery" —
  see §2.3. Long commands/responses (rare — waveform data does NOT ride this channel, see §4) use
  the `flags`/`status` continuation bits the same way USBTMC's `EOM` bit works.
- Errors are queryable (`SYST:ERR?` drains a FIFO of pending errors, IEEE-488.2 style) rather than
  encoded as magic byte patterns the host must special-case — directly replacing the vendor
  protocol's "not ready" sentinel (`00 00 ff ff 00 00 ff ff 00 00 ff ff`, see `usb-protocol.md`
  §"Waveform Data") with an explicit, self-describing mechanism.
- Command *coverage* (which SCPI subtree names map to which scope/DMM/AWG parameter) is
  deliberately **out of scope for this document** — that's a large design surface of its own
  (every command in `usb-protocol.md`'s tables needs a new mnemonic) and belongs in a follow-up
  doc once this framing is agreed. §7 tracks this as an open question.

**Why not just reuse the vendor protocol's 10-byte binary format for the command channel?** It
fails the "Fluke-like" and "self-describing" goals outright: fixed opcodes with no capability
query, no error path (see `usb-protocol.md`'s cmd `0x17` — a byte-for-byte no-op with zero
indication to the caller), payload meaning that changes based on an undocumented bank byte, and
nothing a human could type by hand. It was reasonable for a vendor GUI that ships a matched DLL,
but is the opposite of the goal here.

---

## 4. Streaming channel design

**Recommendation: one dedicated bulk IN endpoint for outbound streaming, shared by waveform
sample data and GPIO state-change data (framed distinctly per §4.1), separate from the command
bulk pair in §3. Stream lifecycle (start/stop/error) is controlled *by* SCPI-style commands on
the control channel, not by a separate protocol.**

### 4.1 Why one streaming endpoint, not two

STM32F103's USB peripheral has a limited number of endpoint buffer slots (8 bidirectional
endpoints total including EP0). Between the control pair (§3), a streaming IN endpoint, and the
CDC-ACM port's 3 endpoints (§5), endpoint budget is a real constraint on this MCU — see §5.2. A
single streaming endpoint, multiplexed by a 1-byte record-type tag in each record's header, keeps
the endpoint count down and matches the "small self-framed records" pattern from §2.4: GPIO
watch data and waveform data are both bursty, moderate-rate sample streams: they don't need
physically separate pipes, they need to be distinguishable in the same pipe.

Record format (fits in a single 64-byte FS bulk packet for the common case):

```
[0]     record type       0x01=waveform sample block, 0x02=GPIO snapshot, 0x03=GPIO delta,
                           0x7F=stream control (started/stopped/overrun notice)
[1]     stream id         which active stream this record belongs to (host allocates on start,
                           see §3 command channel — supports >1 concurrent stream, e.g. waveform
                           + GPIO watch running together)
[2:4]   sequence (LE u16) monotonically increasing per stream id, wraps; host detects drops by
                           gap in sequence, does NOT need to ACK each record
[4:8]   timestamp (LE u32) microsecond tick, free-running device clock — lets the host correlate
                           waveform samples and GPIO transitions to real time, and to each other
[8:]    payload            up to 56 bytes, meaning depends on record type (§4.2 waveform,
                           §6.3 GPIO)
```

- **No per-record ACK.** The host does not send a bulk-OUT reply per streamed record — that would
  reintroduce the vendor protocol's request-per-chunk bottleneck this design exists to remove
  (§2.4's throughput analysis: firmware polling cadence, not USB bandwidth, was the old
  bottleneck). The device just keeps writing to the bulk IN endpoint; USB's own bulk retry
  handles wire-level reliability, and the sequence number lets the host detect firmware-side
  buffer overruns (if the device ever has to drop a record because the host isn't draining fast
  enough, it emits a `0x7F` overrun-notice record with the count of dropped records, then resumes
  — an explicit, honest "some data was lost, here's how much" mechanism instead of silent gaps).
- **Stream lifecycle rides the control channel.** `STREam:WAVeform:STARt?` (returns an allocated
  stream id), `STREam:GPIO:WATCh PA0,PC15` (§6.4), `STREam:STOP <id>`, `STREam:LIST?` are SCPI-
  style commands on the §3 channel; the streaming endpoint only ever carries data going the other
  direction. This mirrors USBTMC's interrupt-IN-for-notify / bulk-for-data split conceptually,
  but folds "notify" into the same bulk stream as a tagged record type (`0x7F`) rather than a
  fourth endpoint, again for STM32F103 endpoint-budget reasons.
- **Abort/mid-stream-change is control-plane, mirroring USBTMC's `INITIATE_ABORT_BULK_IN` idiom
  (§2.3) rather than requiring an in-band stop marker.** `STREam:STOP <id>` (already listed above)
  *is* this device's abort mechanism — sent on the control bulk pair (§3), a logically separate
  pipe from the streaming bulk IN endpoint, exactly matching USBTMC's rationale for putting abort
  on the control endpoint rather than the data pipe (§2.3: an in-flight transfer can be cancelled
  out-of-band without needing the data pipe free or synchronized, and without the device having to
  parse a stop command out of the same stream it's mid-transfer on). This was implicit in the
  original §4.1 design (stream lifecycle already "rides the control channel," above) but is called
  out explicitly here because this pass's research confirmed it is the *standards-precedented*
  choice, not just a convenient one — Keysight's own abort idiom (`myScope.IO.Clear`, §2.5) and
  USBTMC's `INITIATE_ABORT_BULK_IN`/`OUT` (§2.3) both land on the same control-plane-not-data-plane
  principle. One addition this pass recommends: `STREam:STOP <id>` should be defined as
  taking effect on a **record boundary**, not a byte boundary (the streaming endpoint's 64-byte
  records, §4.3, are never split mid-record across packets, so this is free) — the device stops
  emitting new records for that stream id promptly, with no further protocol needed to determine
  "did the stop take effect," unlike USBTMC's asynchronous abort which needs the
  `CHECK_ABORT_BULK_IN_STATUS` poll specifically because a partially-flushed message can leave
  ambiguous state. This device's design avoids that ambiguity by construction (record-atomic
  framing) rather than needing a status-check round-trip — a concrete simplification this project
  gets for free from the small-record design that a message-oriented protocol like USBTMC does
  not.

### 4.2 Waveform streaming specifically

Replaces `usb-protocol.md`'s cmd `0x16`/`0x17` pull loop. Once a capture is armed and triggers,
the device pushes `0x01` waveform-sample-block records continuously (bounded by the capture
depth) instead of waiting for a host poll per 64 bytes. The payload sub-layout (channel id, ADC
encoding, sample packing) is intentionally **not fixed by this document** — it depends on
decisions the new firmware hasn't made yet (final ADC resolution/format, one vs two channel
interleave, whether raw FPGA samples or MCU-resampled data are exposed) and is tracked as an open
question in §7 rather than guessed here.

### 4.3 Reconciling small-packet streaming with large-capture transfer — the chunk-size question

§1's goal 3 states "small packets are a feature, not a limitation." §2.5's vendor survey found the
opposite-looking pattern at the high end: Keysight requesting up to 8,000,000 points in one
`:WAVeform:DATA?` call. These look like they're in tension. **They are not — they're answers to
different questions, and working through why resolves the "what chunk size should this device
actually use" question concretely, not just qualitatively.**

**The USB 2.0 spec itself already answers the low-level part of this question.** A Full-Speed bulk
endpoint's `wMaxPacketSize` is capped at **64 bytes** (USB 2.0 spec §5.8.3; also the value this
project's own existing vendor-protocol RE work independently confirms — `usb-protocol.md`
documents the *existing* Hantek 2D82AUTO firmware already moving all bulk data, including waveform
readback, in 64-byte packets over this same Full-Speed peripheral, "104 packets of 64 bytes" for a
6672-byte waveform pull, and the FPGA-bitstream write path likewise chunking at 64 bytes). This
isn't a design choice this project is making — it's the hardware's ceiling. A Keysight scope on
USB 2.0 High-Speed has a 512-byte `wMaxPacketSize` ceiling instead (8x this device's), and at
USB 3.0 SuperSpeed bulk endpoints go to 1024 bytes with a fundamentally different transfer
protocol (bursting) besides — neither of those ceilings, nor the "just request 8,000,000 points
and let the driver stack sort out the packetization" pattern that a High-Speed/SuperSpeed link
makes practical, is available to this device's silicon.

**Given that hard 64-byte ceiling, is Keysight-style "one giant block read" achievable at all on
this hardware?** Technically yes — nothing stops firmware from answering a hypothetical
`WAVeform:DATA?`-alike command with a single logical multi-kilobyte or multi-megabyte block reply,
internally chunked into a long run of 64-byte bulk-IN packets exactly the way §2.3 showed USBTMC
itself does it (one header packet, then raw unframed 64-byte packets until a byte count is
satisfied). But this would be **working against this device's actual constraints, not with them**,
for two concrete reasons specific to this hardware (not general USB theory):

1. **A single long unframed block transfer forfeits per-packet drop/resume information that this
   device's small-record design gets for free.** §2.3's USBTMC trace showed the header (with the
   total length) only appears on the *first* packet of a `DEV_DEP_MSG_IN` response — if packet 400
   of 800 in that stream is lost/NAK'd and needs host-driver-level retry logic, or if the host
   wants to know "how far did this get" after an error, the raw mid-stream packets carry zero
   self-describing information; the host can only count bytes received. §4.1's existing per-record
   design (sequence number + timestamp in *every* 64-byte record, not just the first) already
   solves this better — every single packet is independently a complete, self-describing unit.
   Adopting a Keysight-style single-header-then-raw-bytes scheme would be a strict regression from
   what §4.1 already specifies, not an improvement borrowed from more mature vendors.
2. **At Full-Speed's ~1 ms microframe/frame-scheduling granularity (vs. High-Speed's 125 μs
   microframes), the per-packet framing overhead that matters at 480 Mbit/s is proportionally
   much less significant at 12 Mbit/s.** A 64-byte payload plus this design's 8-byte record header
   (§4.1) is 12.5% header overhead per packet — worse than USBTMC's ~12-byte header amortized over
   a multi-KB message (well under 1%), but the absolute bandwidth this device needs to sustain is
   itself far lower (waveform capture depth and GPIO event rates on an STM32F103-class instrument,
   not a multi-GHz-class deep-memory bench scope) — the percentage framing tax is a non-issue in
   absolute terms at this device's actual data rates, while the self-description benefit (point 1)
   is a real, continuous win given this design's push/streaming (not discrete-pull) model.

**Recommendation — reconciled, concrete:** *keep* §4.1's existing 64-byte, fully-self-framed
per-record design (8-byte header + up to 56-byte payload) as the wire-level chunk size for
streaming, including large waveform captures — do **not** adopt a Keysight-style large
single-block-with-one-header scheme. This is the single most load-bearing conclusion of this
research pass, so it's stated as a firm recommendation rather than an open question:

- **64 bytes (one FS bulk max-packet) is the right unit, and it is not a compromise vs. the vendor
  research — it is what USB Full-Speed hardware makes the natural unit, and this device's
  streaming/push model (§1 goal 2) benefits from every packet being self-describing in a way a
  discrete-pull deep-memory scope protocol does not need to be.**
- A large waveform capture (say, a deep-memory FPGA acquisition of tens or hundreds of thousands
  of samples — this device's realistic ceiling, nowhere near Keysight's 4-8M-point deep-memory
  tier) is transferred as a long **run of `0x01` records**, each independently sequenced and
  timestamped per §4.1, not as one giant block-header'd blob. The host reassembles by
  concatenating payloads in sequence order — conceptually identical to what a USBTMC host driver
  already does when reading a multi-packet `DEV_DEP_MSG_IN` response (§2.3), except every packet
  carries reassembly metadata instead of only the first.
- **Rigol's capped-request-plus-explicit-batching pattern (§2.5) is the vendor precedent whose
  shape is actually relevant here, not Keysight's "just ask for everything."** This device's
  design already has an equivalent, and arguably a stronger one: instead of a host-driven
  fixed-size-batch loop (Rigol's `:WAV:STARt`/`:WAV:STOP` windowing, repeated per batch), this
  device's stream-until-told-to-stop model (§4.1) means the *device* paces the transfer naturally
  as records are produced, with no host-side batch-loop bookkeeping needed at all — the host just
  drains the endpoint until the capture-complete `0x7F` record arrives. Rigol's pattern is the
  closer relative in spirit (bounded-size units, not one unbounded block) even though this
  design's mechanism (continuous push) differs from Rigol's (repeated discrete pull-with-window).
- **No change to §4.1's record layout is needed as a result of this research pass** — the existing
  64-byte self-framed record was already the right call before this research, and the Keysight/
  Tek/Rigol comparison in §2.5 confirms rather than overturns it, once the speed-class gap (§2.5)
  is accounted for. What this section adds is the explicit reasoning for *why* it's right, not
  just an assertion that it is — future readers of this doc revisiting the chunk-size choice should
  start here rather than re-deriving it.

---

## 5. USB CDC-ACM shell design

### 5.1 Goal and existing groundwork

Add a USB virtual serial port so the same Zephyr shell already working over UART1 (`prj.conf`:
`CONFIG_SHELL=y`, `CONFIG_SHELL_BACKEND_SERIAL=y`; working example command in
[`code/a2k_board/src/backlight.c`](../../code/a2k_board/src/backlight.c), registered via
`SHELL_CMD_REGISTER(backlight, ...)`) is *also* reachable as a `/dev/ttyACM0`-style device over
USB — **in addition to**, not instead of, the physical UART1 shell. This is already anticipated
in [`driver-design.md`](driver-design.md)'s boot sequence (`usb_cdc_init()`, called late/
non-blocking, right before `usb_scope_init()`) and in [`reimplementation-plan.md`](
../reimplementation-plan.md)'s peripheral list ("USB-C (DFU + CDC)") — this section fills in the
detail those docs left as a placeholder.

### 5.2 Composite device shape

The device needs, on one physical USB-C connector, all of:

| Interface | Endpoints | Purpose |
|---|---|---|
| CDC-ACM (2 interfaces: Comm + Data, per CDC spec) | 1 interrupt IN (notify) + 1 bulk IN + 1 bulk OUT | Shell TTY |
| Vendor control (§3) | 1 bulk IN + 1 bulk OUT | SCPI-lite command/response |
| Vendor streaming (§4) | 1 bulk IN | Waveform + GPIO streaming |

That's **3 interfaces, 6 endpoint directions** (5 unique, since CDC's 2 bulk + control's 2 bulk
are 4 distinct pipes, plus CDC's interrupt IN, plus the streaming IN = 3 IN + 2 OUT + 1 interrupt
IN = 4 IN total, 2 OUT total) plus EP0 control. STM32F103's USB peripheral has **8 endpoint
buffer slots total (including EP0)** — this fits with one slot to spare, but leaves no headroom
for a future second streaming pipe without either dropping something or time-multiplexing further
on the existing streaming endpoint (which §4.1 already does by design). This constraint is the
direct reason §4.1 recommends one shared streaming endpoint rather than two.

A **composite USB device** in this shape needs an **Interface Association Descriptor (IAD)**
grouping the CDC-ACM Comm+Data interface pair so host OSes (particularly Windows) recognise them
as one logical CDC function rather than two unrelated interfaces — this is standard CDC-ACM
composite practice, not specific to Zephyr, and is why CDC-ACM is usually the "well-behaved
citizen" interface in a composite descriptor: it has an established IAD convention other class
drivers (including custom vendor interfaces) don't need but also don't conflict with.

### 5.3 Zephyr Kconfig / devicetree shape

Two viable paths exist in current (west-tracked `main`) Zephyr; recommend the first as primary
since it matches what `prj.conf` already declares (`CONFIG_USB_DEVICE_STACK=n` today, i.e. the
legacy stack is the one already wired into this project's Kconfig surface, just disabled for
first-stage bring-up):

**Path A — legacy USB device stack (`CONFIG_USB_DEVICE_STACK`), recommended as primary:**

```
CONFIG_USB_DEVICE_STACK=y
CONFIG_USB_CDC_ACM=y

# Second shell backend instance, on top of the existing UART one:
CONFIG_SHELL_BACKEND_SERIAL=y        # already set — keep for UART1
# The CDC-ACM UART emulation (drivers/serial, uart_cdc_acm) is itself treated
# as a Zephyr UART device, so a *second* shell backend instance binds to it —
# Zephyr's shell subsystem supports multiple shell instances on separate
# backends simultaneously (this is the standard "shell over USB CDC-ACM,
# console over physical UART" pattern used by several in-tree Zephyr samples,
# e.g. samples/subsys/usb/cdc_acm and boards that define both zephyr,console
# and a USB CDC shell).
CONFIG_UART_LINE_CTRL=y              # DTR/RTS so host tools (tio, minicom) can
                                      # signal connect state cleanly
CONFIG_USB_CDC_ACM_RINGBUF_SIZE=1024 # or similar — buffer typed input while the
                                      # USB stack is between polls
```

Devicetree: add a `zephyr_cdc_acm_uart` (or equivalent, exact node name is Zephyr-version
dependent — verify against the checked-out Zephyr revision at implementation time) chosen/child
node under the `&usb` device node in `hantek_2d82.dts` (alongside the existing `&usb { status =
"okay"; }` stanza already present, see file), analogous to:

```dts
&usb {
	status = "okay";

	cdc_acm_uart0: cdc_acm_uart0 {
		compatible = "zephyr,cdc-acm-uart";
		label = "CDC_ACM_0";
	};
};
```

Then register a **second shell instance** on this UART device — Zephyr's `SHELL_DEFINE` /
`SHELL_UART_DEFINE`-style macros support multiple simultaneous shell backends. The `chosen`
`zephyr,shell-uart` node stays pointed at `&usart1` (physical UART) — do **not** repoint it at the
CDC-ACM UART, since that would replace rather than add. The CDC-ACM shell instance needs its own
explicit backend registration rather than relying on the `chosen` shell-uart mechanism (which
only wires up *one* default shell instance).

**Path B — new USBD "next" stack (`CONFIG_USB_DEVICE_STACK_NEXT` and the `usbd_*` API),
noted as forward-looking alternative:** Zephyr's newer USB device stack (built around explicit
`usbd_init()`/class-instance registration rather than the older static-Kconfig class-driver model)
is where upstream Zephyr has been directing new development, with the legacy stack in
maintenance/deprecation mode over time. Since this project tracks Zephyr `main` (`west.yml`:
`revision: main`), the legacy-vs-next tradeoff should be re-checked against the actual checked-out
Zephyr revision at implementation time rather than assumed from this doc — by the time this
feature is built, "legacy" Kconfig names or the recommended class registration API may have moved.
This doc's design (composite device, 3 logical functions, one shared streaming endpoint) is stack-
API-agnostic; only the exact Kconfig symbol names and devicetree binding names in this section are
stack-version-sensitive.

### 5.4 Known gotchas

- **Composite descriptor ordering/IAD requirements**: Windows in particular is picky about IAD
  placement when mixing a standard class (CDC-ACM) with a vendor-defined class in one composite
  descriptor — the IAD must immediately precede the CDC Comm interface, and interface numbers
  must be contiguous per function. Get this wrong and Windows may enumerate the device with
  driver errors on the CDC function specifically (the vendor bulk interfaces, having no class
  driver to match anyway, are less sensitive to this since they're WinUSB/libusb-driven regardless).
- **Enumeration is slow relative to boot**: already known and already designed around in
  `driver-design.md` (`usb_cdc_init()` explicitly called late, non-blocking, boot must not stall
  on USB enumeration — 200-500ms estimate cited there). No change needed, just confirming this
  design doesn't violate that constraint — the composite device enumerates as one unit, so the
  vendor interfaces come up at the same time as CDC-ACM, not separately/later.
- **Endpoint budget is tight** (§5.2) — 8 total slots on STM32F103, this design uses 7. Any future
  additional USB function (e.g. a second streaming pipe, or DFU-in-application via USB rather
  than the existing serial-bootloader DFU path) needs to either reuse an existing pipe or free one
  up; there is no slack for "just add another endpoint" without re-examining this budget.
- **Two shell instances means two independent line-editing/history states** — a user typing in
  the UART1 `tio` session and a user connected via USB CDC-ACM `screen`/`tio`/`minicom`
  simultaneously get independent shell prompts, not a shared session. This is expected/desirable
  (matches "alongside, not replacing") but should be documented in-product (e.g. `dmesg`/banner
  text distinguishing "USB shell" vs "UART shell") so a user with both connected isn't confused
  about which one echoed what.
- **CDC-ACM is not a real UART** — DTR/RTS line-control state (`CONFIG_UART_LINE_CTRL`) reflects
  whether a host application has the port *open*, not whether a physical cable is plugged in
  (VBUS/enumeration state is separate — this device already tracks physical USB presence via
  `PC2`=`USB_VBUS_SENSE`, see [`gpio-pinout.md`](gpio-pinout.md)). Shell backends
  that gate output on DTR (common pattern, avoids writing into a closed terminal's buffer) need
  both checks: VBUS present AND DTR asserted, not just one.
- **GH issue #32** ("Redirect calibration debug output from USART1 to USB CDC") is an existing,
  open, related ask — it wants *log* output moved to CDC-ACM, which is a narrower thing than
  "add a shell" (per `driver-design.md`'s existing log-backend-strategy section: prod builds
  already plan a RAM ring buffer flushed to the USB CDC-ACM shell via a `log backend` command).
  This design's CDC-ACM shell is exactly the transport issue #32 needs; implementing this section
  resolves the transport half of that issue's ask (the log-backend-selection command itself is
  separate follow-up work, not redesigned here).

---

## 6. GPIO state-streaming protocol design

### 6.1 Existing groundwork — do not duplicate

`code/diag_gpio/src/gpio_mon.c` (currently untracked, a
standalone `diag_gpio` Zephyr app) **already implements** a substantial, working piece of this:

- A full STM32F103VET6 pin table (`s_pins[]`, ports A-G) with a per-pin status flag (`V`=verified/
  `?`=unverified/`X`=explicitly unused/`S`=system — never reconfigure SWD/OSC pins), cross-
  referenced against `ghidra_names_2d82.tsv`, `hantek_2d82.dts`, and `gpio-pinout.md`.
- Named-pin lookup (`find_pin("PA0")`-style, case-insensitive) — i.e. the pin-selection mechanism
  this task asks to design **already exists and already uses the project's own `PXn` naming
  convention** from `gpio-pinout.md`. No new naming scheme needed.
- Per-pin mode control (`gpio_mon mode <PIN> <mode>` — analog/input/input-pu/input-pd/output-low/
  output-high/int-both/int-rise/int-fall) and direct read/set (`gpio_mon read <PIN>`,
  `gpio_mon set <PIN> <0|1>`).
- **Edge-triggered change detection already implemented**, currently logged rather than streamed:
  `gpio_mon_poll()` walks monitored pins and `LOG_INF`s on any value change; `gpio_mon_poll_hyst()`
  is a debounced variant with per-pin configurable stable-sample-count thresholds
  (`gpio_mon hyst <PIN> <mode> <lo_cnt> <hi_cnt>`) for noisy/slow signals (explicitly designed for
  cases like a battery-voltage comparator, per its own doc comment).
- This is a **polling** implementation today (`gpio_mon_poll()` is called from somewhere in a
  loop, not from an EXTI ISR) — it detects changes by re-reading `gpio_pin_get()` on each
  monitored pin every call, not by hardware interrupt. Sections 6.2-6.4 below describe (a) how to
  route this pin table's already-selected pins onto **real EXTI interrupts** where hardware
  allows, and (b) how to turn its existing `LOG_INF` change reports into **streamed USB records**
  (§4.1's `0x03` GPIO-delta record type) instead of/in addition to UART log lines.

This design is therefore an **extension of `gpio_mon.c`**, not a competing design: reuse its pin
table, its `pin_mode_t` enum, its `find_pin()`/`apply_mode()` machinery, and its hysteresis
counters. What's added is (1) EXTI-driven push instead of poll-driven scan, where hardware
allows, and (2) a USB streaming wire format and `gpio watch` shell command surface.

### 6.2 STM32F103 EXTI hardware constraint — the limiting factor

The STM32F103's EXTI (external interrupt) controller has **16 GPIO-capable lines, EXTI0-EXTI15**,
one per pin *number* (not per pin) — line `N` can be routed via `AFIO_EXTICRx` to **exactly one**
of PA*N*, PB*N*, PC*N*, PD*N*, PE*N* (etc.) at a time. This means: **you cannot get hardware
edge-interrupts on both PA5 and PB5 simultaneously** — only one pin per line number, board-wide,
can use EXTI at any moment. (EXTI16-19 exist too, but are PVD/RTC-alarm/USB-wakeup/... — not
general GPIO lines, not relevant here.)

Cross-referencing this against `gpio-pinout.md`'s actual pin usage shows **every single EXTI line
number (0 through 15) already has a confirmed-in-use pin on this board**, e.g.:

| Line | Contending pins (partial list, see `gpio-pinout.md` for full roles) |
|---|---|
| EXTI0 | PA0 (power button sense), PB0 (DMM_U600_RCLK), PC0 (USB VBUS mirror out), PE0 (DMM_SR_DATA) |
| EXTI4 | PA4 (FPGA_SPI1_CS), PB4 (FPGA_SSPI_CS), PC4 (DMM_SR_CLK), PD4 (FSMC_NOE) |
| EXTI7 | PA7 (SPI1_MOSI), PB7 (USB_POWER_FLAG), PC7 (BUZZER), PD7 (FSMC_NE1), PE7 (FSMC_D4) |
| EXTI13 | PA13 (SWDIO — never touch), PB13/PD13 (keypad rows), PC13 (unused, no ref) |

(Full per-line table generated during this doc's research — every line 0-15 has at least 2
same-numbered pins with a real, distinct role on this board; GPIOD/E are almost entirely consumed
by the FSMC LCD bus, which leaves GPIOA/B/C carrying most of the "interesting for debug" signals,
and they collide with each other constantly on shared line numbers.)

**Practical consequence for this design:**

- A user can request EXTI-based (true hardware edge-interrupt, zero-latency, no missed narrow
  pulses) watching on **any one pin per line number at a time** — e.g. watch PA5 via EXTI5 *or*
  watch PC5 via EXTI5, not both, and not simultaneously with whatever else on this board might
  transiently need EXTI5 (SPI1_SCK on PA5 doesn't need an interrupt in normal operation, so this
  is rarely a real conflict for *debug* purposes — the constraint mainly bites when a user wants
  to watch two *different* pins that happen to share a line number).
  The known-idle-during-debug candidates most useful for EXTI watching (input pins not
  continuously driven by another peripheral) are the keypad matrix inputs (PB5/PB6/PB12,
  EXTI5/6/12) and PC1/PC2 (USB sense inputs, EXTI1/2) — see §6.4 for the resulting shell UX.
- Any pin **not** currently holding its line number's EXTI slot falls back to **software
  polling** — exactly what `gpio_mon_poll()`/`gpio_mon_poll_hyst()` already do. This is not a
  degraded afterthought; it's the documented fallback path, and for a debug/bring-up tool
  (not a production timing-critical feature) polling at a fast tick (e.g. every 1 ms from the
  existing supervisor loop, or a dedicated higher-rate k_timer while any watch is active) already
  catches most bring-up-relevant glitches. Fast, sub-microsecond pulses on non-EXTI pins simply
  cannot be caught without EXTI — that limitation should be surfaced to the user (see §6.4's
  watch-status output), not hidden.
- **The device must arbitrate EXTI line ownership itself.** If pin PA5 currently "owns" EXTI5
  (say, nothing does today since SPI1_SCK doesn't need it) and the user asks to `gpio watch PC5`,
  the firmware must either (a) grant it (reassign EXTI5's `AFIO_EXTICR` mux to port C) if PA5 has
  no active interrupt need at that moment, or (b) refuse with a clear error ("EXTI5 already
  routed to PA5, cannot also watch PC5 — stop watching PA5 first, or PC5 will use polling
  instead") and offer the polling fallback. Silent double-booking (which pin's ISR actually fires
  is undefined once two ports share a mux'd line) must never happen.

### 6.3 Wire format — reuse §4.1's streaming record, two record types

Both configurable modes the task asks for ("state of all or a selectable subset," "consider
whether edge-triggered delta is more bandwidth-efficient than periodic polling") map onto the two
GPIO record types already reserved in §4.1's shared streaming endpoint:

**`0x02` — GPIO full snapshot** (periodic or on-demand, all-pins or selected-subset):

```
[8]      port/pin count (N, up to 6 fit in one 56-byte payload at 1 byte/pin-state; see below)
[9:9+N]  one byte per watched pin, in watch-list order: bit0=value, bit1=1 if this pin is
         EXTI-backed (hardware-accurate) vs polled (may have missed transitions), bits[2:7] rsvd
```

Used for `gpio watch --all` (§6.4) or any large subset — a full-device sweep doesn't fit useful
edge semantics per pin anyway when watching dozens of pins at once, so a periodic snapshot
(interval configurable, default suggestion: 10 ms — fast enough to see human-timescale toggling,
slow enough not to saturate the shared streaming endpoint against waveform data) is the practical
mode. This is a deliberate, explicit *poll* (with a known, stated interval, unlike the vendor
protocol's implicit host-side ~20 Hz loop) rather than a claim of edge accuracy.

**`0x03` — GPIO edge-triggered delta** (default for small, explicit pin lists):

```
[8]      pin index (position in the watch list this session, not a raw port/pin encoding)
[9]      new value (0/1)
[10]     source: 0=EXTI (hardware-timestamped, high confidence), 1=poll (timestamp is
         "detected at," not "occurred at" — may lag the real edge by up to one poll interval)
[11:15]  (LE u32) polls-since-last-change counter — lets the host distinguish "just changed"
         from "device only just started watching and this is the initial value"
[15:56]  reserved for future use (e.g. sub-record batching multiple simultaneous edges)
```

Preferred mode for a small, explicitly-named pin list (§6.4's `gpio watch PA0 PC15` form) — this
is the bandwidth-efficient mode the task description asks to weigh against periodic snapshotting:
a pin that never toggles costs **zero** streaming bandwidth after its initial value report, versus
a periodic-snapshot approach which costs bandwidth every interval regardless of activity. This
directly extends `gpio_mon.c`'s existing `LOG_INF("GPIO %-5s %d->%d ...")` change-report logic —
same trigger condition, output goes to a `0x03` streaming record instead of (or in addition to) a
log line.

### 6.4 Shell command surface

Extends `gpio_mon.c`'s existing `gpio_mon dump/mode/read/set/hyst` command family (§6.1) with a
`watch` subcommand family that starts/stops/lists **streaming** sessions (as opposed to the
existing commands, which are synchronous one-shot reads or local log-only monitoring):

```
gpio watch <PIN> [<PIN> ...]      Start an edge-triggered delta stream (§6.3 0x03 records) for
                                   the named pins. Each pin gets EXTI if its line number is free;
                                   otherwise falls back to polling — the start response reports
                                   which pins got which (see below). Returns a stream id (per
                                   §4's STREam:* control-channel commands) that the host uses to
                                   correlate 0x03 records on the shared streaming endpoint.

gpio watch --all [--interval=N]   Start a full-device periodic snapshot stream (§6.3 0x02
                                   records) at N ms interval (default 10 ms). Excludes 'S'-status
                                   (system: SWD/OSC) pins per gpio_mon.c's existing safety rule —
                                   never touch those, watch-only is safe but still excluded to
                                   avoid any accidental mode reconfiguration path opening on them.

gpio watch --list                 Show active watch sessions: pins, EXTI-backed vs polled, stream
                                   id, records sent, records dropped (from 0x7F overrun notices).

gpio watch --stop <id>            Stop a specific stream (frees its EXTI line ownership if it
                                   held one, so a subsequent watch on a different same-numbered
                                   pin can claim it — see §6.2's arbitration rule).

gpio watch --stop --all           Stop all active watch sessions.
```

Sample interactive session (values illustrative):

```
uart:~$ gpio watch PA0 PC15
watch: stream #1 started, 2 pins
  PA0   EXTI0  (hardware edge-interrupt)
  PC15  poll   (EXTI15 already owned by PB15/TIM1_CH3N-adjacent... no conflict actually,
                but PC15's line free — hardware-backed)
uart:~$ gpio watch PB5
watch: ERROR — EXTI5 already routed to PA0's neighbour... [example only; real message names
       the actual current EXTI5 owner and pin]
watch: PB5 added to stream #1 using polling fallback (10ms tick)
uart:~$ gpio watch --list
  #1  PA0(EXTI0) PC15(EXTI15) PB5(poll)   142 records sent, 0 dropped
uart:~$ gpio watch --stop 1
watch: stream #1 stopped, EXTI0 and EXTI15 released
```

(The exact free/busy EXTI5 example above is illustrative of the *mechanism*, not a definitive
claim about which pin currently owns EXTI5 in shipped firmware — that depends on what else is
running at watch-time, e.g. whether the keypad driver's rows are actively using interrupts.)

This command surface is reachable from **either shell backend** (UART1 or the new USB CDC-ACM
port, §5) — streamed records themselves only ever go out the dedicated streaming bulk endpoint
(§4.1), never over the CDC-ACM TTY, keeping "typing shell commands" and "receiving a firehose of
binary GPIO records" on physically separate pipes even though both can be initiated from the same
typed command.

---

## 7. Open questions / follow-up work

| # | Question | Why it's deferred |
|---|---|---|
| 1 | Full SCPI mnemonic tree for existing scope/DMM/AWG commands (the `usb-protocol.md` command tables) | Large surface, deserves its own doc once this framing doc is agreed; §3 only specifies the transport/framing, not command coverage |
| 2 | Waveform record payload layout (`0x01` in §4.1) — channel interleave, sample width, raw-FPGA vs MCU-resampled | Depends on capture-pipeline decisions not yet made in the new firmware; placeholder in §4.2 |
| 3 | Exact Zephyr Kconfig/devicetree symbol names for CDC-ACM (§5.3) | West manifest tracks Zephyr `main` (rolling) — verify against the actual checked-out revision at implementation time, both legacy and "next" USB stack options are moving targets upstream |
| 4 | Host-side driver/library shape (libusb? a Python module akin to `vendors/hantek/2d82/scripts/hantek_capture.py`? a `usbtmc`-alike Linux kernel driver?) | Not asked for in this task; a natural next design doc once §3/§4's wire format is implementation-tested |
| 5 | Authentication/safety for `STREam:*`/`gpio watch` commands that reassign EXTI ownership away from an in-use pin | §6.2 specifies "refuse with a clear error" as the safe default; whether some in-use pins should be entirely unwatchable (not just EXTI-refused-but-poll-allowed) needs a decision once real EXTI usage by other drivers (keypad, USB sense) is finalised in the new firmware |
| 6 | Concrete throughput measurement on real hardware | §2.4's "~1 MB/s ballpark" is a planning envelope from general USB FS bulk knowledge, not a measurement on this MCU/USB PHY/Zephyr USB stack combination — should be benchmarked once §5's composite device boots, to validate the streaming design's assumptions. §4.3's reconciliation (64-byte self-framed records are the right chunk size for this device, not a Keysight-style large single block) is a reasoned design conclusion from vendor comparison + USB spec limits, not itself a substitute for this measurement |
| 7 | ~~Whether Fluke's ScopeMeter/handheld product line uses SCPI over USB~~ **RESOLVED by research (§2.2)**: no — both the 289/287 handheld DMM and the 190-series ScopeMeter use proprietary ASCII protocols (informally documented for basics only) over USB-CDC, not SCPI. Remaining gap: Fluke's consumer-grade handhelds (87V/175-class) have no protocol documentation or teardown found at all — may have no PC connectivity | Does not block this design; §2.2's recommendation is grounded in the public SCPI/USBTMC standards and a synthesis of Fluke's own documented tiers, not a single assumed Fluke wire format |
| 8 | Fluke's exact company-wide policy on which product tiers get USBTMC/SCPI vs proprietary protocols | Inferred from the specific products checked (8845A/8846A/8588A/289/287/190-series) in §2.2, not a stated Fluke policy — treat the tier pattern as observed-but-not-confirmed-universal |
| 9 | ~~Whether Keysight/Tek/Rigol's large-transfer patterns should inform this device's chunk sizing~~ **RESOLVED by research (§2.5/§4.3)**: no, not directly — those vendors' documented patterns (up to 8M points/request for Keysight) are sized for USB High-Speed/SuperSpeed device ports (480 Mbit/s-5 Gbit/s, confirmed from their own guides), 40-400x this device's confirmed USB Full-Speed (12 Mbit/s) ceiling. §4.3 concludes the existing 64-byte self-framed per-record design (§4.1) is already correct for this hardware and should NOT be replaced with a Keysight-style single-large-block scheme | Does not block this design; §4.3's conclusion is a reasoned design decision, not left as an open question — recorded here per this doc's pattern of tracking resolved questions alongside open ones |

---

Cross-references: [`../hardware/usb-protocol.md`](usb-protocol.md) (vendor protocol,
reference/RE material) · [`driver-design.md`](driver-design.md) (boot sequence, existing
`usb_cdc_init()`/`usb_scope_init()` placement) · [`../reimplementation-plan.md`](
../reimplementation-plan.md) (peripheral list) · [`../hardware/gpio-pinout.md`](
../hardware/gpio-pinout.md) (pin naming convention, confirmed pin roles) · `code/diag_gpio/src/
gpio_mon.c` (existing GPIO shell command implementation this
design extends) · [`code/a2k_board/src/backlight.c`](../../code/a2k_board/src/backlight.c)
(existing `SHELL_CMD_REGISTER` pattern reference) · GH issue #32 "Redirect calibration debug
output from USART1 to USB CDC" (existing open ask this design's §5 CDC-ACM shell resolves the
transport half of, see §5.4).
