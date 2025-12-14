/* ---------------------------------------------------------------------
   SMS PDU decoding routines
   (ChatGPT generated)
   ---------------------------------------------------------------------
*/


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "smsdecode.h"


static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, int out_max) {
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

// Decode semi-octet BCD digits (swapped nibbles). len_digits is number of digits.
static int decode_bcd_number(const uint8_t *b, int len_digits, uint8_t toa, char *out, int out_max) {
    int oi = 0;
    if ((toa & 0xF0) == 0x90) { // international
        if (oi < out_max - 1) out[oi++] = '+';
    }
    int bytes = (len_digits + 1) / 2;
    for (int i = 0; i < bytes; i++) {
        uint8_t v = b[i];
        uint8_t lo = (v & 0x0F);
        uint8_t hi = (v >> 4) & 0x0F;

        // first digit = low nibble
        if ((i * 2) < len_digits) {
            if (lo <= 9 && oi < out_max - 1) out[oi++] = (char)('0' + lo);
        }
        // second digit = high nibble
        if ((i * 2 + 1) < len_digits) {
            if (hi <= 9 && oi < out_max - 1) out[oi++] = (char)('0' + hi);
        }
    }
    out[oi] = '\0';
    return oi;
}

// Append a Unicode codepoint to UTF-8 buffer.
static void utf8_append(uint32_t cp, char *out, int out_max, int *oi) {
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

static void parse_udh_concat(const uint8_t *udh, int udh_len, sms_decoded_t *s) {
    // udh points at UDHL byte (not included); udh_len is UDHL bytes (the content length)
    int i = 0;
    while (i + 2 <= udh_len) {
        uint8_t iei = udh[i++];
        uint8_t iel = udh[i++];
        if (i + iel > udh_len) break;

        if (iei == 0x00 && iel == 0x03) {           // 8-bit ref
            s->concat = true;
            s->concat_ref = udh[i];
            s->concat_total = udh[i+1];
            s->concat_seq   = udh[i+2];
        } else if (iei == 0x08 && iel == 0x04) {    // 16-bit ref
            s->concat = true;
            s->concat_ref = (uint16_t)((udh[i] << 8) | udh[i+1]);
            s->concat_total = udh[i+2];
            s->concat_seq   = udh[i+3];
        }
        i += iel;
    }
}

// Decode UCS2 / UTF-16BE bytes to UTF-8.
static void ucs2_to_utf8(const uint8_t *in, int in_len, char *out, int out_max, int *out_len) {
    int oi = 0;
    int i = 0;
    while (i + 1 < in_len) {
        uint16_t w1 = (uint16_t)((in[i] << 8) | in[i+1]);
        i += 2;

        // Handle surrogate pairs if present (UTF-16BE)
        if (w1 >= 0xD800 && w1 <= 0xDBFF && i + 1 < in_len) {
            uint16_t w2 = (uint16_t)((in[i] << 8) | in[i+1]);
            if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                i += 2;
                uint32_t cp = 0x10000 + (((uint32_t)(w1 - 0xD800) << 10) | (uint32_t)(w2 - 0xDC00));
                utf8_append(cp, out, out_max, &oi);
                continue;
            }
        }
        utf8_append((uint32_t)w1, out, out_max, &oi);
    }
    if (oi < out_max) out[oi] = '\0';
    *out_len = oi;
}

// Returns 0 on success.
int decode_sms_deliver_pdu(const char *pdu_hex, sms_decoded_t *s)
{
    uint8_t pdu[512];
    int pdu_len = hex_to_bytes(pdu_hex, pdu, (int)sizeof(pdu));
    if (pdu_len <= 0) return -1;

    // Clear output
    *s = (sms_decoded_t){0};

    int idx = 0;

    // SMSC
    if (idx >= pdu_len) return -2;
    uint8_t smsc_len = pdu[idx++];
    if (idx + smsc_len > pdu_len) return -3;
    idx += smsc_len;

    if (idx >= pdu_len) return -4;
    uint8_t fo = pdu[idx++];
    uint8_t mti = (fo & 0x03);
    s->udhi = (fo & 0x40) != 0;

    // We expect DELIVER (MTI=0) here
    if (mti != 0) return -5;

    // OA
    if (idx + 2 > pdu_len) return -6;
    uint8_t oa_len_digits = pdu[idx++];
    uint8_t oa_toa = pdu[idx++];
    int oa_bytes = (oa_len_digits + 1) / 2;
    if (idx + oa_bytes > pdu_len) return -7;

    decode_bcd_number(&pdu[idx], oa_len_digits, oa_toa, s->sender, (int)sizeof(s->sender));
    idx += oa_bytes;

    // PID, DCS
    if (idx + 2 > pdu_len) return -8;
    idx++;                 // PID
    s->dcs = pdu[idx++];   // DCS

    // SCTS (7 bytes)
    if (idx + 7 > pdu_len) return -9;
    idx += 7;

    // UDL
    if (idx >= pdu_len) return -10;
    uint8_t udl = pdu[idx++];

    if (idx + udl > pdu_len) return -11;
    const uint8_t *ud = &pdu[idx];
    int ud_bytes = udl;

    // Handle UDH if present
    int text_off = 0;
    if (s->udhi) {
        if (ud_bytes < 1) return -12;
        uint8_t udhl = ud[0];
        if (1 + udhl > ud_bytes) return -13;

        // parse concat IEs (udh content starts at ud[1], length udhl)
        parse_udh_concat(&ud[1], udhl, s);

        text_off = 1 + udhl;
    }

    // Decode body depending on DCS (we care about UCS2/Unicode)
    if ((s->dcs & 0x0C) == 0x08) {
        // UCS2 / UTF-16BE-like
        const uint8_t *ucs = ud + text_off;
        int ucs_len = ud_bytes - text_off;
        ucs2_to_utf8(ucs, ucs_len, s->text, (int)sizeof(s->text) - 1, &s->text_len);
        return 0;
    }

    // If not UCS2, you can add:
    // - GSM 7-bit unpack (DCS=0x00)
    // - 8-bit copy (DCS=0x04)
    // For now, just mark empty:
    s->text[0] = '\0';
    s->text_len = 0;
    return 0;
}
