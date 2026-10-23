Title: STLINK-V3SET debugger/programmer for STM8 and STM32 - User manual

URL Source: https://www.st.com/resource/en/user_manual/um2448-stlinkv3set-debuggerprogrammer-for-stm8-and-stm32-stmicroelectronics.pdf

Number of Pages: 55

Markdown Content:
April 2025 UM2448 Rev 9 1/55 1

# UM2448 User manual 

# STLINK-V3SET debugger/programmer for STM8 and STM32 

# Introduction 

The STLINK-V3SET is a standalone modular debugging and programming probe for the STM8 and STM32 microcontrollers. This product consists of a main module and a complementary adapter board. It supports SWIM and JTAG/SWD interfaces for communication with any STM8 or STM32 microcontroller on an application board. 

The STLINK-V3SET provides a Virtual COM port interface allowing the host PC to communicate with the target microcontroller through one UART. It also includes bridge interfaces to several communication protocols allowing, for instance, the programming of the target through the bootloader. 

The STLINK-V3SET can provide a second Virtual COM port interface allowing the host PC to communicate with the target microcontroller through another UART, called the bridge UART. Bridge UART signals, including optional RTS and CTS, are only available on the MB1440 adapter board. The second Virtual COM port activation is done through a reversible firmware update, which disables the mass-storage interface used for drag-and-drop flash programming. 

The modular architecture of STLINK-V3SET enables the extension of its main features through additional modules such as the adapter board for different connectors, the B-STLINK-VOLT board for voltage adaptation, and the B-STLINK-ISOL board for voltage adaptation and galvanic isolation. 

Figure 1. STLINK-V3SET product top view 

> Picture is not contractual.
> www.st.com

Contents UM2448  

> 2/55 UM2448 Rev 9

# Contents 

1 Features . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 7 2 General information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 8 3 Ordering information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 8 4 Development environment . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 9 

4.1 System requirements . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 9 4.2 Development toolchains . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 9 

5 Conventions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 9 6 Quick start . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 10 7 STLINK-V3SET functional description . . . . . . . . . . . . . . . . . . . . . . . . . 11 

7.1 STLINK-V3SET overview . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .11 7.1.1 Main module for high performance . . . . . . . . . . . . . . . . . . . . . . . . . . . . 11 7.1.2 Adapter configuration for added functions . . . . . . . . . . . . . . . . . . . . . . . 12 7.1.3 How to build the adapter configuration for added functions . . . . . . . . . . 13 7.2 Hardware layout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 14 7.3 Mechanical information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19 7.4 STLINK-V3SET functions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19 7.4.1 SWD with SWV . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19 7.4.2 JTAG . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19 7.4.3 SWIM . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20 7.4.4 Virtual COM port (VCP) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20 7.4.5 Bridge functions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 20 7.4.6 LEDs . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22 7.5 Jumper configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22 

8 Board connectors . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23 

8.1 Connectors on MB1441 board . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23 8.1.1 USB Micro-B . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23 8.1.2 STDC14 (STM32 JTAG/SWD and VCP) . . . . . . . . . . . . . . . . . . . . . . . . 24 UM2448 Rev 9 3/55 

UM2448 Contents 

> 4

8.2 Connectors on MB1440 board . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24 8.2.1 STDC14 (STM32 JTAG/SWD and VCP) . . . . . . . . . . . . . . . . . . . . . . . . 24 8.2.2 Legacy Arm 20-pin JTAG/SWD IDC connector . . . . . . . . . . . . . . . . . . . 25 8.2.3 Virtual COM port connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25 8.2.4 SWIM connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26 8.2.5 CAN connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26 8.2.6 SWD connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26 8.2.7 UART/I 2 C/CAN bridge connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27 8.2.8 SPI/GPIO bridge connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27 8.2.9 Bridge 20-pin connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28 

9 Flat ribbons . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 29 10 Software configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 31 

10.1 Supporting toolchains (not exhaustive) . . . . . . . . . . . . . . . . . . . . . . . . . . 31 10.2 Drivers and firmware upgrade . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 31 10.3 STLINK-V3SET frequency selection . . . . . . . . . . . . . . . . . . . . . . . . . . . . 31 10.4 Mass-storage interface . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 31 10.5 Bridge interface . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 32 

11 B-STLINK-VOLT board extension description . . . . . . . . . . . . . . . . . . . 33 

11.1 Features . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33 11.2 Connection instructions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 33 11.2.1 Closed casing for STM32 debug 

(STDC14 connector only) with B-STLINK-VOLT . . . . . . . . . . . . . . . . . . 33 11.2.2 Opened casing for access to all connectors 

(through MB1440 adapter board) with B-STLINK-VOLT . . . . . . . . . . . . 35 11.3 Selection of bridge GPIO direction . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 36 11.4 Jumper configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37 11.5 Target voltage connection . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37 11.6 Board connectors . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38 11.6.1 STDC14 (STM32 JTAG/SWD and VCP) . . . . . . . . . . . . . . . . . . . . . . . . 38 11.6.2 UART/I 2 C/CAN bridge connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38 11.6.3 SPI/GPIO bridge connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 38 

12 B-STLINK-ISOL board extension description . . . . . . . . . . . . . . . . . . . . 39 

12.1 Features . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39 Contents UM2448  

> 4/55 UM2448 Rev 9

12.2 Connection instructions . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 39 12.2.1 Closed casing for STM32 debug   

> (STDC14 connector only) with B-STLINK-ISOL . . . . . . . . . . . . . . . . . . 39 12.2.2 Opened casing for access to all connectors
> (through MB1440 adapter board) with B-STLINK-ISOL . . . . . . . . . . . . . 40

12.3 Bridge GPIO direction . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42 12.4 Jumper configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42 12.5 Target voltage connection . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42 12.6 Board connectors . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42 12.6.1 STDC14 (STM32 JTAG/SWD and VCP) . . . . . . . . . . . . . . . . . . . . . . . . 42 12.6.2 UART/I 2 C/CAN bridge connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42 12.6.3 SPI/GPIO bridge connector . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 42 

13 Performance figures . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43 

13.1 Global overview . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43 13.2 Baud rate computing . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 43 

14 Product information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 44 

14.1 Product marking . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 44 14.2 Product history . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 45 14.3 Board revision history . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 47 

15 Safety certification information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 49 16 Federal Communications Commission (FCC) and ISED Canada 

Compliance Statements . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 50 

16.1 FCC Compliance Statement . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 50 16.2 ISED Compliance Statement . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 51 

17 Product disposal . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 52 Revision history . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 53 UM2448 Rev 9 5/55 

UM2448 List of tables 

> 5

# List of tables 

Table 1. Ordering information . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 8 Table 2. ON/OFF convention . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 9 Table 3. MB1441 jumper configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22 Table 4. MB1440 jumper configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 22 Table 5. USB Micro-B connector (CN5) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23 Table 6. STDC14 connector (CN1) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 24 Table 7. Legacy Arm 20-pin JTAG/SWD IDC connector (CN2) pinout. . . . . . . . . . . . . . . . . . . . . . . 25 Table 8. Virtual COM port connector (CN3) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 25 Table 9. SWIM connector (CN4) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26 Table 10. CAN connector (CN5) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26 Table 11. SWD (wires) connector (CN6) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 26 Table 12. UART bridge connector (CN7) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27 Table 13. SPI bridge connector (CN8) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 27 Table 14. Bridge connector (CN9) pinout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 28 Table 15. ARM10-compatible connector pinout (target side) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 29 Table 16. ARM20-compatible connector pinout (target side) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 30 Table 17. Toolchain versions supporting STLINK-V3SET . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 31 Table 18. MB1598 jumper configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37 Table 19. Achievable maximal performance with STLINK-V3SET on different channels. . . . . . . . . . 43 Table 20. Product history . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 45 Table 21. Board revision history . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 47 Table 22. Document revision history . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 53 List of figures UM2448  

> 6/55 UM2448 Rev 9

# List of figures 

Figure 1. STLINK-V3SET product top view . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 1 Figure 2. Probe top side. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 11 Figure 3. Connections . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 12 Figure 4. Probe with adapter . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 12 Figure 5. Operating mode to change the configuration . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 13 Figure 6. Hardware board MB1441 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 14 Figure 7. Hardware board MB1440 . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 15 Figure 8. MB1441 top layout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 16 Figure 9. MB1441 bottom layout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 16 Figure 10. MB1440 top layout . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 17 Figure 11. MB1441 mechanical drawing . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 17 Figure 12. MB1440 mechanical drawing . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 18 Figure 13. MB1441 mechanical dimensions (in millimeters) . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 19 Figure 14. Firmware selection for a second Virtual COM port . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 21 Figure 15. USB Micro-B connector (CN5) front view . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 23 Figure 16. Closed casing for STM32 debug with B-STLINK-VOLT . . . . . . . . . . . . . . . . . . . . . . . . . . . 34 Figure 17. Opened casing for access to all connectors with B-STLINK-VOLT . . . . . . . . . . . . . . . . . . 36 Figure 18. B-STLINK-VOLT . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . 37 Figure 19. Closed casing for STM32 debug with B-STLINK-ISOL. . . . . . . . . . . . . . . . . . . . . . . . . . . . 40 Figure 20. Opened casing for access to all connectors with B-STLINK-ISOL . . . . . . . . . . . . . . . . . . . 41 UM2448 Rev 9 7/55 

UM2448 Features 

> 54

# 1 Features 

• Standalone probe with modular extensions 

• Self-powered through a USB connector (Micro-B) 

• USB 2.0 high-speed interface 

• Probe firmware update through USB 

• JTAG/SWD (serial wire debugging) specific features: – 3 V to 3.6 V application voltage support and 5 V tolerant inputs (extended down to 1.65 V with the B-STLINK-VOLT or B-STLINK-ISOL board) – Flat cables STDC14 to MIPI10/STDC14/MIPI20 (connectors with 1.27 mm pitch) – JTAG communication support – SWD and serial wire viewer (SWV) communication support 

• SWIM-specific features (only available with adapter board MB1440): – 1.65 V to 5.5 V application voltage support – SWIM header (2.54 mm pitch) – SWIM low-speed and high-speed modes support 

• Virtual COM port (VCP) specific features: – 3 V to 3.6 V application voltage support on the UART interface and 5 V tolerant inputs (extended down to 1.65 V with the B-STLINK-VOLT or B-STLINK-ISOL board) – VCP frequency up to 16 MHz – Available on STDC14 debug connector (not available on MIPI10) 

• Multipath bridge USB to SPI/UART/I 2 C/CAN/GPIOs specific features: – 3 V to 3.6 V application voltage support and 5 V tolerant inputs (extended down to 1.65 V with the B-STLINK-VOLT or B-STLINK-ISOL board) – Signals available on adapter board only (MB1440) 

• Drag-and-drop flash programming of binary files 

• Two-color LEDs: communication, power 

Note: The STLINK-V3SET product does not provide the power supply to the target application. 

B-STLINK-VOLT is not required for STM8 targets, for which voltage adaptation is performed on the baseline adapter board (MB1440) provided with the STLINK-V3SET. General information UM2448 

8/55 UM2448 Rev 9 

# 2 General information 

The STLINK-V3SET embeds an STM32 32-bit microcontroller based on the Arm ®(a) 

Cortex ®-M processor. 

# 3 Ordering information 

To order the STLINK-V3SET or any additional board (provided separately), refer to  Table  1.

a. Arm is a registered trademark of Arm Limited (or its subsidiaries) in the US and/or elsewhere. 

Table 1. Ordering information Order code Board reference Description 

STLINK-V3SET MB1441 (1) MB1440 (2) 1. Main module. 2. Adapter board. STLINK-V3 modular in-circuit debugger and programmer for STM8 and STM32 B-STLINK-VOLT MB1598 (3) 3. Voltage adapter board Voltage adapter board for STLINK-V3SET B-STLINK-ISOL MB1599 (4) 4. Voltage adapter and galvanic isolation board Voltage adapter and galvanic isolation board for STLINK-V3SET UM2448 Rev 9 9/55 

UM2448 Development environment 

> 54

# 4 Development environment 4.1 System requirements 

• Multi-OS support: Windows ® 10, Linux ® 64-bit, or macOS ®(a)(b)(c) 

• USB Type-A or USB Type-C ® to Micro-B cable 

# 4.2 Development toolchains 

• IAR Systems ® - IAR Embedded Workbench ®(d) 

• Keil ® - MDK-ARM (d) 

• STMicroelectronics - STM32CubeIDE 

# 5 Conventions 

Table  2 provides the conventions used for the ON and OFF settings in the present 

document. 

a. macOS ® is a trademark of Apple Inc. registered in the U.S. and other countries. b. Linux ® is a registered trademark of Linus Torvalds. c. All other trademarks are the property of their respective owners. d. On Windows ® only. 

Table 2. ON/OFF convention Convention Definition 

Jumper JPx ON Jumper fitted Jumper JPx OFF Jumper not fitted Jumper JPx [1-2] Jumper must be fitted between pin 1 and pin 2 Solder bridge SBx ON SBx connections closed by 0-ohm resistor Solder bridge SBx OFF SBx connections left open Quick start UM2448  

> 10/55 UM2448 Rev 9

# 6 Quick start 

This section describes how to start development quickly using the STLINK-V3SET. 

Before installing and using the product, accept the evaluation product license agreement from the www.st.com/epla web page. 

The STLINK-V3SET is a standalone modular debugging and programming probe for STM8 and STM32 microcontrollers. 

• It supports protocols SWIM, JTAG, and SWD to communicate with any STM8 or STM32 microcontroller. 

• It provides a Virtual COM port interface allowing the host PC to communicate with the target microcontroller through one UART 

• It provides bridge interfaces to several communication protocols allowing, for instance, the programming of the target through the bootloader. 

To start using this board, follow the steps below: 

1. Check that all items are available inside the box 

(V3S + 3 flat cables + adapter board and its guide). 

2. Install/update the IDE/STM32CubeProgrammer to support the STLINK-V3SET (drivers). 

3. Choose a flat cable and connect it between the STLINK-V3SET and the application. 

4. Connect a USB Type-A to Micro-B cable between the STLINK-V3SET and the PC. 

5. Check that the PWR LED is green and the COM LED is red. 

6. Open the development toolchain or STM32CubeProgrammer (STM32CubeProg) software utility. 

For more details, refer to the www.st.com/stlink-v3set website. UM2448 Rev 9 11/55 

UM2448 STLINK-V3SET functional description 

> 54

# 7 STLINK-V3SET functional description 7.1 STLINK-V3SET overview 

The STLINK-V3SET is a standalone modular debugging and programming probe for the STM8 and STM32 microcontrollers. This product supports many functions and protocols for debugging, programming, and communicating with one or several targets. The STLINK-V3SET package includes complete hardware with the main module for high performance and an adapter board for added functions to connect with wires or flat cables anywhere in the application. 

This module is fully powered by the PC. If the COM LED blinks red, refer to the technical note Overview of ST-LINK derivatives (TN1235) at www.st.com for details. 

7.1.1 Main module for high performance 

This configuration is the preferred one for high performance. It only supports STM32 microcontrollers. The working voltage range is 3 to 3.6  V. 

Figure 2. Probe top side 

The protocols and functions supported are: 

• SWD (up to 24 MHz) with SWO (up to 16 MHz) 

• JTAG (up to 21 MHz) 

• VCP (from 732 bps to 16 Mbps) 

A 2x7-pin 1.27  mm pitch male connector is located in the STLINK-V3SET for connection to the application target. Three different flat cables are included in the packaging to connect STLINK-V3SET functional description UM2448  

> 12/55 UM2448 Rev 9

with standard connectors MIPI10/ARM10, STDC14, and ARM20 (refer to  Section  9: Flat 

ribbons on page  29 ). 

Refer to  Figure  3 for connections: 

Figure 3. Connections 7.1.2 Adapter configuration for added functions 

This configuration favors the connection to targets using wires or flat cables. It is composed of MB1441 and MB1440. It supports debugging, programming, and communicating with STM32 and STM8 microcontrollers. 

Figure 4. Probe with adapter UM2448 Rev 9 13/55 

UM2448 STLINK-V3SET functional description 

> 54

7.1.3 How to build the adapter configuration for added functions 

Refer to the operating mode below to build the adapter configuration from the main module configuration and back. 

Figure 5. Operating mode to change the configuration STLINK-V3SET functional description UM2448  

> 14/55 UM2448 Rev 9

# 7.2 Hardware layout 

The STLINK-V3SET product is designed around the STM32F723 microcontroller (176-pin in a UFBGA package). The hardware board pictures ( Figure  6 and  Figure  7) show the two 

boards included in the package in their standard configurations (components and jumpers). 

Figure  8, Figure  9, and  Figure  10  help users locate the features on the board. The 

mechanical dimensions of the STLINK-V3SET product are shown in  Figure  11  and 

Figure  12 .

Figure 6. Hardware board MB1441 UM2448 Rev 9 15/55 

UM2448 STLINK-V3SET functional description 

> 54

Figure 7. Hardware board MB1440 STLINK-V3SET functional description UM2448 

16/55 UM2448 Rev 9 

Figure 8. MB1441 top layout Figure 9. MB1441 bottom layout    

>          !" #$%&'()  * +,+*  -+.-/ !0,12,0

 UM2448 Rev 9 17/55 

UM2448 STLINK-V3SET functional description 

54 

Figure 10. MB1440 top layout Figure 11. MB1441 mechanical drawing 

$-3! '  . . . . 3 '  #$ #$2 #$4 3!/   3!/ *.,5 * '  */ *  -   -+* +,+*+.-/ ,+*- ,+ - 

 STLINK-V3SET functional description UM2448  

> 18/55 UM2448 Rev 9

Figure 12. MB1440 mechanical drawing UM2448 Rev 9 19/55 

UM2448 STLINK-V3SET functional description 

> 54

# 7.3 Mechanical information Figure 13. MB1441 mechanical dimensions (in millimeters) 7.4 STLINK-V3SET functions 

All functions have been designed for high performance: all signals are 3.3  V compatible except the SWIM protocol, which supports a voltage range of 1.65 to 5.5  V. The following description concerns the two boards MB1441 and MB1440 and indicates where to find the functions on the boards and connectors. The main module for high performance only includes the MB1441 board. The adapter configuration for added functions includes both the MB1441 and MB1440 boards. 

7.4.1 SWD with SWV 

The SWD protocol is a Debug/Program protocol used for STM32 microcontrollers with SWV as a trace. The signals are 3.3  V compatible and can perform up to 24  MHz. This function is available on MB1440 CN1, CN2, and CN6, and MB1441 CN1. 

For details regarding the baud rate, refer to  Section  13.2 .

7.4.2 JTAG 

The JTAG protocol is a Debug/Program protocol used for STM32 microcontrollers. The signals are 3.3  V compatible and can perform up to 21  MHz. This function is available on MB1440 CN1 and CN2 connectors, and MB1441 CN1. 

The STLINK-V3SET does not support the chaining of devices in JTAG (daisy chain). STLINK-V3SET functional description UM2448  

> 20/55 UM2448 Rev 9

For correct operation, the STLINK-V3SET microcontroller on the MB1441 board requires a JTAG return clock. By default, this return clock is provided through the closed jumper JP1 on MB1441, but might also be externally provided through pin  9 of CN1 (This configuration might be necessary to reach high JTAG frequencies; in this case, JP1 on MB1441 must be opened). In case of use with the B-STLINK-VOLT extension board, the JTAG clock loopback must be removed from the STLINK-V3SET board (JP1 opened). For the correct functioning of JTAG, the loopback must be done either on the B-STLINK-VOLT extension board (JP1 closed) or on the target application side. 

7.4.3 SWIM 

The SWIM protocol is a Debug/Program protocol used for STM8 microcontrollers. JP3, JP4, and JP6 on the MB1440 board must be ON to activate the SWIM protocol. JP2 on the MB1441 board must also be ON (default position). The signals are available on the MB1440 CN4 connector and a voltage range of 1.65 to 5.5  V is supported. 

Note that a 680  Ω pull up to VCC, pin  1 of MB1440 CN4, is provided on DIO, pin  2 of MB1440 CN4, and consequently: 

• No additional external pull-up is required. 

• VCC of MB1440 CN4 must be connected to V target .

7.4.4 Virtual COM port (VCP) 

The serial interface VCP is directly available as a Virtual COM port of the PC, connected to the STLINK-V3SET USB connector (CN5). This function can be used for STM32 and STM8 microcontrollers. The signals are 3.3  V compatible and can perform from 732  bps to 16  Mbps. This function is available on MB1440 CN1 and CN3 connectors, and MB1441 CN1. T_VCP_RX (or Rx) signal is the Rx for the target (Tx for the STLINK-V3SET) and T_VCP_TX (or Tx) signal is the Tx for the target (Rx for the STLINK-V3SET). 

A second Virtual COM port might be activated, as detailed later in  Section  7.4.5  (Bridge 

UART). 

For details regarding the baud rate, refer to  Section  13.2 .

7.4.5 Bridge functions 

The STLINK-V3SET provides a proprietary USB interface that allows communication with any STM8 or STM32 target using several protocols: SPI, I 2C, CAN, UART, and GPIOs. This interface might be used to communicate with the target bootloader, but might also be used for customized needs through its public software interface. 

All bridge signals can be easily accessed on CN9 using wire clips, at the risk of reducing signal quality and performance, particularly for SPI and UART. This depends, for instance, on the quality of the wires used, whether the cables are shielded or not, and on the layout of the application board. 

Bridge SPI 

SPI signals are available on MB1440 CN8 and CN9. To reach a high SPI frequency, it is recommended to use a flat ribbon on MB1440 CN8 with all unused signals tied to the ground on the target side. UM2448 Rev 9 21/55 

UM2448 STLINK-V3SET functional description 

> 54

Bridge I 2C

I2C signals are available on MB1440 CN7 and CN9. The adapter module also provides optional 680  Ω pull-ups, which can be activated by closing JP10 jumpers. In that case, the T_VCC target voltage must be supplied to any MB1440 connectors accepting it (CN1, CN2, CN6, or JP10 jumpers). 

Bridge CAN 

CAN logic signals (Rx/Tx) are available on MB1440 CN9. They can be used as inputs for an external CAN transceiver. It is also possible to connect directly the CAN target signals to MB1440 CN5 (target Tx to CN5 Tx, target Rx to CN5 Rx), provided that: 

1. JP7 is closed, meaning CAN is ON. 

2. CAN voltage is provided to CN5 CAN_VCC. 

Bridge UART 

UART signals with hardware flow control (CTS/RTS) are available on MB1440 CN9 and MB1440 CN7. They need dedicated firmware to be programmed on the main module before use. With this firmware, a second Virtual COM port is available and the mass-storage interface (used for Drag-and-drop flash programming) disappears. The firmware selection is reversible and is done by STLinkUpgrade applications as shown in  Figure  14 . The hardware 

flow control might be activated by physically connecting UART_RTS and UART_CTS signals to the target. If not connected, the second Virtual COM port works without hardware flow control. Note that the hardware flow control activation/deactivation cannot be configured by software from the host side on a Virtual COM port. Consequently, configuring a parameter related to that on the host application does not affect the system behavior. 

To reach a high UART frequency, it is recommended to use a flat ribbon on MB1440 CN7 with all unused signals tied to the ground on the target side. 

Figure 14. Firmware selection for a second Virtual COM port 

For details regarding the baud rate, refer to  Section  13.2 .

Bridge GPIOs 

Four GPIO signals are available on MB1440 CN8 and CN9. The basic management is provided by the public ST bridge software interface. STLINK-V3SET functional description UM2448 

22/55 UM2448 Rev 9 

7.4.6 LEDs 

PWR LED: A red light indicates that 5  V is enabled (only used when a daughterboard is plugged). 

COM LED: refer to the technical note Overview of ST-LINK derivatives (TN1235) at ww.st.com for details. 

# 7.5 Jumper configuration 

Table 3. MB1441 jumper configuration Jumper State Description 

JP1 ON JTAG clock loopback done on board JP2 ON Provides 5 V power on connectors, required for SWIM usage, B-STLINK-VOLT, and B-STLINK-ISOL boards. JP3 OFF STLINK-V3SET reset. It can be used to enforce STLINK-V3SET UsbLoader mode 

Table 4. MB1440 jumper configuration Jumper State Description 

JP1 Not used GND JP2 Not used GND JP3 ON Getting 5 V power from CN12, required for SWIM usage. JP4 OFF Disables SWIM input JP5 ON JTAG clock loopback done on board JP6 OFF Disables SWIM output JP7 OFF Closed to use CAN through CN5 JP8 ON Provides 5 V power to CN7 (internal use) JP9 ON Provides 5 V power to CN10 (internal use) JP10 OFF Closed to enable I 2 C pull-ups JP11 Not used GND JP12 Not used GND UM2448 Rev 9 23/55 

UM2448 Board connectors 

> 54

# 8 Board connectors 

11 user connectors are implemented on the STLINK-V3SET product and are described in this paragraph: 

• 2 user connectors are available on the MB1441 board: – CN1: STDC14 (STM32 JTAG/SWD and VCP) – CN5: USB Micro-B (connection to the host) 

• 9 user connectors are available on the MB1440 board: – CN1: STDC14 (STM32 JTAG/SWD and VCP) – CN2: Legacy Arm 20-pin JTAG/SWD IDC connector – CN3: VCP – CN4: SWIM – CN5: bridge CAN – CN6: SWD – CN7, CN8, CN9: bridge 

Other connectors are reserved for internal use and are not described here. 

# 8.1 Connectors on MB1441 board 8.1.1 USB Micro-B 

The USB connector (CN5) connects the embedded STLINK-V3SET to the PC. 

Figure 15. USB Micro-B connector (CN5) front view 

The related pinout for the USB ST-LINK connector is listed in  Table  5.

Table 5. USB Micro-B connector (CN5) pinout Pin number Pin name Function 

1 VBUS 5 V power 2 DM (D-) USB differential pair M 3 DP (D+) USB differential pair P Board connectors UM2448 

24/55 UM2448 Rev 9 

8.1.2 STDC14 (STM32 JTAG/SWD and VCP) 

The STDC14 connector (CN1) allows the connection to an STM32 target using the JTAG or SWD protocol, respecting (from pin  3 to pin  12) the ARM10 pinout (Arm® Cortex® debug connector). However, it also advantageously provides two UART signals for the Virtual COM port. The related pinout for the STDC14 connector is listed in  Table  6.

# 8.2 Connectors on MB1440 board 8.2.1 STDC14 (STM32 JTAG/SWD and VCP) 

The STDC14 CN1 connector on MB1440 replicates the STDC14 CN1 connector from the MB1441 main module. Refer to  Section  8.1.2  for details. 

4 ID -5 GND GND 

Table 5. USB Micro-B connector (CN5) pinout (continued) Pin number Pin name Function 

Table 6. STDC14 connector (CN1) pinout Pin number Description Pin number Description 

1 Reserved (1) 1. Do not connect to the target. 2 Reserved (1) 3 T_VCC (2) 2. Input for STLINK-V3SET. 4 T_JTMS/T_SWDIO 5 GND 6 T_JCLK/T_SWCLK 7 GND 8 T_JTDO/T_SWO (3) 3. SWO is optional, required only for Serial Wire Viewer (SWV) trace. 9 T_JRCLK (4) /NC (5) 4. Optional loopback of T_JCLK on the target side, required if loopback is removed on the STLINK-V3SET side. 5. NC means not required for the SWD connection. 10 T_JTDI/NC (5) 11 GNDDetect (6) 6. Tied to GND by STLINK-V3SET firmware. It might be used by the target to detect the tool. 12 T_NRST 13 T_VCP_RX (7) 7. Output for STLINK-V3SET 

The used connector is SAMTEC FTSH-107-01-L-DV-K-A. 

14 T_VCP_TX (2) UM2448 Rev 9 25/55 

UM2448 Board connectors 

> 54

8.2.2 Legacy Arm 20-pin JTAG/SWD IDC connector 

The CN2 connector allows the connection to an STM32 target in the JTAG or SWD mode. Its pinout is listed in  Table  7. It is compatible with the pinout of ST-LINK/V2, but the STLINK-

V3SET does not manage the JTAG TRST signal (pin  3). 

8.2.3 Virtual COM port connector 

The CN3 connector allows the connection of a target UART for the Virtual COM port function. The debug connection (through JTAG/SWD or SWIM) is not required simultaneously. However, a GND connection between STLINK-V3SET and the target must be ensured in some other way in case no debug cable is plugged. The related pinout for the VCP connector is listed in  Table  8.

Table 7. Legacy Arm 20-pin JTAG/SWD IDC connector (CN2) pinout Pin number Description Pin number Description 

1 T_VCC (1) 1. Input for STLINK-V3SET. 2 NC 3 NC 4 GND (2) 2. At least one of these pins must be connected to the ground on the target side for correct behavior (connecting all is recommended for noise reduction on the ribbon). 5 T_JTDI/NC (3) 3. NC means not required for the SWD connection. 6 GND (2) 7 T_JTMS/T_SWDIO 8 GND (2) 9 T_JCLK/T_SWCLK 10 GND (2) 11 T_JRCLK (4) /NC (3) 4. Optional loopback of T_JCLK on the target side, required if loopback is removed on the STLINK-V3SET side. 12 GND (2) 13 T_JTDO/T_SWO (5) 5. SWO is optional, required only for Serial Wire Viewer (SWV) trace. 14 GND (2) 15 T_NRST 16 GND (2) 17 NC 18 GND (2) 19 NC 20 GND (2) 

Table 8. Virtual COM port connector (CN3) pinout Pin number Description Pin number Description 

1 T_VCP_TX (1) 1. Input for STLINK-V3SET. Must be connected to UART_TX on target. 2 T_VCP_RX (2) 2. Output for STLINK-V3SET. Must be connected to UART_RX on target. Board connectors UM2448 

26/55 UM2448 Rev 9 

8.2.4 SWIM connector 

The CN4 connector allows the connection to an STM8 SWIM target. The related pinout for the SWIM connector is listed in  Table  9.

8.2.5 CAN connector 

The CN5 connector allows the connection to a CAN target without a CAN transceiver. The related pinout for this connector is listed in  Table  10 .

8.2.6 SWD connector 

The CN6 connector allows the connection to an STM32 target in SWD mode through wires. It is not recommended for high performance. The related pinout for this connector is listed in 

Table  11 .

Table 9. SWIM connector (CN4) pinout Pin number Description 

1 T_VCC (1) 1. Input for STLINK-V3SET. 2 SWIM_DATA 3 GND 4 T_NRST 

Table 10. CAN connector (CN5) pinout 

Pin number Description 1 T_CAN_VCC (1) 1. Input for STLINK-V3SET. 2 T_CAN_TX 3 T_CAN_RX 

Table 11. SWD (wires) connector (CN6) pinout Pin number Description 

1 T_VCC (1) 1. Input for STLINK-V3SET. 2 T_SWCLK 3 GND 4 T_SWDIO 5 T_NRST 6 T_SWO (2) 2. Optional, required only for Serial Wire Viewer (SWV) trace. UM2448 Rev 9 27/55 

UM2448 Board connectors 

> 54

8.2.7 UART/I 2 C/CAN bridge connector 

Some bridge functions are provided on the CN7 2x5-pin 1.27  mm pitch connector. The related pinout is listed in  Table  12 . This connector offers CAN logic signals (Rx/Tx), which 

can be used as an input for an external CAN transceiver. Otherwise, it is preferable to use the MB1440 CN5 connector for the CAN connection. 

8.2.8 SPI/GPIO bridge connector 

Some bridge functions are provided on the CN82x5-pin 1.27  mm pitch connector. The related pinout is listed in  Table  13 .

Table 12. UART bridge connector (CN7) pinout Pin number Description Pin number Description 

1 UART_CTS 2 I2C_SDA 3 UART_TX (1) 1. Tx signals are outputs for STLINK-V3SET and inputs for the target. 4 CAN_TX (1) 5 UART_RX (2) 2. Rx signals are inputs for STLINK-V3SET and outputs for the target. 6 CAN_RX (2) 7 UART_RTS 8 I2C_SCL 9 GND 10 Reserved (3) 3. Do not connect to the target. 

Table 13. SPI bridge connector (CN8) pinout Pin number Description Pin number Description 

1 SPI_NSS 2 Bridge_GPIO0 3 SPI_MOSI 4 Bridge_GPIO1 5 SPI_MISO 6 Bridge_GPIO2 7 SPI_SCK 8 Bridge_GPIO3 9 GND 10 Reserved (1) 1. Do not connect to the target. Board connectors UM2448 

28/55 UM2448 Rev 9 

8.2.9 Bridge 20-pin connector 

All bridge functions are provided on a 2x10-pin connector with a 2.0  mm pitch CN9. The related pinout is listed in  Table  14 .

Table 14. Bridge connector (CN9) pinout Pin number Description Pin number Description 

1 SPI_NSS 11 Bridge_GPIO0 2 SPI_MOSI 12 Bridge_GPIO1 3 SPI_MISO 13 Bridge_GPIO2 4 SPI_SCK 14 Bridge_GPIO3 5 GND 15 Reserved (1) 1. Do not connect to the target. 6 Reserved (1) 16 GND 7 I2C_SCL 17 UART_RTS 8 CAN_RX (2) 2. Rx signals are inputs for STLINK-V3SET and outputs for the target. 18 UART_RX (2) 9 CAN_TX (3) 3. Tx signals are outputs for STLINK-V3SET and inputs for the target. 19 UART_TX (3) 10 I2C_SDA 20 UART_CTS UM2448 Rev 9 29/55 

UM2448 Flat ribbons 

> 54

# 9 Flat ribbons 

The STLINK-V3SET provides three flat cables allowing the connection from the STDC14 output to: 

• STDC14 connector (1.27 mm pitch) on target application: pinout detailed in Table 6 .The connector reference is Samtec FFSD-07-D-05.90-01-N-R. 

• ARM10-compatible connector (1.27 mm pitch) on target application: pinout detailed in 

Table 15 . The connector reference is Samtec ASP-203799-02. 

• ARM20-compatible connector (1.27 mm pitch) on target application: pinout detailed in 

Table 16 . The connector reference is Samtec ASP-203799-02. 

Table 15. ARM10-compatible connector pinout (target side) Pin number Description Pin number Description 

1 T_VCC (1) 1. Input for STLINK-V3SET. 2 T_JTMS/T_SWDIO 3 GND 4 T_JCLK/T_SWCLK 5 GND 6 T_JTDO/T_SWO (2) 2. SWO is optional, required only for Serial Wire Viewer (SWV) trace. 7 T_JRCLK (3) /NC (4) 3. Optional loopback of T_JCLK on the target side, required if loopback is removed on the STLINK-V3SET side. 4. NC means not required for the SWD connection. 8 T_JTDI/NC (4) 9 GNDDetect (5) 5. Tied to GND by STLINK-V3SET firmware. It might be used by the target to detect the tool. 10 T_NRST Flat ribbons UM2448 

30/55 UM2448 Rev 9 

Table 16. ARM20-compatible connector pinout (target side) Pin number Description Pin number Description 

1 T_VCC (1) 1. Input for STLINK-V3SET. 2 T_JTMS/T_SWDIO 3 GND 4 T_JCLK/T_SWCLK 5 GND 6 T_JTDO/T_SWO (2) 2. SWO is optional, required only for Serial Wire Viewer (SWV) trace. 7 T_JRCLK (3) /NC (4) 3. Optional loopback of T_JCLK on the target side, required if loopback is removed on the STLINK-V3SET side. 4. NC means not required for the SWD connection. 8 T_JTDI/NC (4) 9 GNDDetect (5) 5. Tied to GND by STLINK-V3SET firmware. It might be used by the target to detect the tool. 10 T_NRST 11 NC 12 NC 13 NC 14 NC 15 NC 16 NC 17 NC 18 NC 19 NC 20 NC UM2448 Rev 9 31/55 

UM2448 Software configuration 

> 54

# 10 Software configuration 10.1 Supporting toolchains (not exhaustive) 

Table  17  lists the first toolchain version supporting the STLINK-V3SET product. 

Note: Some of the very first toolchain versions supporting the STLINK-V3SET (in runtime) might not install the complete USB driver for STLINK-V3SET (especially the STLINK-V3SET bridge USB interface description might miss). In this case, whether the user upgrades to a more recent version of the toolchain or updates the ST-LINK driver from www.st.com (refer to  Section  10.2 ). 

# 10.2 Drivers and firmware upgrade 

The STLINK-V3SET requires drivers to be installed on Windows and embeds firmware that needs to be updated occasionally to benefit from new functionality or corrections. Refer to the technical note Overview of ST-LINK derivatives (TN1235) at www.st.com for details. 

# 10.3 STLINK-V3SET frequency selection 

The STLINK-V3SET can run internally at 3 different frequencies: 

• High-performance frequency 

• Standard frequency, compromising between performance and consumption 

• Low-consumption frequency 

By default, the STLINK-V3SET starts at a high-performance frequency. It is the responsibility of the toolchain provider to propose or not the frequency selection at the user level. 

# 10.4 Mass-storage interface 

The STLINK-V3SET implements a virtual mass-storage interface allowing the programming of an STM32 target flash memory with the drag-and-drop action of a binary file from a file explorer. This ability requires the STLINK-V3SET to identify the connected target before 

Table 17. Toolchain versions supporting STLINK-V3SET Toolchain Description Minimum Version 

STM32CubeProgrammer ST Programming tool for ST microcontrollers 1.1.0 SW4STM32 Free IDE on Windows, Linux, and macOS 2.4.0 IAR EWARM Third-party debugger for STM32 8.20 Keil MDK-ARM Third-party debugger for STM32 5.26 STVP ST Programming tool for ST microcontrollers 3.4.1 STVD ST Debugging tool for STM8 4.3.12 Software configuration UM2448  

> 32/55 UM2448 Rev 9

enumerating it on the USB host. Consequently, this functionality is available only if the target is connected to the STLINK-V3SET before the STLINK-V3SET is plugged into the host. This functionality is not available for STM8 targets. 

The ST-LINK firmware programs the binary dropped file at the beginning of the flash area, only if it is detected as a valid STM32 application according to the following criteria: 

• The reset vector points to an address in the target flash area, 

• The stack pointer vector points to an address in any of the target RAM areas. 

If all these conditions are not respected, the binary file is not programmed and the target flash memory keeps its initial contents. 

# 10.5 Bridge interface 

The STLINK-V3SET implements a USB interface dedicated to the bridging functions from USB to SPI/I 2C/CAN/UART/GPIOs of the ST microcontroller target. This interface is first used by STM32CubeProgrammer to allow target programming through the SPI/I 2C/CAN bootloader. 

A host software API is provided to extend the use cases. UM2448 Rev 9 33/55 

UM2448 B-STLINK-VOLT board extension description 

> 54

# 11 B-STLINK-VOLT board extension description 11.1 Features 

• 1.65 V to 3.3 V voltage adapter board for STLINK-V3SET 

• Input/output level shifters for STM32 SWD/SWV/JTAG signals 

• Input/output level shifters for VCP Virtual COM port signals (UART) 

• Input/output level shifters for bridge signals (SPI/UART/I 2 C/CAN/GPIOs) 

• Closed casing when using STDC14 connector (STM32 SWD, SWV, and VCP) 

• Connection compatible with STLINK-V3SET adapter board (MB1440) for STM32 JTAG and bridge 

# 11.2 Connection instructions 11.2.1 Closed casing for STM32 debug 

(STDC14 connector only) with B-STLINK-VOLT 

1. Remove the USB cable from STLINK-V3SET. 

2. Unscrew the casing bottom cover of the STLINK-V3SET or remove the adapter board (MB1440). 

3. Remove the JP1 jumper from the MB1441 main module and place it on the JP1 header of the MB1598 board. 

4. Put the plastic edge in place to guide the B-STLINK-VOLT board connection to the STLINK-V3SET main module (MB1441). 

5. Connect the B-STLINK-VOLT board to the STLINK-V3SET main module (MB1441). 

6. Close the casing bottom cover. B-STLINK-VOLT board extension description UM2448  

> 34/55 UM2448 Rev 9

Figure 16. Closed casing for STM32 debug with B-STLINK-VOLT 

The STDC14 CN1 connector on the B-STLINK-VOLT board replicates the STDC14 CN1 connector from the MB1441 main module. Refer to  Section  8.1.2  for details. UM2448 Rev 9 35/55 

UM2448 B-STLINK-VOLT board extension description 

> 54

11.2.2 Opened casing for access to all connectors 

(through MB1440 adapter board) with B-STLINK-VOLT 

1. Remove the USB cable from STLINK-V3SET. 

2. Unscrew the casing bottom cover of the STLINK-V3SET or remove the adapter board (MB1440). 

3. Remove the JP1 jumper from the MB1441 main module and place it on the JP1 header of the MB1598 board. 

4. Put the plastic edge in place to guide the B-STLINK-VOLT board connection to the STLINK-V3SET main module (MB1441). 

5. Connect the B-STLINK-VOLT board to the STLINK-V3SET main module (MB1441). 

6. [optional] Screw the B-STLINK-VOLT board to ensure good and stable contact. 

7. Plug the MB1440 adapter board into the B-STLINK-VOLT board in the same way that it was previously plugged into the STLINK-V3SET main module (MB1441). B-STLINK-VOLT board extension description UM2448  

> 36/55 UM2448 Rev 9

Figure 17. Opened casing for access to all connectors with B-STLINK-VOLT 

For connector description, refer to  Section  8.2 .

# 11.3 Selection of bridge GPIO direction 

The level-shifter components on the B-STLINK-VOLT board require manually configuring the direction of bridge GPIO signals. This is possible through the SW1 switch on the bottom of the board. Pin  1 of SW1 is for bridge GPIO0 and pin  4 of SW1 is for bridge GPIO3. By default, the direction is the target output/ST-LINK input (selectors on the ON/CTS3 side of UM2448 Rev 9 37/55 

UM2448 B-STLINK-VOLT board extension description 

> 54

SW1). It can be changed for each GPIO independently in the direction of the target input/ST-LINK output by moving the corresponding selector to sides 1, 2, 3, or 4 of SW1. Refer to  Figure  18 .

Figure 18. B-STLINK-VOLT 

# 11.4 Jumper configuration Caution: Always remove the JP1 jumper from the STLINK-V3SET main module (MB1441) before stacking the B-STLINK-VOLT board (MB1598). This jumper can be used on the MB1598 board to provide the return JTAG clock required for correct JTAG operations. If the JTAG clock loopback is not done at the B-STLINK-VOLT board level through JP1, it must be done externally between CN1 pins 6 and 9. 

# 11.5 Target voltage connection 

The target voltage must always be provided to the board for proper operation (input for 

B-STLINK-VOLT). It must be provided to pin  3 of the CN1 STDC14 connector, either directly on MB1598 or through the MB1440 adapter board. In case of use with the MB1440 adapter board, the target voltage can be provided either through pin  3 of CN1, pin  1 of CN2, pin  1 of CN6, or pins  2 and 3 of JP10 on the MB1440 board. The expected range is 1.65  to  3.3  V. 

Table 18. MB1598 jumper configuration Jumper State Description   

> JP1 ON JTAG clock loopback done on board

B-STLINK-VOLT board extension description UM2448  

> 38/55 UM2448 Rev 9

# 11.6 Board connectors 11.6.1 STDC14 (STM32 JTAG/SWD and VCP) 

The STDC14 CN1 connector on the MB1598 board replicates the STDC14 CN1 connector from the MB1441 board. Refer to  Section  8.1.2  for details. 

11.6.2 UART/I 2 C/CAN bridge connector 

The UART/I 2C/CAN bridge CN7 connector on the MB1598 board replicates the UART/I 2C/CAN bridge CN7 connector from the MB1440 board. Refer to  Section  8.2.7  for 

details. 

11.6.3 SPI/GPIO bridge connector 

The SPI/GPIO bridge CN8 connector on the MB1598 board replicates the SPI/GPIO bridge CN8 connector from the MB1440 board. Refer to  Section  8.2.8  for details. UM2448 Rev 9 39/55 

UM2448 B-STLINK-ISOL board extension description 

> 54

# 12 B-STLINK-ISOL board extension description 12.1 Features 

• 1.65 V to 3.3 V voltage adapter and galvanic isolation board for STLINK-V3SET 

• 2.5 kV RMS galvanic isolation 

• Input/output isolation and level shifters for STM32 SWD/SWV/JTAG signals 

• Input/output isolation and level shifters for VCP Virtual COM port signals (UART) 

• Input/output isolation and level shifters for bridge signals (SPI/UART/I 2 C/CAN/GPIOs) 

• Closed casing when using STDC14 connector (STM32 SWD, SWV, and VCP) 

• Connection compatible with STLINK-V3SET adapter board (MB1440) for STM32 JTAG and bridge 

# 12.2 Connection instructions 12.2.1 Closed casing for STM32 debug 

(STDC14 connector only) with B-STLINK-ISOL 

1. Remove the USB cable from STLINK-V3SET. 

2. Unscrew the casing bottom cover of the STLINK-V3SET or remove the adapter board (MB1440). 

3. Remove the JP1 jumper from the MB1441 main module and place it on the JP2 header of the MB1599 board. 

4. Put the plastic edge in place to guide the B-STLINK-ISOL board connection to the STLINK-V3SET main module (MB1441). 

5. Connect the B-STLINK-ISOL board to the STLINK-V3SET main module (MB1441). 

6. Close the casing bottom cover. B-STLINK-ISOL board extension description UM2448  

> 40/55 UM2448 Rev 9

Figure 19. Closed casing for STM32 debug with B-STLINK-ISOL 

The STDC14 CN1 connector on the B-STLINK-ISOL board replicates the STDC14 CN1 connector from the MB1441 main module. Refer to  Section  8.1.2  for details. 

12.2.2 Opened casing for access to all connectors 

(through MB1440 adapter board) with B-STLINK-ISOL 

1. Remove the USB cable from STLINK-V3SET 

2. Unscrew the casing bottom cover of the STLINK-V3SET or remove the adapter board (MB1440) 

3. Remove the JP1 jumper from the MB1441 main module and place it on the JP2 header of the MB1599 board 

4. Put the plastic edge in place to guide the B-STLINK-ISOL board connection to the STLINK-V3SET main module (MB1441) 

5. Connect the B-STLINK-ISOL board to the STLINK-V3SET main module (MB1441) UM2448 Rev 9 41/55 

UM2448 B-STLINK-ISOL board extension description 

> 54

Caution: Do not screw the B-STLINK-ISOL board to the STLINK-V3SET main module with a metal screw. Any contact of the MB1440 adapter board with this screw short-circuits the grounds and might cause damage. 

6. Plug the MB1440 adapter board into the B-STLINK-ISOL board in the same way that it was previously plugged into the STLINK-V3SET main module (MB1441) 

Figure 20. Opened casing for access to all connectors with B-STLINK-ISOL 

For connector description, refer to  Section  8.2 .

A metal screw is forbidden. Use only nylon fastener screws or do not screw. Risk of damage B-STLINK-ISOL board extension description UM2448  

> 42/55 UM2448 Rev 9

# 12.3 Bridge GPIO direction 

On the B-STLINK-ISOL board, the direction of bridge GPIO signals is fixed by hardware: 

• GPIO0 and GPIO1 are the target input and ST-LINK output. 

• GPIO2 and GPIO3 are the target output and ST-LINK input. 

# 12.4 Jumper configuration 

Jumpers on the B-STLINK-ISOL board (MB1599) are used to configure the return JTAG clock path required for correct JTAG operations. The highest is the JTAG clock frequency, the closest to the target must be the loopback. 

1. Loopback is done at STLINK-V3SET main module (MB1441) level: MB1441 JP1 is ON, while MB1599 JP2 is OFF. 

2. Loopback is done at B-STLINK-ISOL board (MB1599) level: MB1441 JP1 is OFF (very important potentially not to degrade the MB1599 board), while MB1599 JP1 and JP2 are ON. 

3. Loopback is done at the target level: MB1441 JP1 is OFF (very important not potentially to degrade the MB1599 board), MB1599 JP1 is OFF and JP2 is ON. Loopback is done externally between CN1 pins  6 and 9. 

Caution: Always ensure that either the JP1 jumper from the STLINK-V3SET main module (MB1441), or the JP2 jumper from the B-STLINK-ISOL board (MB1599) is OFF, before stacking them. 

# 12.5 Target voltage connection 

The target voltage must always be provided to the board to work correctly (input for B-STLINK-ISOL). It must be provided to pin  3 of the CN1 STDC14 connector, either directly on MB1599 or through the MB1440 adapter board. In case of use with the MB1440 adapter board, the target voltage can be provided either through pin  3 of CN1, pin  1 of CN2, pin  1 of CN6, or pin  2 and pin  3 of JP10 of the MB1440 board. The expected range is 1.65 to 3.3  V. 

# 12.6 Board connectors 12.6.1 STDC14 (STM32 JTAG/SWD and VCP) 

The STDC14 CN1 connector on the MB1599 board replicates the STDC14 CN1 connector from the MB1441 main module. Refer to  Section  8.1.2  for details. 

12.6.2 UART/I 2 C/CAN bridge connector 

The UART/I2C/CAN bridge CN7 connector on the MB1599 board replicates the UART/I2C/CAN bridge CN7 connector from the MB1440 board. Refer to  Section  8.2.7  for 

details. 

12.6.3 SPI/GPIO bridge connector 

The SPI/GPIO bridge CN8 connector on the MB1599 board replicates the SPI/GPIO bridge CN8 connector from the MB1440 board. Refer to  Section  8.2.8  for details. UM2448 Rev 9 43/55 

UM2448 Performance figures 

> 54

# 13 Performance figures 13.1 Global overview 

Table  19  gives an overview of the achievable maximal performances with the STLINK-

V3SET on different communication channels. Those performances are also depending on the overall system context (target included), so they are not guaranteed to be always reachable. For instance, a noisy environment or the connection quality can impact system performance. 

# 13.2 Baud rate computing 

Some interfaces (VCP and SWV) are using the UART protocol. In that case, the baud rate of STLINK-V3SET must be aligned as much as possible with the target one. 

Below is a rule allowing the user to compute the baud rate achievable by the STLINK-V3SET probe: 

• In high-performance mode: 384 MHz/prescaler with prescaler = [24 to 31] then 192 MHz / prescaler with prescaler = [16 to 65535] 

• In standard mode: 192 MHz/prescaler with prescaler = [24 to 31] then 96 MHz / prescaler with prescaler = [16 to 65535] 

• In low consumption mode: 96 MHz/prescaler with prescaler = [24 to 31] then 48 MHz / prescaler with prescaler = [16 to 65535] 

Note that the UART protocol does not guarantee data delivery (all the more without hardware flow control). Consequently, at high frequencies, the baud rate is not the only parameter impacting the data integrity. The line load rate and the capability of the receiver to process all the data also affect the communication. With a heavily loaded line, data loss might occur at the STLINK-V3SET side above 12  MHz. 

Table 19. Achievable maximal performance with STLINK-V3SET on different channels Board Target voltage Maximum frequency (in MHz) SWD JTAG SWV VCP SPI I 2 C CAN                                         

> STLINK-V3SET 3.3 24 21 16 16 24 11STLINK-V3SET + B-STLINK-VOLT 3.3 812 12 10 12 11STLINK-V3SET + B-STLINK-VOLT 1.8 8812 10 12 11STLINK-V3SET + B-STLINK-ISOL 3.3 8812 10 611STLINK-V3SET + B-STLINK-ISOL 1.8 8812 10 611

Product information UM2448 

44/55 UM2448 Rev 9 

# 14 Product information 14.1 Product marking 

The product and each board composing the product are identified with one or several stickers. The stickers, located on the top or bottom side of each PCB, provide product information: 

• Main board featuring the target device: product order code, product identification, serial number, and board reference with revision. 

Single-sticker example: 

Dual-sticker example: 

• Other boards if any: board reference with revision and serial number. 

Examples: 

On the main board sticker, the first line provides the product order code, and the second line the product identification. 

On all board stickers, the line formatted as “MBxxxx-Variant-yzz” shows the board reference 

“MBxxxx” , the mounting variant “Variant” when several exist (optional), the PCB revision “y” ,and the assembly revision “zz” , for example B01. The other line shows the board serial number used for traceability. 

Products and parts labeled as “ES” or “E” are not yet qualified or feature devices that are not yet qualified. STMicroelectronics disclaims any responsibility for consequences arising from their use. Under no circumstances will STMicroelectronics be liable for the customer's use of these engineering samples. Before deciding to use these engineering samples for qualification activities, contact STMicroelectronics' quality department. 

“ES” or “E” marking examples of location: 

• On the targeted STM32 that is soldered on the board (for an illustration of STM32 marking, refer to the STM32 datasheet Package information paragraph at the 

www.st.com website). 

• Next to the evaluation tool ordering part number that is stuck or silk-screen printed on the board. 

Some boards feature a specific STM32 device version, which allows the operation of any bundled commercial stack/library available. This STM32 device shows a “U” marking option at the end of the standard part number and is not available for sales. 

To use the same commercial stack in their applications, the developers might need to purchase a part number specific to this stack/library. The price of those part numbers includes the stack/library royalties.  DT73589V1 Product order code Product identification syywwxxxxx MBxxxx-Variant-yzz   

> Product order code Product identification
> Product order code Product identification MBxxxx-Variant-yzz syywwxxxxx
> and
> or MBxxxx-Variant-yzz syywwxxxxx syywwxxxxx MBxxxx-Variant-yzz
> or or

UM2448 Rev 9 45/55 

UM2448 Product information 

> 54

# 14.2 Product history 

Table 20. Product history Order code Product identification Product details Product change description Product limitations 

> STLINK-V3SET

LKV3SET$AT1 Boards: – MB1441-DEFAULT-B01 

(main module) – MB1440-DEFAULT-B01 

(adapter board) Initial revision No limitation LKV3SET$AT2 Boards: – MB1441-DEFAULT-B01 

(main module) – MB1440-DEFAULT-B01 

(adapter board) Added cable for bridge signals out of the CN9 MB1440 adapter board connector No limitation LKV3SET$AT3 Boards: – MB1441-DEFAULT-B01 

(main board) – MB1440-DEFAULT-B01 

(adapter board) Replaced metal screws with nylon screws No limitation LKV3SET$AT4 Boards: – MB1441-DEFAULT-B01 

(main module) – MB1440-DEFAULT-C01 

(adapter board) Adapter board revision changed No limitation LKV3SET$RT1 Boards: – MB1441-DEFAULT-B02 

(main module) – MB1440-DEFAULT-C01 

(adapter board) – Main module revision changed – Board stickers format changed No limitation B-STLINK-VOLT BSTLINKVOLT$AZ1 Board: – MB1598-DEFAULT-A01 

(voltage adapter board) Initial revision No limitation BSTLINKVOLT$AZ2 Board: – MB1598-DEFAULT-A01 

(voltage adapter board) – New packaging – New welcome letter – Replaced metal pillars and washer with nylon pillars and washer No limitation BSTLINKVOLT$RZ1 Board: – MB1598-DEFAULT-A01 

(voltage adapter board) – Board stickers format changed No limitation Product information UM2448 

46/55 UM2448 Rev 9 

> B-STLINK-ISOL

BSTLINKISOL$AZ1 Board: – MB1599-DEFAULT-B01 

(voltage adapter and galvanic isolation board) Initial revision Do not screw the B-STLINK-ISOL board to the STLINK-V3SET main module with a metal screw, especially if you intend to use the MB1440 adapter board. Any contact of the MB1440 adapter board with this screw short-circuits the grounds and might cause damage. Use only nylon fastener screws or do not screw. BSTLINKISOL$AZ2 Board: – MB1599-DEFAULT-B01 

(voltage adapter and galvanic isolation board) – Replaced metal pillars and washer with nylon pillars and washer – New packaging No limitation BSTLINKISOL$AZ3 Board: – MB1599-DEFAULT-B02 

(voltage adapter and galvanic isolation board) Voltage adapter and galvanic isolation board revision changed No limitation BSTLINKISOL$RZ1 Board: – MB1599-DEFAULT-B03 

(voltage adapter and galvanic isolation board) – Voltage adapter and galvanic isolation board revision changed – Board stickers format change No limitation 

Table 20. Product history (continued) Order code Product identification Product details Product change description Product limitations UM2448 Rev 9 47/55 

UM2448 Product information 

> 54

# 14.3 Board revision history 

Table 21. Board revision history Board reference Board variant and revision Board change description Board limitations 

MB1441 

(main module) DEFAULT-B01 Initial revision No limitation DEFAULT-B02 Several part references are updated due to obsolescence. No limitation MB1440 

(adapter board) DEFAULT-B01 Initial revision Care must be taken if used with MB1599 board revision B-01: refer to MB1599 board revision B-01 limitations. DEFAULT-C01 The revision C-01 removes the metal layer generating shortcut when it is screwed on a MB1599 B-01 board with metal screws. No limitation MB1598 

(voltage adapter board) DEFAULT-A01 Initial revision The target voltage cannot be provided through the bridge connectors CN7 and CN8 while required for bridge functions. The target voltage must be provided either through CN1 or through the MB1440 adapter board (refer to Section 11.5: Target voltage connection ). Product information UM2448 

48/55 UM2448 Rev 9 

MB1599 

(voltage adapter and galvanic isolation board) DEFAULT-B01 Initial revision The target voltage cannot be provided through the bridge connectors CN7 and CN8 while required for bridge functions. The target voltage must be provided either through CN1 or through the MB1440 adapter board (refer to Section 12.5: Target voltage connection ). Do not screw the B-STLINK-ISOL board to the STLINK-V3SET main module with a metal screw, especially if you intend to use the MB1440 adapter board. Any contact of the MB1440 adapter board with this screw short circuits the grounds and might cause damage. Use only nylon fastener screws or do not screw. DEFAULT-B02 Several part references updated due to obsolescence. The target voltage cannot be provided through the bridge connectors CN7 and CN8 while required for bridge functions. The target voltage must be provided either through CN1 or through the MB1440 adapter board (refer to Section 12.5: Target voltage connection ). DEFAULT-B03 Several part references updated due to obsolescence. The target voltage cannot be provided through the bridge connectors CN7 and CN8 while required for bridge functions. The target voltage must be provided either through CN1 or through the MB1440 adapter board (refer to Section 12.5: Target voltage connection ). 

Table 21. Board revision history (continued) Board reference Board variant and revision Board change description Board limitations UM2448 Rev 9 49/55 

UM2448 Safety certification information 

> 54

# 15 Safety certification information 

The product must be powered by a power supply unit or auxiliary equipment complying with standard EN 62368-1:2014+A1:2017 or the standard replacing it, and must be safety extralow voltage (SELV) with limited power capability. Federal Communications Commission (FCC) and ISED Canada Compliance Statements UM2448  

> 50/55 UM2448 Rev 9

# 16 Federal Communications Commission (FCC) and ISED Canada 

# Compliance Statements 16.1 FCC Compliance Statement Part 15.19 

This device complies with Part 15 of the FCC Rules. Operation is subject to the following two conditions: (1) this device may not cause harmful interference, and (2) this device must accept any interference received, including interference that may cause undesired operation. 

Part 15.21 

Any changes or modifications to this equipment not expressly approved by STMicroelectronics may cause harmful interference and void the user's authority to operate this equipment. 

Part 15.105 

This equipment has been tested and found to comply with the limits for a Class B digital device, pursuant to part 15 of the FCC Rules. These limits are designed to provide reasonable protection against harmful interference in a residential installation. This equipment generates uses and can radiate radio frequency energy and, if not installed and used in accordance with the instructions, may cause harmful interference to radio communications. However, there is no guarantee that interference will not occur in a particular installation. If this equipment does cause harmful interference to radio or television reception which can be determined by turning the equipment off and on, the user is encouraged to try to correct interference by one or more of the following measures: 

• Reorient or relocate the receiving antenna. 

• Increase the separation between the equipment and receiver. 

• Connect the equipment into an outlet on a circuit different from that to which the receiver is connected. 

• Consult the dealer or an experienced radio/TV technician for help. 

Note: Use a shielded USB cable with a length of less than 0.5 m and ferrite on the computer side, like WURTH ELEKTRONIK 742 711 12. 

Responsible party (in the USA) 

Francesco Doddo 

STMicroelectronics, Inc. 

200 Summit Drive | Suite 405 | Burlington, MA 01803 

USA 

Telephone: +1 781-472-9634 UM2448 Rev 9 51/55 

UM2448 Federal Communications Commission (FCC) and ISED Canada Compliance Statements 

> 54

# 16.2 ISED Compliance Statement 

ISED Canada ICES-003 Compliance Label: CAN ICES-3 (B) / NMB-3 (B). 

Étiquette de conformité à la NMB-003 d'ISDE Canada : CAN ICES-3 (B) / NMB-3 (B). 

Note: Use a shielded USB cable with a length of less than 0.5 m and ferrite on the computer side, like WURTH ELEKTRONIK 742 711 12. Product disposal UM2448 

52/55 UM2448 Rev 9 

# 17 Product disposal 

Disposal of this product: WEEE (Waste Electrical and Electronic Equipment) 

(Applicable in Europe) 

This symbol on the product, accessories, or accompanying documents indicates that the product and its electronic accessories should not be disposed of with household waste at the end of their working life. 

To prevent possible harm to the environment and human health from uncontrolled waste disposal, please separate these items from other type of waste and recycle them responsibly to the designated collection point to promote the sustainable reuse of material resources. 

Household users: 

You should contact either the retailer where you buy the product or your local authority for further details of your nearest designated collection point. 

Business users: 

You should contact your dealer or supplier for further information. UM2448 Rev 9 53/55 

UM2448 Revision history 

> 54

# Revision history 

Table 22. Document revision history Date Revision Changes 

6-Sep-2018 1 Initial release. 8-Feb-2019 2Updated: – Section 8.3.4: Virtual COM port (VCP) ,– Section 8.3.5: Bridge functions ,– Section 9.1.2: STDC14 (STM32 JTAG/SWD and VCP) , and – Section 9.2.3: Virtual COM port connector explaining how Virtual COM ports are connected to the target. 20-Nov-2019 3Added: – Second Virtual COM port chapter in Introduction ,– Figure 13 in Section 8.3.5 Bridge UART, and – Figure 15 in the new section of Mechanical information .19-Mar-2020 4Added: – Section 12: B-STLINK-VOLT board extension description. 

5-Jun-2020 5Added: – Section 12.5: Target voltage connection and – Section 12.6: Board connectors .Updated: – Section 1: Features ,– Section 3: Ordering information ,– Section 8.2.7: UART/I 2C/CAN bridge connector , and – Section 13: STLINK-V3SET and B-STLINK-VOLT information .5-Feb-2021 6Added: – Section 13: B-STLINK-ISOL board extension description ,– Figure 19 and Figure 20 , and – Section 14: Performance figures .Updated: – Introduction ,– Ordering information ,– Figure 16 and Figure 17 , and – Section 15: STLINK-V3SET, B-STLINK-VOLT, and B-STLINK-ISOL information .All modifications linked to the latest B-STLINK-ISOL board for voltage adaptation and galvanic isolation Revision history UM2448 

54/55 UM2448 Rev 9 

7-Dec-2021 7Added: – Section 15.2.2: Product identification LKV3SET$AT2 

and – Reminder not to use metal screws to avoid damages in Figure 20 , Section 15.4.1 , and Section 15.5.4 .Updated: – Features ,– System requirements , and – Section 7.3.4: Virtual COM port (VCP) .28-Nov-2023 8Added: – Product history and Board revision history tables Updated: – Product marking 

17-Apr-2025 9Added: – STDC14, ARM10-compatible, and ARM20-compatible connector references – Safety certification information 

– Product disposal 

Updated: – Ordering information , Product history , and Board revision history tables – Product marking 

– FCC Compliance Statement and ISED Compliance Statement 

Table 22. Document revision history (continued) Date Revision Changes UM2448 Rev 9 55/55 

UM2448 

> 55

IMPORTANT NOTICE – READ CAREFULLY 

STMicroelectronics NV and its subsidiaries (“ST”) reserve the right to make changes, corrections, enhancements, modifications, and improvements to ST products and/or to this document at any time without notice. Purchasers should obtain the latest relevant information on ST products before placing orders. ST products are sold pursuant to ST’s terms and conditions of sale in place at the time of order acknowledgment. 

Purchasers are solely responsible for the choice, selection, and use of ST products and ST assumes no liability for application assistance or the design of purchasers’ products. 

No license, express or implied, to any intellectual property right is granted by ST herein. 

Resale of ST products with provisions different from the information set forth herein shall void any warranty granted by ST for such product. 

ST and the ST logo are trademarks of ST. For additional information about ST trademarks, refer to www.st.com/trademarks. All other product or service names are the property of their respective owners. 

Information in this document supersedes and replaces information previously supplied in any prior versions of this document. 

© 2025 STMicroelectronics – All rights reserved
