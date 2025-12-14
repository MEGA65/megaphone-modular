/* ---------------------------------------------------------------------
   SMS PDU decoding routines
   (ChatGPT generated)
   ---------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

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

/* Parse concatenation IEs in UDH. */
static void parse_udh_concat(const uint8_t *udh, int udh_len, sms_decoded_t *s)
{
    int i = 0;
    while (i + 2 <= udh_len) {
        uint8_t iei = udh[i++];
        uint8_t iel = udh[i++];
        if (i + iel > udh_len) break;

        if (iei == 0x00 && iel == 0x03) {           /* 8-bit ref */
            s->concat = true;
            s->concat_ref   = udh[i];
            s->concat_total = udh[i + 1];
            s->concat_seq   = udh[i + 2];
        } else if (iei == 0x08 && iel == 0x04) {    /* 16-bit ref */
            s->concat = true;
            s->concat_ref   = (uint16_t)((udh[i] << 8) | udh[i + 1]);
            s->concat_total = udh[i + 2];
            s->concat_seq   = udh[i + 3];
        }
        i += iel;
    }
}

static int ceil_div(int a, int b) { return (a + b - 1) / b; }

/* Determine alphabet from common "General Data Coding indication" group.
   returns: 0=7-bit, 1=8-bit, 2=UCS2, 3=reserved/unknown */
static int dcs_alphabet(uint8_t dcs)
{
    if ((dcs & 0xC0) == 0x00) {
        return (dcs >> 2) & 0x03;
    }
    /* Conservative default: treat as 7-bit if unknown */
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
   - septet_len is UDL (septets)
   - bit_offset is 0 normally; if UDHI present, bit_offset = (1+UDHL)*8
   Note: This does NOT implement GSM 03.38 character mapping/escape; it
   outputs septet values as bytes (works OK for plain ASCII-ish texts). */
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

/* -------------------------- main entry point -------------------------- */

/* Returns 0 on success, negative on failure. */
int decode_sms_deliver_pdu(const char *pdu_hex, sms_decoded_t *s)
{
    uint8_t pdu[512];

    int pdu_len = hex_to_bytes(pdu_hex, pdu, (int)sizeof(pdu));
    if (pdu_len <= 0) return -1;

    *s = (sms_decoded_t){0};

    int idx = 0;

    /* SMSC */
    if (idx >= pdu_len) return -2;
    uint8_t smsc_len = pdu[idx++];
    if (idx + smsc_len > pdu_len) return -3;
    idx += smsc_len;

    /* First octet */
    if (idx >= pdu_len) return -4;
    uint8_t fo = pdu[idx++];
    uint8_t mti = (fo & 0x03);
    s->udhi = (fo & 0x40) != 0;

    /* Expect DELIVER (MTI=0) */
    if (mti != 0) return -5;

    /* Originating address (OA) */
    if (idx + 2 > pdu_len) return -6;
    uint8_t oa_len_digits = pdu[idx++];
    uint8_t oa_toa        = pdu[idx++];
    int oa_bytes = (oa_len_digits + 1) / 2;
    if (idx + oa_bytes > pdu_len) return -7;

    decode_bcd_number(&pdu[idx], oa_len_digits, oa_toa,
                      s->sender, (int)sizeof(s->sender));
    idx += oa_bytes;

    /* PID, DCS */
    if (idx + 2 > pdu_len) return -8;
    idx++;               /* PID */
    s->dcs = pdu[idx++]; /* DCS */

    /* SCTS (7 bytes) */
    if (idx + 7 > pdu_len) return -9;
    idx += 7;

    /* UDL */
    if (idx >= pdu_len) return -10;
    uint8_t udl = pdu[idx++];

    int avail = pdu_len - idx;
    int ud_bytes = ud_bytes_from_udl(s->dcs, udl, avail);
    if (ud_bytes < 0) {
        fprintf(stderr,
                "bad udl/dcs: idx=%d udl=%u dcs=0x%02x avail=%d pdu_len=%d\n",
                idx, udl, s->dcs, avail, pdu_len);
        return -11;
    }

    const uint8_t *ud = &pdu[idx];

    /* UDH handling */
    int text_off_bytes = 0;
    int bit_offset = 0;
    int alpha = dcs_alphabet(s->dcs);

    if (s->udhi) {
        if (ud_bytes < 1) return -12;
        uint8_t udhl = ud[0];
        if (1 + (int)udhl > ud_bytes) return -13;

        /* Parse UDH IEs */
        parse_udh_concat(&ud[1], (int)udhl, s);

        text_off_bytes = 1 + (int)udhl;

        if (alpha == 0) {
            /* 7-bit: UDH shifts septet packing by (UDHL+1) octets */
            bit_offset = text_off_bytes * 8;
            text_off_bytes = 0; /* keep UD pointer at start; use bit_offset */
        }
    }

    /* Decode message body */
    if (alpha == 2) {
        /* UCS2 / UTF-16BE-like */
        const uint8_t *ucs = ud + text_off_bytes;
        int ucs_len = ud_bytes - text_off_bytes;
        if (ucs_len < 0) ucs_len = 0;
        ucs2_to_utf8(ucs, ucs_len, s->text, (int)sizeof(s->text) - 1, &s->text_len);
        return 0;
    }

    if (alpha == 0) {
        /* GSM 7-bit */
        s->text_len = gsm7_unpack(ud, ud_bytes, (int)udl, bit_offset,
                                  s->text, (int)sizeof(s->text));
        return 0;
    }

    if (alpha == 1) {
        /* 8-bit: copy bytes as-is (caller may interpret encoding) */
        int n = ud_bytes - text_off_bytes;
        if (n < 0) n = 0;
        if (n > (int)sizeof(s->text) - 1) n = (int)sizeof(s->text) - 1;
        for (int i = 0; i < n; i++) s->text[i] = (char)ud[text_off_bytes + i];
        s->text[n] = '\0';
        s->text_len = n;
        return 0;
    }

    /* Reserved/unknown alphabet */
    s->text[0] = '\0';
    s->text_len = 0;
    return 0;
}


