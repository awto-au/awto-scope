# FPGA RE database — query reference

All recovered bitstream data lives in the pluribus database. Default backend is
**PostgreSQL** (`fpga_re`). No export needed for normal use — postgres is the default.
To force SQLite (dev/offline only):

```bash
export PLURIBUS_DB_BACKEND=sqlite     # load + reach only (see below)
```

## Backend comparison

| Step | Postgres | SQLite |
|------|----------|--------|
| load | 3.8s | 1.2s |
| reach (BFS, 1 worker) | 3.1s | 0.8s |
| reach2 (ff_cones + dominators) | 13.3s | **impractical (>7 min)** |
| reach3 + patterns | parallel with reach2 | sequential, fast |
| reach4, report2, chains, verilog | ~1–2s each | same |
| **Full build** | **~40s** | **stalls at reach2** |

**Use postgres for all real work.** SQLite is fine for testing load/reach in
offline or dev environments — stop the pipeline before reach2.

### Why reach2 is impractical on SQLite

`pass_dominators` inside reach2 runs a CTE with a 3-way self-join on the
`reachability` table (252k rows × 252k rows). PostgreSQL resolves this in ~0.3s
using a hash join plan. SQLite can only do nested-loop joins and has no parallel
query planner — it does not complete in reasonable time.

`ff_cones` (also in reach2) spawns worker threads that each write to the DB.
SQLite WAL allows only one concurrent writer, so multiple threads fight for the
write lock. Fix in `run_pluribus.py`: `--workers 1` is passed to reach2 on
SQLite, serialising the writes. Still slow because all 1105 FFs are processed in
one batch before the insert.

**Architecture rule:** SQLite mode = ALL data in SQLite. Postgres mode = ALL
data in Postgres. No backend mixing.

## Connect (postgres)

```bash
psql -d fpga_re
```

All queries below assume the `V07` bitstream is loaded. Check with:
```sql
SELECT id, label, device, loaded_at FROM bitstreams;
```

## Rebuild from scratch

```bash
cd .
python3 fpga/scripts/run_pluribus.py          # full rebuild (postgres default)
python3 fpga/scripts/run_pluribus.py --report # print current DB state only
```

Runs the full pipeline: load → reach → reach2 → reach3 → patterns →
auto_name → reach4 → report2 + chains + verilog (parallel).
Prints a before/after row-count table at the end. Log: `fpga/tmp/rebuild.log`.

Pipeline script: `fpga/scripts/run_pluribus.py`. Also writes `fpga/aw2/out/V07-tilemap.svg`.

## Fuzz data

Fuzz results live in `fuzz_full.db` (SQLite, separate file). They are **not**
imported into the pluribus DB (neither postgres nor sqlite). `fuzz.py` queries
`fuzz_full.db` directly. The `run_pluribus.py` script does not touch fuzz data.

---

## Common queries

### Pad → net mapping

```sql
SELECT pin, label, direction, net_in, net_out, conn_class
FROM pad_map WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07')
ORDER BY pin;
```

### What drives a pad output?

```sql
-- ADC_ENCB (pin 98): which FF drives it?
SELECT pm.pin, pm.label, ff.cell, ff.clk, ff.d, ff.ce, cds.hpbx_track
FROM pad_map pm
JOIN ffs ff ON ff.bitstream=pm.bitstream AND ff.q=pm.net_out
JOIN clock_domain_summary cds ON cds.bitstream=pm.bitstream AND cds.clk_net=ff.clk
WHERE pm.bitstream=(SELECT id FROM bitstreams WHERE label='V07')
AND pm.label='ADC_ENCB';
```

### Clock domains

```sql
SELECT clk_net, ff_count,
       CASE hpbx_track
         WHEN 'BRANCH_HPBX0000' THEN 'HPBX0 (125 MHz CLKOS)'
         WHEN 'BRANCH_HPBX0100' THEN 'HPBX1 (~62.5 MHz CLKOS2)'
         WHEN 'BRANCH_HPBX0200' THEN 'HPBX2 (250 MHz CLKOP)'
         WHEN 'BRANCH_HPBX0300' THEN 'HPBX3'
         ELSE '?'
       END AS track_label
FROM clock_domain_summary
WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07')
ORDER BY ff_count DESC;
```

HPBX track → PLL output assignment (confirmed from bitstream + EBR analysis):

| Track | PLL output | Frequency | Evidence |
|-------|-----------|-----------|---------|
| HPBX0 | CLKOS     | **125 MHz** | n1858 drives ADC_ENCB/DAC_CLK FF; EBR JCLK0 (read) |
| HPBX1 | CLKOS2?   | **~62.5 MHz?** | Lower FF count; AWG EBR R6C20 JCLK3 (n2014) — **frequency inferred, not confirmed** |
| HPBX2 | CLKOP     | **250 MHz** | Highest FF count; ADC EBR write clocks; AWG DDS (n1943) |
| HPBX3 | CLKOS3?   | 250 MHz?  | Very low use in V07 |

### EBR clock assignments

```sql
SELECT ep.block, ep.port, ep.net, cds.hpbx_track, cds.ff_count
FROM ebr_ports ep
LEFT JOIN clock_domain_summary cds
  ON cds.bitstream=ep.bitstream AND cds.clk_net=ep.net
WHERE ep.bitstream=(SELECT id FROM bitstreams WHERE label='V07')
AND ep.role='ctrl' AND ep.port LIKE 'JCLK%'
ORDER BY ep.block, ep.port;
```

### Routing arc trace

```sql
-- Find all arcs sourced from net n2508 (ADC_ENCA) on the right edge
SELECT tile_row, tile_col, source_wire, sink_wire, sink_net
FROM arcs
WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07')
AND source_net='n2508' AND tile_col=21
ORDER BY tile_row;

-- What drives a specific net?
SELECT tile_row, tile_col, source_wire, sink_wire, source_net
FROM arcs
WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07')
AND sink_net='n1147' AND source_net != 'n1147'
LIMIT 20;
```

### HPBX spine taps

```sql
SELECT tile_row, track, local_net
FROM hpbx_branches
WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07') AND tile_col=1
ORDER BY track, tile_row;
```

### Net reachability

```sql
-- What can net n2518 (SPI CS) reach?
SELECT dst, min_hops
FROM reachability
WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07') AND src='n2518'
ORDER BY min_hops LIMIT 20;

-- Which input pads can reach a given FF's D input?
SELECT DISTINCT pm.label AS pad, pfi.min_hops
FROM pad_ff_influence pfi
JOIN pad_map pm ON pm.bitstream=pfi.bitstream AND pm.label=pfi.pad_label
WHERE pfi.bitstream=(SELECT id FROM bitstreams WHERE label='V07')
AND pfi.ff_cell='ff_r4c11_d1k0'
ORDER BY pfi.min_hops;
```

### EFB ports

```sql
SELECT port_name, net FROM efb_ports
WHERE bitstream=(SELECT id FROM bitstreams WHERE label='V07')
ORDER BY port_name;
```

---

## Net name annotations — aw2-nets.tsv

Human-maintained net name table. Edit the TSV, run `./fpga/scripts/run_pluribus`, names
propagate everywhere (chains.txt, report, queries).

**File:** `fpga/aw2/aw2-nets.tsv`

### Schema

| Column | Values | Meaning |
|--------|--------|---------|
| `net` | `nNNNN` | Internal net ID from prjtrellis. Primary key. |
| `name` | `clk_h0_adc_jq` etc. | Canonical short name replacing nNNNN in all output. |
| `type` | `clk` `data` `ctrl` `pad` `unknown` | Signal category. |
| `confidence` | `confirmed` `inferred` `speculative` | How much to trust this. |
| `freq_mhz` | number or blank | Clock frequency (clk type only). |
| `hpbx` | `BRANCH_HPBX0000` etc. or blank | HPBX clock spine. |
| `ff_count` | number or blank | FFs clocked by this net (clk type only). |
| `notes` | free text | Reasoning chain. AI-generated entries start with `AI:`. |

### Confidence levels

- **`confirmed`** — Derived directly from bitstream measurement, schematic, or physical trace.
- **`inferred`** — Strong structural reasoning. Likely correct but not independently verified.
- **`speculative`** — Educated guess. Notes must explain the reasoning.

### Naming convention

```
Clock nets:  clk_<domain>_<function>
  domain:    h0 = HPBX0000 (125 MHz, CLKOS)
             h1 = HPBX0100 (~62.5 MHz, CLKOS2 — frequency inferred)
             h2 = HPBX0200 (250 MHz, CLKOP)
  function:  adc_cap  adc_enc  adc_wr  adc_jq  awg_dds  awg_wr
             dac_enc  spi_*  unk_*

Data nets:   <subsystem>_<noun>[_bit]
  subsystem: adc  dac  awg  spi  trig  afe
```

---

## Schema overview

| Table | Contents |
|-------|---------|
| `bitstreams` | One row per loaded bitstream version |
| `nets` | All recovered net names (3104 in V07) |
| `ffs` | All flip-flops with clk/ce/d/q/lsr (1105 in V07) |
| `luts` | All LUTs with init, input nets, fn classification (1138 in V07) |
| `net_fanout` | Which net drives which cell pin, and what that cell outputs |
| `pad_map` | Physical pin → fabric net + IO config (45 pads in V07) |
| `efb_ports` | EFB hard IP port → fabric net |
| `ebr_ports` | EBR block port → fabric net, by role (read/write/ctrl) |
| `clock_domains` | Per-FF clock membership (38 domains in V07) |
| `clock_domain_summary` | Per-clock-net FF count + HPBX track |
| `arcs` | All 12,808 V07 routing arcs with globalised wire coordinates |
| `hpbx_branches` | HPBX spine track → local net per tile column (455 rows) |
| `reachability` | All-net BFS forward reachability (252,218 pairs in V07) |
| `reachability_rev` | Reverse reachability index (252,218 rows) |
| `ff_cones` | Input/output combinational cones per FF |
| `critical_paths` | Longest combinational chain between each FF pair |
| `dominators` | Nets that lie on every pad→FF path |
| `lut_symbolic` | Symbolic boolean expressions for each LUT output |
| `pad_ff_influence` | Which pads can reach each FF's D/CE input |

Full schema: `<workspace>/pluribus/schema.py`
