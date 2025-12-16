# MEGAphone SMS API Documentation

(AI generated summary of the API.  Read https://c65gs.blogspot.com.au for more details.)

**Platform:** MEGA65 / MEGAphone  
**Language:** C (embedded)  
**Transport:** Cellular modem (AT commands over UART)  
**SMS Mode:** GSM PDU  
**Memory Model:** No dynamic allocation

This document describes the SMS-related APIs implemented in the MEGAphone codebase.

---

## Files Covered

- `modem.h` / `modem.c` — High-level modem and SMS control
- `smsencode.c` — Outgoing SMS PDU construction
- `smsdecode.h` / `smsdecode.c` — Incoming SMS PDU decoding
- `utf.c` — UTF-8 / Unicode utilities

---

## Architecture Overview

```
Application
    |
    v
modem.c        (AT command control, SMS orchestration)
    |
    v
smsencode.c   (TX: PDU construction)
smsdecode.c   (RX: PDU parsing)
    |
    v
utf.c         (UTF-8 ↔ Unicode conversion)
    |
    v
Cellular Modem
```

---

## Design Goals

- No heap allocation
- Robust handling of malformed SMS PDUs
- Debug-heavy error reporting
- Designed for real-world modem quirks
- Suitable for tight polling loops on MEGA65

---

## modem.h — Modem & SMS Control API

### Initialization and Polling

```
char modem_init(void);
char modem_poll(void);
void modem_parse_line(void);
```

#### modem_init()

Initializes modem state and configures SMS operation.

- Sets SMS mode to PDU
- Clears internal buffers
- Enables unsolicited result handling

Return values:

- `0` — Success  
- non-zero — Initialization failure

---

#### modem_poll()

Main modem service function.

Must be called frequently from the main loop.

Handles:
- Incoming UART data
- AT command responses
- Incoming SMS notifications
- Call state changes

Return values:

- `0` — No new events  
- non-zero — Event occurred (implementation-defined)

---

#### modem_parse_line()

Parses a completed modem response line.

Normally called internally by `modem_poll()`.

---

### Voice Call Control

```
void modem_place_call(void);
void modem_answer_call(void);
void modem_hangup_call(void);
void modem_mute_call(void);
void modem_unmute_call(void);
void modem_toggle_mute(void);
```

These functions issue standard AT commands for call handling.

Mute control is stateful and depends on active call state.

---

### SMS Management

```
uint16_t modem_get_sms_count(void);
char modem_get_sms(uint16_t sms_number);
char modem_delete_sms(uint16_t sms_number);
uint16_t modem_get_oldest_sms(void);
```

#### modem_get_sms_count()

Returns the number of SMS messages currently stored on the modem.

---

#### modem_get_sms(uint16_t sms_number)

Requests an SMS by index from modem storage.

- Issues `AT+CMGR`
- Triggers PDU decoding via `smsdecode.c`

Return values:

- `0` — Success  
- non-zero — Invalid index or modem error

---

#### modem_delete_sms(uint16_t sms_number)

Deletes an SMS from modem storage using `AT+CMGD`.

---

#### modem_get_oldest_sms()

Returns the index of the oldest stored SMS, or `0` if none exist.

---

## smsencode.c — Outgoing SMS PDU Encoding

Responsible for constructing GSM-compliant SMS PDUs.

### Responsibilities

- Encode destination address
- Build SMS-SUBMIT TPDU
- Pack GSM 7-bit or UCS-2 message bodies
- Insert User Data Header (UDH) when required
- Output hex-encoded PDU for `AT+CMGS`

### Notes

- Does not transmit SMS directly
- Caller is responsible for modem interaction
- Encoding is chosen based on message content

---

## smsdecode.h / smsdecode.c — Incoming SMS PDU Decoding

Parses SMS PDUs received from the modem.

### Features

- Full GSM TPDU parsing
- Sender address decoding
- Timestamp decoding
- GSM 7-bit unpacking
- UCS-2 decoding
- User Data Header (UDH) support
- Strict validation and error reporting

---

### Decoded Message Structure

The decoder exposes a structure similar to:

```
typedef struct {
    char sender[...];
    char timestamp[...];
    char text[...];
    uint16_t text_len;
    uint8_t dcs;
    uint8_t udh_present;
} sms_decoded_t;
```

Refer to `smsdecode.h` for exact field definitions.

---

### Error Handling

Decoder functions return stable, unique error codes.

Typical error categories include:

- Invalid hex encoding
- PDU too short
- Unsupported data coding scheme
- Septet unpacking errors
- Invalid or malformed UDH

This allows precise diagnosis of modem or network-side issues.

---

## utf.c — UTF-8 / Unicode Utilities

Provides UTF-8 decoding suitable for embedded systems.

### Characteristics

- No dynamic allocation
- Strict UTF-8 validation
- Handles malformed sequences safely
- Replaces invalid input with U+FFFD

### Supported Encodings

- 1-byte UTF-8
- 2-byte UTF-8
- 3-byte UTF-8

Used by both SMS encoding and decoding paths.

---

## Error Handling Philosophy

Across the entire SMS stack:

- All failures return explicit error codes
- Debug logging is intentionally verbose
- Malformed input is treated as normal
- Robustness is prioritized over permissiveness

This reflects real-world GSM network behavior.

---

## Integration Notes

- Designed for polling-based embedded systems
- Suitable for low-memory MEGA65 environments
- Can operate alongside voice call handling
- UART buffering is conservative by design

---

## Future Extension Points

The current architecture supports future additions such as:

- SMS concatenation reassembly
- Flash SMS (Class 0)
- Binary SMS payloads
- Delivery reports
- SIM vs modem storage selection

---
