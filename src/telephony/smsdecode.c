/* ---------------------------------------------------------------------
   SMS PDU decoding routines
   (ChatGPT generated)
   ---------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "smsdecode.h"

/* ----------------------------- utilities ----------------------------- */

static int hexval(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, int out_max)
{
  int n = 0;
  while (hex[0] && hex[1]) {
    int hi = hexval(hex[0]);
    int lo = hexval(hex[1]);
    if (hi < 0 || lo < 0) return -1;
    if (n >= out_max) return -2;
    out[n++] = (uint8_t)((hi << 4) | lo);
    hex += 2;
  }
  return n;
}

static int ceil_div(int a, int b) { return (a + b - 1) / b; }

/* Decode semi-octet BCD digits (swapped nibbles). len_digits is number of digits. */
static int decode_bcd_number(const uint8_t *b, int len_digits, uint8_t toa,
                             char *out, int out_max)
{
  int oi = 0;
  
  /* International if top nibble is 0x9 (e.g. 0x91). */
  if ((toa & 0xF0) == 0x90) {
    if (oi < out_max - 1) out[oi++] = '+';
  }

  int bytes = (len_digits + 1) / 2;
  for (int i = 0; i < bytes; i++) {
    uint8_t v  = b[i];
    uint8_t lo = (v & 0x0F);
    uint8_t hi = (v >> 4) & 0x0F;

    if ((i * 2) < len_digits) {
      if (lo <= 9 && oi < out_max - 1) out[oi++] = (char)('0' + lo);
    }
    if ((i * 2 + 1) < len_digits) {
      if (hi <= 9 && oi < out_max - 1) out[oi++] = (char)('0' + hi);
    }
  }

  out[oi] = '\0';
  return oi;
}

/* Append a Unicode codepoint to UTF-8 buffer. */
static void utf8_append(uint32_t cp, char *out, int out_max, int *oi)
{
  if (cp <= 0x7F) {
    if (*oi + 1 <= out_max) out[(*oi)++] = (char)cp;
  } else if (cp <= 0x7FF) {
    if (*oi + 2 <= out_max) {
      out[(*oi)++] = (char)(0xC0 | (cp >> 6));
      out[(*oi)++] = (char)(0x80 | (cp & 0x3F));
    }
  } else if (cp <= 0xFFFF) {
    if (*oi + 3 <= out_max) {
      out[(*oi)++] = (char)(0xE0 | (cp >> 12));
      out[(*oi)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      out[(*oi)++] = (char)(0x80 | (cp & 0x3F));
    }
  } else if (cp <= 0x10FFFF) {
    if (*oi + 4 <= out_max) {
      out[(*oi)++] = (char)(0xF0 | (cp >> 18));
      out[(*oi)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
      out[(*oi)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
      out[(*oi)++] = (char)(0x80 | (cp & 0x3F));
    }
  }
}

/* Decode UCS2 / UTF-16BE bytes to UTF-8. */
static void ucs2_to_utf8(const uint8_t *in, int in_len,
                         char *out, int out_max, int *out_len)
{
  int oi = 0;
  int i = 0;

  while (i + 1 < in_len) {
    uint16_t w1 = (uint16_t)((in[i] << 8) | in[i + 1]);
    i += 2;

    /* Handle surrogate pairs if present (UTF-16BE). */
    if (w1 >= 0xD800 && w1 <= 0xDBFF && i + 1 < in_len) {
      uint16_t w2 = (uint16_t)((in[i] << 8) | in[i + 1]);
      if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
        i += 2;
        uint32_t cp = 0x10000u
                    + (((uint32_t)(w1 - 0xD800u) << 10)
                       | (uint32_t)(w2 - 0xDC00u));
        utf8_append(cp, out, out_max, &oi);
        continue;
      }
    }

    utf8_append((uint32_t)w1, out, out_max, &oi);
  }

  if (oi < out_max) out[oi] = '\0';
  *out_len = oi;
}

/* Determine alphabet from common "General Data Coding indication" group.
   returns: 0=7-bit, 1=8-bit, 2=UCS2, 3=reserved/unknown */
static int dcs_alphabet(uint8_t dcs)
{
  if ((dcs & 0xC0) == 0x00) {
    return (dcs >> 2) & 0x03;
  }
  return 0;
}

/* Calculate how many bytes of user data are present (for bounds checks),
   given DCS, UDL and available bytes remaining in the PDU. */
static int ud_bytes_from_udl(uint8_t dcs, uint8_t udl, int avail_bytes)
{
  int alpha = dcs_alphabet(dcs);

  if (alpha == 0) {
    /* GSM 7-bit: UDL is septets */
    int max_septets = (avail_bytes * 8) / 7;
    if ((int)udl > max_septets) return -1;
    return ceil_div((int)udl * 7, 8);
  }

  /* 8-bit / UCS2: UDL is bytes */
  if ((int)udl > avail_bytes) return -1;
  return (int)udl;
}

/* Minimal GSM 7-bit unpack with configurable bit offset.
   Outputs septet values as bytes (ASCII-ish). */
static int gsm7_unpack(const uint8_t *in, int in_len,
                       int septet_len, int bit_offset,
                       char *out, int out_max)
{
  int oi = 0;

  for (int s = 0; s < septet_len; s++) {
    int bitpos  = bit_offset + s * 7;
    int bytepos = bitpos >> 3;
    int shift   = bitpos & 7;

    if (bytepos >= in_len) break;

    uint16_t w = in[bytepos];
    if (bytepos + 1 < in_len) w |= (uint16_t)in[bytepos + 1] << 8;

    uint8_t septet = (uint8_t)((w >> shift) & 0x7F);

    if (oi < out_max - 1) out[oi++] = (char)septet;
  }

  if (oi < out_max) out[oi] = '\0';
  return oi;
}

/* TON=5 => alphanumeric address (GSM 7-bit packed) */
static bool is_alphanumeric_toa(uint8_t toa)
{
  return ((toa & 0x70) == 0x50);
}

/* Parse concatenation IEs in UDH. */
static void parse_udh_concat(const uint8_t *udh, int udh_len, sms_decoded_t *s)
{
  int i = 0;
  while (i + 2 <= udh_len) {
    uint8_t iei = udh[i++];
    uint8_t iel = udh[i++];
    if (i + iel > udh_len) break;

    if (iei == 0x00 && iel == 0x03) {           /* 8-bit ref */
      s->concat = 1;
      s->concat_ref   = udh[i];
      s->concat_total = udh[i + 1];
      s->concat_seq   = udh[i + 2];
    } else if (iei == 0x08 && iel == 0x04) {    /* 16-bit ref */
      s->concat = 1;
      s->concat_ref   = (uint16_t)((udh[i] << 8) | udh[i + 1]);
      s->concat_total = udh[i + 2];
      s->concat_seq   = udh[i + 3];
    }
    i += iel;
  }
}

/* Address decode helper:
   - len is digits (numeric) or septets (alphanumeric TON=5)
   - returns bytes consumed from pdu (after TOA), or negative on error */
static int decode_address_field(const uint8_t *pdu, int pdu_len, int idx,
                                uint8_t addr_len, uint8_t toa,
                                char *out, int out_max)
{
  if (idx > pdu_len) return -1;

  if (is_alphanumeric_toa(toa)) {
    int bytes = ceil_div((int)addr_len * 7, 8);
    if (idx + bytes > pdu_len) return -2;
    gsm7_unpack(&pdu[idx], bytes, (int)addr_len, 0, out, out_max);
    return bytes;
  } else {
    int bytes = (addr_len + 1) / 2;
    if (idx + bytes > pdu_len) return -2;
    decode_bcd_number(&pdu[idx], (int)addr_len, toa, out, out_max);
    return bytes;
  }
}

/* SCTS decode (semi-octet swapped BCD), store into struct fields. */
static int bcd_swap_to_int(uint8_t v)
{
  int lo = (v & 0x0F);
  int hi = (v >> 4) & 0x0F;
  return lo * 10 + hi;
}

static int scts_tz_minutes(uint8_t tz_octet)
{
  /* Sign bit commonly seen in bit3 (0x08). */
  int neg = (tz_octet & 0x08) ? 1 : 0;
  uint8_t tz_clean = (uint8_t)(tz_octet & (uint8_t)~0x08);

  int q = bcd_swap_to_int(tz_clean);
  int mins = q * 15;
  return neg ? -mins : mins;
}

static void scts_decode(const uint8_t scts[7],
                        uint8_t raw_out[7],
                        uint16_t *year, uint8_t *mo, uint8_t *dd,
                        uint8_t *hh, uint8_t *mi, uint8_t *ss,
                        int16_t *tzmins)
{
  for (int i = 0; i < 7; i++) raw_out[i] = scts[i];

  int yy = bcd_swap_to_int(scts[0]);
  *year = (uint16_t)(2000 + yy);
  *mo   = (uint8_t)bcd_swap_to_int(scts[1]);
  *dd   = (uint8_t)bcd_swap_to_int(scts[2]);
  *hh   = (uint8_t)bcd_swap_to_int(scts[3]);
  *mi   = (uint8_t)bcd_swap_to_int(scts[4]);
  *ss   = (uint8_t)bcd_swap_to_int(scts[5]);
  *tzmins = (int16_t)scts_tz_minutes(scts[6]);
}

/* Decode UD into s->text, parse UDH concat if present.
   idx points to UDL byte in TPDU (right before UDL). */
static int decode_user_data(const uint8_t *pdu, int pdu_len, int idx_udl,
                            uint8_t fo, uint8_t dcs,
                            sms_decoded_t *s)
{
  if (idx_udl >= pdu_len) return -1;

  uint8_t udl = pdu[idx_udl++];
  int avail = pdu_len - idx_udl;
  int ud_bytes = ud_bytes_from_udl(dcs, udl, avail);
  if (ud_bytes < 0) return -2;

  const uint8_t *ud = &pdu[idx_udl];

  int alpha = dcs_alphabet(dcs);
  s->alphabet = (uint8_t)alpha;
  s->udhi = (uint8_t)((fo & 0x40) ? 1 : 0);

  int text_off_bytes = 0;
  int bit_offset = 0;

  if (s->udhi) {
    if (ud_bytes < 1) return -3;
    uint8_t udhl = ud[0];
    if (1 + (int)udhl > ud_bytes) {
      fprintf(stderr,"DEBUG: Bad if (1 + (int)udhl > ud_bytes) {\n");
      fprintf(stderr,"       udhl=%d, ud_bytes=%d\n",udhl,ud_bytes);
      return -4;
    }

    parse_udh_concat(&ud[1], (int)udhl, s);

    text_off_bytes = 1 + (int)udhl;

    if (alpha == 0) {
      bit_offset = text_off_bytes * 8;
      text_off_bytes = 0;
    }
  }

  if (alpha == 2) {
    const uint8_t *ucs = ud + text_off_bytes;
    int ucs_len = ud_bytes - text_off_bytes;
    if (ucs_len < 0) ucs_len = 0;
    ucs2_to_utf8(ucs, ucs_len, s->text, (int)sizeof(s->text) - 1, &s->text_len);
    return 0;
  }

  if (alpha == 0) {
    s->text_len = gsm7_unpack(ud, ud_bytes, (int)udl, bit_offset,
                              s->text, (int)sizeof(s->text));
    return 0;
  }

  if (alpha == 1) {
    int n = ud_bytes - text_off_bytes;
    if (n < 0) n = 0;
    if (n > (int)sizeof(s->text) - 1) n = (int)sizeof(s->text) - 1;
    for (int i = 0; i < n; i++) s->text[i] = (char)ud[text_off_bytes + i];
    s->text[n] = '\0';
    s->text_len = n;
    return 0;
  }

  s->text[0] = '\0';
  s->text_len = 0;
  return 0;
}

/* -------------------------- MTI decoders ----------------------------- */

static int decode_deliver_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo,
                               sms_decoded_t *s)
{
  /* OA */
  if (idx + 2 > pdu_len) return -10;
  uint8_t oa_len = pdu[idx++];
  uint8_t oa_toa = pdu[idx++];

  int consumed = decode_address_field(pdu, pdu_len, idx, oa_len, oa_toa,
                                      s->sender, (int)sizeof(s->sender));
  if (consumed < 0) return -11;
  idx += consumed;

  /* PID, DCS */
  if (idx + 2 > pdu_len) return -12;
  idx++;               /* PID */
  s->dcs = pdu[idx++];

  /* SCTS */
  if (idx + 7 > pdu_len) return -13;
  scts_decode(&pdu[idx],
              s->scts_raw,
              &s->year, &s->month, &s->day,
              &s->hour, &s->minute, &s->second,
              &s->tz_minutes);
  idx += 7;

  /* UDL + UD */
  return decode_user_data(pdu, pdu_len, idx, fo, s->dcs, s);
}

static int decode_submit_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo,
                              sms_decoded_t *s)
{
  /* SUBMIT:
     fo, MR, DA, PID, DCS, VP(optional), UDL, UD */
  if (idx >= pdu_len) return -20;
  s->mr = pdu[idx++];

  /* DA */
  if (idx + 2 > pdu_len) return -21;
  uint8_t da_len = pdu[idx++];
  uint8_t da_toa = pdu[idx++];

  int consumed = decode_address_field(pdu, pdu_len, idx, da_len, da_toa,
                                      s->sender, (int)sizeof(s->sender));
  if (consumed < 0) return -22;
  idx += consumed;

  /* PID, DCS */
  if (idx + 2 > pdu_len) return -23;
  idx++;               /* PID */
  s->dcs = pdu[idx++];

  /* VP field depends on VPF bits (bits 3..4) */
  uint8_t vpf = (fo >> 3) & 0x03;
  if (vpf == 0x02) {
    /* relative: 1 octet */
    if (idx + 1 > pdu_len) return -24;
    idx += 1;
  } else if (vpf == 0x01 || vpf == 0x03) {
    /* enhanced(7) or absolute(7) */
    if (idx + 7 > pdu_len) return -25;
    idx += 7;
  }

  /* No SCTS in SUBMIT; leave timestamp fields as 0 */
  return decode_user_data(pdu, pdu_len, idx, fo, s->dcs, s);
}

static int decode_status_report_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo,
                                     sms_decoded_t *s)
{
  /* STATUS-REPORT:
     fo, MR, RA, SCTS, DT, ST, [PI ... optional] */
  (void)fo;

  if (idx >= pdu_len) return -30;
  s->mr = pdu[idx++];

  /* RA */
  if (idx + 2 > pdu_len) return -31;
  uint8_t ra_len = pdu[idx++];
  uint8_t ra_toa = pdu[idx++];

  int consumed = decode_address_field(pdu, pdu_len, idx, ra_len, ra_toa,
                                      s->sender, (int)sizeof(s->sender));
  if (consumed < 0) return -32;
  idx += consumed;

  /* SCTS */
  if (idx + 7 > pdu_len) return -33;
  scts_decode(&pdu[idx],
              s->scts_raw,
              &s->year, &s->month, &s->day,
              &s->hour, &s->minute, &s->second,
              &s->tz_minutes);
  idx += 7;

  /* DT (discharge time) */
  if (idx + 7 > pdu_len) return -34;
  scts_decode(&pdu[idx],
              s->dt_raw,
              &s->dt_year, &s->dt_month, &s->dt_day,
              &s->dt_hour, &s->dt_minute, &s->dt_second,
              &s->dt_tz_minutes);
  idx += 7;

  /* ST */
  if (idx >= pdu_len) return -35;
  s->status = pdu[idx++];

  /* Optional: PI + extra fields exist, but for compactness we stop here. */
  s->text[0] = '\0';
  s->text_len = 0;
  s->dcs = 0;
  s->alphabet = 0;
  s->udhi = 0;
  s->concat = 0;
  return 0;
}

static int decode_command_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo,
                               sms_decoded_t *s)
{
  /* COMMAND:
     fo, MR, PID, CT, MN, DA, CDL, CD
     Rare; we parse address + keep command data as 8-bit blob if present. */
  (void)fo;

  if (idx + 4 > pdu_len) return -40;
  s->mr = pdu[idx++];     /* MR */
  idx++;                  /* PID */
  uint8_t ct = pdu[idx++];/* CT */
  uint8_t mn = pdu[idx++];/* MN */

  /* DA */
  if (idx + 2 > pdu_len) return -41;
  uint8_t da_len = pdu[idx++];
  uint8_t da_toa = pdu[idx++];

  int consumed = decode_address_field(pdu, pdu_len, idx, da_len, da_toa,
                                      s->sender, (int)sizeof(s->sender));
  if (consumed < 0) return -42;
  idx += consumed;

  /* CDL */
  if (idx >= pdu_len) return -43;
  uint8_t cdl = pdu[idx++];

  /* CD: treat as 8-bit */
  int avail = pdu_len - idx;
  int n = (cdl <= (uint8_t)avail) ? (int)cdl : avail;
  if (n < 0) n = 0;
  if (n > (int)sizeof(s->text) - 1) n = (int)sizeof(s->text) - 1;

  for (int i = 0; i < n; i++) s->text[i] = (char)pdu[idx + i];
  s->text[n] = '\0';
  s->text_len = n;

  /* Stash CT/MN into dcs/alphabet fields? Better: just leave as-is.
     If you want, you can print CT/MN in your caller from mr/status etc. */
  s->dcs = ct;
  s->alphabet = mn;
  return 0;
}

/* -------------------------- main entry point -------------------------- */

int decode_sms_deliver_pdu(const char *pdu_hex, sms_decoded_t *s)
{
  uint8_t pdu[512];

fprintf(stderr, "DEBUG: sizeof(sms_decoded_t)=%zu\n", sizeof(sms_decoded_t));
fprintf(stderr, "DEBUG: s=%p\n", (void*)s);
  
  fprintf(stderr, "DEBUG: smsdecode.c signature: %s %s\n", __DATE__, __TIME__);
  
  fprintf(stderr, "DEBUG: pdu_hex strlen=%zu\n", strlen(pdu_hex));
  fprintf(stderr, "DEBUG: pdu_hex head=%.32s\n", pdu_hex);
  fprintf(stderr, "DEBUG: pdu_hex tail=%.32s\n",
	  strlen(pdu_hex) > 32 ? pdu_hex + strlen(pdu_hex) - 32 : pdu_hex);
  
  int pdu_len = hex_to_bytes(pdu_hex, pdu, (int)sizeof(pdu));

  fprintf(stderr, "DEBUG: hex_to_bytes pdu_len=%d\n", pdu_len);
  
  if (pdu_len > 0) {
    fprintf(stderr, "DEBUG: pdu[0..15]=");
    for (int i=0;i<16 && i<pdu_len;i++) fprintf(stderr, "%02X", pdu[i]);
    fprintf(stderr, "\n");
  }
  if (pdu_len <= 0) return -1;

  if (pdu_len <= 0) return -1;

  *s = (sms_decoded_t){0};

  int idx = 0;

  /* SMSC */
  if (idx >= pdu_len) return -2;
  uint8_t smsc_len = pdu[idx++];
  if (idx + smsc_len > pdu_len) return -3;
  idx += smsc_len;

  /* First octet */
  if (idx >= pdu_len) {
    fprintf(stderr,"DEBUG: Bad idx=%d, pdu_len=%d\n",idx,pdu_len);
    return -4;
  }
  uint8_t fo = pdu[idx++];
  s->mti  = (uint8_t)(fo & 0x03);
  s->udhi = (uint8_t)((fo & 0x40) ? 1 : 0);

  /* Dispatch by MTI */
  if (s->mti == 0) {
    return decode_deliver_tpdu(pdu, pdu_len, idx, fo, s);
  } else if (s->mti == 1) {
    return decode_submit_tpdu(pdu, pdu_len, idx, fo, s);
  } else if (s->mti == 2) {
    return decode_status_report_tpdu(pdu, pdu_len, idx, fo, s);
  } else { /* mti == 3 */
    return decode_command_tpdu(pdu, pdu_len, idx, fo, s);
  }
}
