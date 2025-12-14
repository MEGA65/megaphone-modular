/* ---------------------------------------------------------------------
   SMS PDU decoding routines (Embedded Optimized)
   Generated using ChatGPT and Gemini
   ---------------------------------------------------------------------
*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "smsdecode.h"

/* ----------------------------- constants ----------------------------- */

/* Return codes */
enum {
    RC_OK = 0,
    RC_HEX_BADCHAR      = -1001,
    RC_HEX_TOOLONG      = -1002,
    RC_PDU_EMPTY        = -1003,
    RC_SMSC_OOB         = -1101,
    RC_SMSC_LEN_OOB     = -1102,
    RC_FO_OOB           = -1201,
    RC_MTI_UNSUPPORTED  = -1301,
    RC_DELIVER_OA_HDR_OOB = -2001,
    RC_DELIVER_OA_BODY_OOB = -2002,
    RC_DELIVER_PID_DCS_OOB = -2003,
    RC_DELIVER_SCTS_OOB   = -2004,
    RC_DELIVER_UDL_OOB    = -2005,
    RC_SUBMIT_MR_OOB      = -2101,
    RC_SUBMIT_DA_HDR_OOB  = -2102,
    RC_SUBMIT_DA_BODY_OOB = -2103,
    RC_SUBMIT_PID_DCS_OOB = -2104,
    RC_SUBMIT_VP_OOB      = -2105,
    RC_SUBMIT_UDL_OOB     = -2106,
    RC_SR_MR_OOB          = -2201,
    RC_SR_RA_HDR_OOB      = -2202,
    RC_SR_RA_BODY_OOB     = -2203,
    RC_SR_SCTS_OOB        = -2204,
    RC_SR_DT_OOB          = -2205,
    RC_SR_ST_OOB          = -2206,
    RC_CMD_FIXED_OOB      = -2301,
    RC_CMD_DA_HDR_OOB     = -2302,
    RC_CMD_DA_BODY_OOB    = -2303,
    RC_CMD_CDL_OOB        = -2304,
    RC_UD_UDL_AVAIL_BAD   = -3001,
    RC_UD_UDHI_NO_UDHL    = -3002,
    RC_UD_UDHL_TOO_BIG    = -3003,
    RC_ADDR_TOOSHORT      = -4001,
    RC_ADDR_BODY_OOB      = -4002,
};

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
        if (hi < 0 || lo < 0) return RC_HEX_BADCHAR;
        if (n >= out_max) return RC_HEX_TOOLONG;
        out[n++] = (uint8_t)((hi << 4) | lo);
        hex += 2;
    }
    if (n == 0) return RC_PDU_EMPTY;
    return n;
}

static inline int ceil_div(int a, int b) { return (a + b - 1) / b; }

/* ------------------------ GSM 7-bit unpack ------------------------ */

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

/* ------------------------ UCS2 to UTF-8 ------------------------ */

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

static void ucs2_to_utf8(const uint8_t *in, int in_len,
                         char *out, int out_max, int *out_len)
{
    int oi = 0;
    int i = 0;
    while (i + 1 < in_len) {
        uint16_t w1 = (uint16_t)((in[i] << 8) | in[i + 1]);
        i += 2;
        if (w1 >= 0xD800 && w1 <= 0xDBFF && i + 1 < in_len) {
            uint16_t w2 = (uint16_t)((in[i] << 8) | in[i + 1]);
            if (w2 >= 0xDC00 && w2 <= 0xDFFF) {
                i += 2;
                uint32_t cp = 0x10000u + (((uint32_t)(w1 - 0xD800u) << 10) | (uint32_t)(w2 - 0xDC00u));
                utf8_append(cp, out, out_max, &oi);
                continue;
            }
        }
        utf8_append((uint32_t)w1, out, out_max, &oi);
    }
    if (oi < out_max) out[oi] = '\0';
    *out_len = oi;
}

/* ------------------------ Helpers ------------------------ */

static int dcs_alphabet(uint8_t dcs)
{
    if ((dcs & 0xC0) == 0x00) return (dcs >> 2) & 0x03;
    return 0;
}

static int ud_bytes_from_udl(uint8_t dcs, uint8_t udl, int avail_bytes)
{
    int alpha = dcs_alphabet(dcs);
    if (alpha == 0) {
        int max_septets = (avail_bytes * 8) / 7;
        if ((int)udl > max_septets) return -1;
        return ceil_div((int)udl * 7, 8);
    }
    if ((int)udl > avail_bytes) return -1;
    return (int)udl;
}

static bool is_alphanumeric_toa(uint8_t toa)
{
    return ((toa & 0x70) == 0x50);
}

static int decode_bcd_number(const uint8_t *b, int len_digits, uint8_t toa, char *out, int out_max)
{
    int oi = 0;
    if ((toa & 0xF0) == 0x90) {
        if (oi < out_max - 1) out[oi++] = '+';
    }
    int bytes = (len_digits + 1) / 2;
    for (int i = 0; i < bytes; i++) {
        uint8_t v  = b[i];
        if ((i * 2) < len_digits && (v & 0x0F) <= 9 && oi < out_max - 1) 
            out[oi++] = (char)('0' + (v & 0x0F));
        if ((i * 2 + 1) < len_digits && ((v >> 4) & 0x0F) <= 9 && oi < out_max - 1) 
            out[oi++] = (char)('0' + ((v >> 4) & 0x0F));
    }
    out[oi] = '\0';
    return oi;
}

static int decode_address_field(const uint8_t *pdu, int pdu_len, int idx,
                                uint8_t addr_len, uint8_t toa,
                                char *out, int out_max)
{
    if (idx >= pdu_len) return RC_ADDR_TOOSHORT;

    if (is_alphanumeric_toa(toa)) {
        int bytes = ceil_div((int)addr_len, 2);
        int bits = (int)addr_len * 4;
        int septets = bits / 7;
        if (septets <= 0) septets = (bytes * 8) / 7;
        
        if (idx + bytes > pdu_len) return RC_ADDR_BODY_OOB;
        gsm7_unpack(&pdu[idx], bytes, septets, 0, out, out_max);
        
        /* Trim padding */
        for (int i = 0; i < out_max; i++) {
            if (out[i] == '\0') break;
            if ((unsigned char)out[i] < 0x20 && out[i] != '\t') { out[i] = '\0'; break; }
        }
        return bytes;
    } else {
        int digits = (int)addr_len;
        int bytes = (digits + 1) / 2;
        if (idx + bytes > pdu_len) return RC_ADDR_BODY_OOB;
        decode_bcd_number(&pdu[idx], digits, toa, out, out_max);
        return bytes;
    }
}

static void parse_udh_concat(const uint8_t *udh, int udh_len, sms_decoded_t *s)
{
    int i = 0;
    while (i + 2 <= udh_len) {
        uint8_t iei = udh[i++];
        uint8_t iel = udh[i++];
        if (i + iel > udh_len) break;

        if (iei == 0x00 && iel == 0x03) {
            s->concat = 1;
            s->concat_ref   = udh[i];
            s->concat_total = udh[i + 1];
            s->concat_seq   = udh[i + 2];
        } else if (iei == 0x08 && iel == 0x04) {
            s->concat = 1;
            s->concat_ref   = (uint16_t)((udh[i] << 8) | udh[i + 1]);
            s->concat_total = udh[i + 2];
            s->concat_seq   = udh[i + 3];
        }
        i += iel;
    }
}

/* ------------------------ UD decode ------------------------ */

static int decode_user_data(const uint8_t *pdu, int pdu_len, int idx_udl,
                            uint8_t fo, uint8_t dcs,
                            sms_decoded_t *s)
{
    if (idx_udl >= pdu_len) return RC_DELIVER_UDL_OOB;

    uint8_t udl = pdu[idx_udl++];
    int avail = pdu_len - idx_udl;
    int alpha = dcs_alphabet(dcs);
    int ud_bytes = ud_bytes_from_udl(dcs, udl, avail);

    if (ud_bytes < 0) return RC_UD_UDL_AVAIL_BAD;
    if (idx_udl + ud_bytes > pdu_len) return RC_UD_UDL_AVAIL_BAD;

    const uint8_t *ud = &pdu[idx_udl];
    int text_off_bytes = 0;
    int bit_offset = 0;

    s->udhi = (uint8_t)((fo & 0x40) ? 1 : 0);

    if (s->udhi) {
        if (ud_bytes < 1) return RC_UD_UDHI_NO_UDHL;
        uint8_t udhl = ud[0];
        if (1 + (int)udhl > ud_bytes) return RC_UD_UDHL_TOO_BIG;

        parse_udh_concat(&ud[1], (int)udhl, s);
        text_off_bytes = 1 + (int)udhl;
    }

    /* UCS-2 */
    if (alpha == 2) {
        const uint8_t *ucs = ud + text_off_bytes;
        int ucs_len = ud_bytes - text_off_bytes;
        if (ucs_len < 0) ucs_len = 0;
        ucs2_to_utf8(ucs, ucs_len, s->text, (int)sizeof(s->text) - 1, &s->text_len);
        return RC_OK;
    }

    /* GSM 7-bit */
    if (alpha == 0) {
        int septets_to_decode = (int)udl;
        bit_offset = 0;

        if (s->udhi) {
            int header_bits = text_off_bytes * 8;
            int header_septets = (header_bits + 6) / 7;
            bit_offset = header_septets * 7;
            
            septets_to_decode -= header_septets;
            if (septets_to_decode < 0) septets_to_decode = 0;
        }

        s->text_len = gsm7_unpack(ud, ud_bytes, septets_to_decode, bit_offset,
                                  s->text, (int)sizeof(s->text));
        return RC_OK;
    }

    /* 8-bit Data (fallback) */
    int n = ud_bytes - text_off_bytes;
    if (n < 0) n = 0;
    if (n > (int)sizeof(s->text) - 1) n = (int)sizeof(s->text) - 1;
    for (int i = 0; i < n; i++) s->text[i] = (char)ud[text_off_bytes + i];
    s->text[n] = '\0';
    s->text_len = n;
    return RC_OK;
}

/* ------------------------ TPDU decoders ------------------------ */

static int decode_deliver_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo, sms_decoded_t *s)
{
    if (idx + 2 > pdu_len) return RC_DELIVER_OA_HDR_OOB;
    uint8_t oa_len = pdu[idx++];
    uint8_t oa_toa = pdu[idx++];

    int consumed = decode_address_field(pdu, pdu_len, idx, oa_len, oa_toa, s->sender, (int)sizeof(s->sender));
    if (consumed < 0) return RC_DELIVER_OA_BODY_OOB;
    idx += consumed;

    if (idx + 2 > pdu_len) return RC_DELIVER_PID_DCS_OOB;
    uint8_t pid = pdu[idx++]; /* PID unused */
    (void)pid;
    s->dcs = pdu[idx++];

    if (idx + 7 > pdu_len) return RC_DELIVER_SCTS_OOB;
    idx += 7; /* Skip SCTS validation for tight embedded size */

    if (idx >= pdu_len) return RC_DELIVER_UDL_OOB;
    return decode_user_data(pdu, pdu_len, idx, fo, s->dcs, s);
}

static int decode_submit_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo, sms_decoded_t *s)
{
    if (idx >= pdu_len) return RC_SUBMIT_MR_OOB;
    idx++; /* MR */

    if (idx + 2 > pdu_len) return RC_SUBMIT_DA_HDR_OOB;
    uint8_t da_len = pdu[idx++];
    uint8_t da_toa = pdu[idx++];

    int consumed = decode_address_field(pdu, pdu_len, idx, da_len, da_toa, s->sender, (int)sizeof(s->sender));
    if (consumed < 0) return RC_SUBMIT_DA_BODY_OOB;
    idx += consumed;

    if (idx + 2 > pdu_len) return RC_SUBMIT_PID_DCS_OOB;
    idx++; /* PID */
    s->dcs = pdu[idx++];

    uint8_t vpf = (fo >> 3) & 0x03;
    if (vpf == 0x02) {
        if (idx + 1 > pdu_len) return RC_SUBMIT_VP_OOB;
        idx += 1;
    } else if (vpf == 0x01 || vpf == 0x03) {
        if (idx + 7 > pdu_len) return RC_SUBMIT_VP_OOB;
        idx += 7;
    }

    if (idx >= pdu_len) return RC_SUBMIT_UDL_OOB;
    return decode_user_data(pdu, pdu_len, idx, fo, s->dcs, s);
}

static int decode_status_report_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo, sms_decoded_t *s)
{
    (void)fo;
    if (idx >= pdu_len) return RC_SR_MR_OOB;
    idx++; /* MR */

    if (idx + 2 > pdu_len) return RC_SR_RA_HDR_OOB;
    uint8_t ra_len = pdu[idx++];
    uint8_t ra_toa = pdu[idx++];

    int consumed = decode_address_field(pdu, pdu_len, idx, ra_len, ra_toa, s->sender, (int)sizeof(s->sender));
    if (consumed < 0) return RC_SR_RA_BODY_OOB;
    idx += consumed;

    if (idx + 14 > pdu_len) return RC_SR_SCTS_OOB;
    idx += 14; /* Skip SCTS + DT */

    if (idx >= pdu_len) return RC_SR_ST_OOB;
    /* uint8_t st = pdu[idx++]; */
    
    return RC_OK;
}

static int decode_command_tpdu(const uint8_t *pdu, int pdu_len, int idx, uint8_t fo, sms_decoded_t *s)
{
    (void)fo;
    if (idx + 4 > pdu_len) return RC_CMD_FIXED_OOB;
    idx += 4;

    if (idx + 2 > pdu_len) return RC_CMD_DA_HDR_OOB;
    uint8_t da_len = pdu[idx++];
    uint8_t da_toa = pdu[idx++];

    int consumed = decode_address_field(pdu, pdu_len, idx, da_len, da_toa, s->sender, (int)sizeof(s->sender));
    if (consumed < 0) return RC_CMD_DA_BODY_OOB;
    idx += consumed;

    if (idx >= pdu_len) return RC_CMD_CDL_OOB;
    uint8_t cdl = pdu[idx++];

    int avail = pdu_len - idx;
    int n = (cdl <= (uint8_t)avail) ? (int)cdl : avail;
    if (n < 0) n = 0;
    if (n > (int)sizeof(s->text) - 1) n = (int)sizeof(s->text) - 1;

    for (int i = 0; i < n; i++) s->text[i] = (char)pdu[idx + i];
    s->text[n] = '\0';
    s->text_len = n;
    return RC_OK;
}

/* ------------------------ main entry point ------------------------ */

int decode_sms_deliver_pdu(const char *pdu_hex, sms_decoded_t *s)
{
    /* Reduced buffer from 512 to 256 for 8-bit MCU stack safety */
    uint8_t pdu[256]; 

    int pdu_len = hex_to_bytes(pdu_hex, pdu, (int)sizeof(pdu));
    if (pdu_len < 0) return pdu_len;

    *s = (sms_decoded_t){0};

    int idx = 0;

    /* SMSC */
    if (idx >= pdu_len) return RC_SMSC_OOB;
    uint8_t smsc_len = pdu[idx++];
    if (idx + smsc_len > pdu_len) return RC_SMSC_LEN_OOB;
    idx += smsc_len;

    /* First octet of TPDU */
    if (idx >= pdu_len) return RC_FO_OOB;
    uint8_t fo = pdu[idx++];
    uint8_t mti = (uint8_t)(fo & 0x03);

    switch (mti) {
        case 0: return decode_deliver_tpdu(pdu, pdu_len, idx, fo, s);
        case 1: return decode_submit_tpdu(pdu, pdu_len, idx, fo, s);
        case 2: return decode_status_report_tpdu(pdu, pdu_len, idx, fo, s);
        case 3: return decode_command_tpdu(pdu, pdu_len, idx, fo, s);
        default: return RC_MTI_UNSUPPORTED;
    }
}
