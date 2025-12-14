#pragma once
#include <stdint.h>

typedef struct {
  char sender[32];      // DELIVER: originator; STATUS-REPORT: recipient; SUBMIT: destination
  char text[512];       // utf-8 output (UCS2->UTF8, 7-bit->approx ASCII)
  int  text_len;

  // Optional metadata
  uint8_t dcs;
  uint8_t udhi;

  // TPDU type
  uint8_t mti;          // 0=DELIVER, 1=SUBMIT, 2=STATUS-REPORT, 3=COMMAND
  uint8_t alphabet;     // 0=7bit,1=8bit,2=UCS2,3=reserved

  uint8_t lang_lock;
  uint8_t lang_single;
  
  // Concatenation (if UDH has it)
  uint8_t  concat;
  uint16_t concat_ref;
  uint8_t  concat_total;
  uint8_t  concat_seq;

  // Timestamp from PDU (SCTS for DELIVER and STATUS-REPORT)
  uint8_t  scts_raw[7];
  uint16_t year;
  uint8_t  month, day;
  uint8_t  hour, minute, second;
  int16_t  tz_minutes;

  // STATUS-REPORT only: discharge time + status
  uint8_t  dt_raw[7];
  uint16_t dt_year;
  uint8_t  dt_month, dt_day;
  uint8_t  dt_hour, dt_minute, dt_second;
  int16_t  dt_tz_minutes;

  uint8_t  status;      // STATUS-REPORT: TP-ST, else 0
  uint8_t  mr;          // message reference if present (SUBMIT/STATUS-REPORT/COMMAND)
} sms_decoded_t;

int decode_sms_deliver_pdu(const char *pdu_hex, sms_decoded_t *s);
