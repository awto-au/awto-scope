//
// ScopeSimBridge — a Renode peripheral that forwards the STM32's SPI1 traffic to the
// live aw2_gateware simulation (scope_sim_server.py) over a TCP socket, so the
// firmware running in Renode drives the REAL new FPGA design (SPI-v2) and reads back
// real responses. This is the Renode end of the co-simulation; the Python side runs
// the amaranth.sim of Aw2Gateware.
//
// Contrast with FpgaSPI.cs: that models the OEM FPGA's protocol behaviourally. This
// bridge holds no protocol state — it is a transparent pipe to the gateware sim, so
// what the firmware sees IS the synthesised design.
//
// Wiring (see README.md). Two connections:
//   1) MMIO at the SPI1 controller registers (replaces the SPI1 stub):
//        scopeSim: Video.ScopeSimBridge @ sysbus <0x40013000, +0x400>
//   2) The CS line. The firmware toggles CS on GPIOA PA4; route that edge here so the
//      bridge can frame transactions (the SPI-v2 slave resets its phase on CS high):
//        gpioPortA: ... { 4 -> scopeSim@0 }
//
// The sim server must be running first:  python3 fpga/sim/scope_sim_server.py --port 42001
//
// Socket protocol (2-byte request -> 1-byte reply), see scope_sim_server.py:
//   [0x01,0]->[0] CS low   [0x02,0]->[0] CS high   [0x03,b]->[miso] xfer   [0x04,0] quit
//
using System;
using System.Net.Sockets;
using Antmicro.Renode.Peripherals.Bus;
using Antmicro.Renode.Peripherals.GPIOPort;
using Antmicro.Renode.Logging;

namespace Antmicro.Renode.Peripherals.Video
{
    public class ScopeSimBridge : IDoubleWordPeripheral, IWordPeripheral, IBytePeripheral,
                                  IGPIOReceiver, IKnownSize
    {
        public ScopeSimBridge(string host = "127.0.0.1", int port = 42001)
        {
            this.host = host;
            this.port = port;
            Reset();
        }

        public long Size => 0x400;

        public void Reset()
        {
            lastResponse = 0;
            csAsserted = false;
            Reconnect();
        }

        // ---- SPI1 controller MMIO: SR always ready; DR write -> xfer; DR read -> last MISO.
        public uint ReadDoubleWord(long offset) => ReadReg(offset);
        public ushort ReadWord(long offset) => (ushort)ReadReg(offset);
        public byte ReadByte(long offset) => (byte)ReadReg(offset);
        public void WriteDoubleWord(long offset, uint value) => WriteReg(offset, (byte)value);
        public void WriteWord(long offset, ushort value) => WriteReg(offset, (byte)value);
        public void WriteByte(long offset, byte value) => WriteReg(offset, value);

        private uint ReadReg(long offset)
        {
            switch(offset)
            {
                case 0x08: return 0x03;         // SR: RXNE=1, TXE=1, BSY=0 — never blocks the firmware
                case 0x0c: return lastResponse; // DR: MISO byte from the last xfer
                default:   return 0;
            }
        }

        private void WriteReg(long offset, byte b)
        {
            if(offset != 0x0c) return;          // only DR writes carry SPI bytes
            lastResponse = Xfer(b);
        }

        // ---- CS line, routed from GPIOA PA4. Low = frame begin, high = frame end.
        public void OnGPIO(int number, bool value)
        {
            // PA4 is active-low chip-select. value==false -> CS asserted.
            bool assert = !value;
            if(assert == csAsserted)
            {
                return;
            }
            csAsserted = assert;
            Send((byte)(assert ? 0x01 : 0x02), 0);   // OP_CS_LOW / OP_CS_HIGH
        }

        // ---- socket plumbing ----
        private byte Xfer(byte mosi) => Send(0x03, mosi);   // OP_XFER

        private byte Send(byte op, byte arg)
        {
            try
            {
                EnsureConnected();
                stream.WriteByte(op);
                stream.WriteByte(arg);
                stream.Flush();
                int r = stream.ReadByte();
                return r < 0 ? (byte)0 : (byte)r;
            }
            catch(Exception e)
            {
                this.Log(LogLevel.Warning, "ScopeSimBridge: socket error ({0}); is scope_sim_server.py running on {1}:{2}?",
                         e.Message, host, port);
                Reconnect();
                return 0;
            }
        }

        private void EnsureConnected()
        {
            if(client != null && client.Connected)
            {
                return;
            }
            client = new TcpClient();
            client.NoDelay = true;
            client.Connect(host, port);
            stream = client.GetStream();
        }

        private void Reconnect()
        {
            try { stream?.Dispose(); } catch { }
            try { client?.Close(); } catch { }
            client = null; stream = null;
        }

        private readonly string host;
        private readonly int port;
        private TcpClient client;
        private NetworkStream stream;
        private byte lastResponse;
        private bool csAsserted;
    }
}
