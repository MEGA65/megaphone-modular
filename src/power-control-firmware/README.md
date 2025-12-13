# MEGAphone Power Control firmware

For low-power operation the MEGAphone uses an IceSugar Nano Lattice-based FPGA to
monitor the cellular modem for important events (RING for incoming calls, and +QIND
for incoming SMS and other configurable events).

The firmware intentionally does very little, so as to use as little power as possible.

Essentially it relays the UART from the cellular modem to the main FPGA (if it is turned on),
and allows the main FPGA to request power to be turned on and off as required.

It does one extra thing that allows the main FPGA to be turned off most of the time:
Monitoring the cellular modem for those events mentioned above.

The end results fits in a ~1K LC FPGA.


# MEGAphone Power Control Interface

(The following API documentation was generated with the assistance of AI)

## Overview

The MEGAphone Power Control Interface provides a simple UART-based protocol and a small C library for controlling power to MEGAphone sub-systems.

A dedicated low-power FPGA (iCESugar Nano) manages:

- Power enable lines for up to six DC:DC converters
- Wake-up behavior triggered by:
  - Power button presses
  - Cellular modem events (RING, +QIND)
- Logging of cellular modem events while the main FPGA is powered down

The interface is intentionally minimal to fit within tight FPGA resource constraints and to support low-level system reliability.

---

## Hardware Architecture Summary

- Low-power FPGA: iCESugar Nano
- Power control outputs: 6 GPIOs (one per DC:DC enable)
- UART interfaces:
  - Management UART (shared with USB UART for debugging)
  - Cellular modem UART RX (with pass-through to main FPGA)
- Wake sources:
  - Cellular modem events (RING, +QIND)
  - Power button (short press = power on, long press = power off)
- Optional peripherals:
  - I²C IO expanders (buttons, indicators)

Electrical isolation and board-level design are outside the scope of this interface.

---

## UART Protocol Specification

All communication occurs over an 8-bit UART at 115200 baud (default).

### General Rules

- Status bytes always have bit 7 set (0x80–0xFF)
- Text and data streams use 7-bit ASCII and are terminated by a NUL byte (0x00)
- Commands are single bytes
- No framing, checksums, or escaping are used

This simplicity is intentional.

---

## Commands (Host → FPGA)

### Power Status

Command: .  
Byte: 0x2E  
Description: Request current power status byte

---

### Power Control (Circuits 0–5)

Circuit | OFF byte | ON byte  
0 | 0x20 | 0x30  
1 | 0x21 | 0x31  
2 | 0x22 | 0x32  
3 | 0x23 | 0x33  
4 | 0x24 | 0x34  
5 | 0x25 | 0x35  

- Bit 4 of the command byte determines ON (1) or OFF (0)
- After processing a command, the FPGA reports a new status byte

---

### Configuration Dump

Command: ?  
Byte: 0x3F  

- Sends human-readable ASCII lines
- Terminates with a NUL byte
- Includes:
  - Firmware version
  - Circuit count
  - Per-circuit descriptive fields

---

### Cellular Event Log

Command | Byte | Description  
P | 0x50 | Play back cellular event log  
X | 0x58 | Clear cellular event log  

- Playback is ASCII text terminated by a NUL byte
- Used to capture RING and +QIND activity while powered down

---

### Cellular UART Baud Rate

Baud Rate | Command  
2000000 | A  
1000000 | B  
230400 | C  
115200 | D  
19200 | E  
9600 | F  
2400 | G  

Incorrect baud settings will disable wake-on-cellular events.

---

## Status Byte Format (FPGA → Host)

The status byte always has bit 7 set.

Bit | Meaning  
0–5 | Power state of circuits 0–5 (1 = ON)  
6 | Cellular events recorded  
7 | Always 1 (marks byte as status)

---

## C Library API

The C library wraps the UART protocol for use on:

- MEGA65 / MEGAphone
- Linux (debugging and development)

### Low-Level I/O (Platform Provided)

    int powerctl_uart_write(uint8_t *buffer, uint16_t size);
    uint16_t powerctl_uart_read(uint8_t *buffer, uint16_t size);

---

### Core Power Management

Synchronize with the FPGA and return the current status byte:

    uint8_t powerctl_sync(void);

---

Turn a circuit on or off:

    char powerctl_switch_circuit(uint8_t circuit_id, char on_off);

Returns 0 on success, 0xFF on failure.

---

Get the number of supported power circuits:

    char powerctl_get_circuit_count(void);

---

### Configuration Enumeration

Begin reading the configuration stream:

    char powerctl_start_read_config(void);

---

Retrieve a configuration field for a circuit:

    char powerctl_getconfig(
        char circuit_id,
        char field_id,
        unsigned char *out,
        uint8_t out_len,
        int mode
    );

Supported field identifiers:

- FIELD_CIRCUITNAME
- FIELD_ONCHAR
- FIELD_OFFCHAR
- FIELD_STATUSBITMASK

---

Find a circuit by matching its name:

    char powerctl_find_circuit_by_name(char *string);

---

### Cellular Event Log

Clear the stored cellular event log:

    void powerctl_cellog_clear(void);

---

Retrieve the cellular event log:

    uint16_t powerctl_cellog_retrieve(uint8_t *out, uint16_t buf_len);

---

Set the expected cellular modem UART baud rate:

    char powerctl_cel_setbaud(uint32_t speed);

---

## Command-Line Utility

The standalone utility supports the following commands:

config  
status  
+<n|name>  
-<n|name>  
celspeed=<baud>  
celplay  
celclear  

Examples:

    powerctl /dev/ttyUSB0 115200 status
    powerctl /dev/ttyUSB0 115200 +2
    powerctl /dev/ttyUSB0 115200 -main
    powerctl /dev/ttyUSB0 115200 celplay celclear

---

## Design Notes

- The protocol is intentionally stateless and byte-oriented
- No retry, checksum, or framing is used
- The FPGA autonomously handles wake conditions
- The C API favors small code size over abstraction

This design reflects the tight resource constraints of the low-power FPGA and the need for robust, predictable system behavior.
