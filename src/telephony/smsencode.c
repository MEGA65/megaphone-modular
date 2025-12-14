/* ---------------------------------------------------------------------
   SMS PDU Encoding Routines (Embedded Optimized)
   Developed using Gemini LLM
   Target: 8-bit MCU, EC25 Modem
   Features: UTF-8 input, Auto-Switch (7-bit/UCS-2), Concatenation
   ---------------------------------------------------------------------
*/

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "includes.h"

#include "shstate.h"
#include "dialer.h"
#include "screen.h"
#include "records.h"
#include "contacts.h"
#include "buffers.h"
#include "af.h"
#include "modem.h"
#include "smsdecode.h"

/* --- Configuration --- */
#define MAX_PDU_SIZE    180  /* Max valid SMS PDU is ~176 bytes */
#define MAX_HEX_SIZE    (MAX_PDU_SIZE * 2 + 1)

char send_at_command(const char *cmd, const char *pdu_payload)
{
  fprintf(stderr,"DEBUG: Would send '%s','%s'\n",cmd,pdu_payload);
  modem_uart_write((unsigned char *)cmd,strlen(cmd));
  modem_uart_write((unsigned char *)"\r\n",2);
  shared.modem_saw_error = 0;
  shared.modem_saw_ok = 0;
  fprintf(stderr,"DEBUG: Waiting for >\n");
  shared.modem_line[0]=0;
  while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
    modem_poll();
    if (!strncmp((char *)shared.modem_line,"+CMS ERROR",10)) {
      fprintf(stderr,"DEBUG: CMS Error: '%s'\n",(char *)shared.modem_line);
      return 0xfe;
    }
    
    if (shared.modem_line[0]=='>') {
      fprintf(stderr,"DEBUG: Saw > prompt\n");
      modem_uart_write((unsigned char *)pdu_payload,strlen(pdu_payload));
      unsigned char terminator = 0x1a;
      modem_uart_write(&terminator,1);
      // Wait for OK or error
      fprintf(stderr,"DEBUG: Waiting for OK/ERROR after send\n");
      while(!(shared.modem_saw_ok|shared.modem_saw_error)) {
	modem_poll();
	if (!strncmp((char *)shared.modem_line,"+CMS ERROR",10)) {
	  fprintf(stderr,"DEBUG: CMS Error: '%s'\n",(char *)shared.modem_line);
	  return 0xfe;
	}
      }
      fprintf(stderr,"DEBUG: Saw %s %s\n",
	      shared.modem_saw_ok?"OK":"",
	      shared.modem_saw_error?"ERROR":"");
      return shared.modem_saw_error;
    }
  }

  // If we saw OK or ERROR, then something bad happened
  fprintf(stderr,"DEBUG: Sending failed\n");
  return 0xff;
}

/* --- Static Allocation (Save Stack) --- */
/* These persist between calls. Not thread-safe, but fine for single-threaded MCU */
static uint8_t  g_pdu[MAX_PDU_SIZE];
static char     g_pdu_hex[MAX_HEX_SIZE];
static uint8_t  g_septets[160];       /* Intermediate buffer for 7-bit packing */
static char     g_cmd_buf[16];        /* Buffer for "AT+CMGS=..." */

/* ----------------------------- Helpers ----------------------------- */

char hex_digit(uint8_t v) {
    return (v < 10) ? '0' + v : 'A' + (v - 10);
}

void bytes_to_hex(const uint8_t *in, uint8_t len, char *out) {
    uint8_t i;
    for (i = 0; i < len; i++) {
        uint8_t val = in[i];
        *out++ = hex_digit(val >> 4);
        *out++ = hex_digit(val & 0x0F);
    }
    *out = '\0';
}

/* ----------------------------- GSM 03.38 Mapping ----------------------------- */

/* Returns 0xFFFF if not supported in 7-bit alphabet */
uint16_t unicode_to_gsm(unsigned long cp) {
    /* Fast path for standard ASCII overlap */
    if (cp >= 'A' && cp <= 'Z') return (uint16_t)cp;
    if (cp >= 'a' && cp <= 'z') return (uint16_t)cp;
    if (cp >= '0' && cp <= '9') return (uint16_t)cp;
    if (cp == ' ') return 0x20;

    /* Common Symbols & Punctuation */
    switch (cp) {
        case '@': return 0x00;
        case '$': return 0x02;
        case '\n': return 0x0A;
        case '\r': return 0x0D;
        case '_': return 0x11;
        /* European High-Bit Chars */
        case 0x00A3: return 0x01; /* £ */
        case 0x00A5: return 0x03; /* ¥ */
        case 0x00E8: return 0x04; /* è */
        case 0x00E9: return 0x05; /* é */
        case 0x00F9: return 0x06; /* ù */
        case 0x00EC: return 0x07; /* ì */
        case 0x00F2: return 0x08; /* ò */
        case 0x00C7: return 0x09; /* Ç */
        case 0x00D8: return 0x0B; /* Ø */
        case 0x00F8: return 0x0C; /* ø */
        case 0x00C5: return 0x0E; /* Å */
        case 0x00E5: return 0x0F; /* å */
        case 0x0394: return 0x10; /* Δ */
        case 0x03A6: return 0x12; /* Φ */
        case 0x0393: return 0x13; /* Γ */
        case 0x039B: return 0x14; /* Λ */
        case 0x03A9: return 0x15; /* Ω */
        case 0x03A0: return 0x16; /* Π */
        case 0x03A8: return 0x17; /* Ψ */
        case 0x03A3: return 0x18; /* Σ */
        case 0x0398: return 0x19; /* Θ */
        case 0x039E: return 0x1A; /* Ξ */
        case 0x00C6: return 0x1C; /* Æ */
        case 0x00E6: return 0x1D; /* æ */
        case 0x00DF: return 0x1E; /* ß */
        case 0x00C9: return 0x1F; /* É */
        case 0x00A4: return 0x24; /* ¤ */
        case 0x00A1: return 0x40; /* ¡ */
        case 0x00C4: return 0x5B; /* Ä */
        case 0x00D6: return 0x5C; /* Ö */
        case 0x00D1: return 0x5D; /* Ñ */
        case 0x00DC: return 0x5E; /* Ü */
        case 0x00A7: return 0x5F; /* § */
        case 0x00BF: return 0x60; /* ¿ */
        case 0x00E4: return 0x7B; /* ä */
        case 0x00F6: return 0x7C; /* ö */
        case 0x00F1: return 0x7D; /* ñ */
        case 0x00FC: return 0x7E; /* ü */
        case 0x00E0: return 0x7F; /* à */

        /* Extension Table (ESC + char) - encoded as 0x1Bxx */
        case 0x20AC: return 0x1B65; /* € */
        case '^':    return 0x1B14;
        case '{':    return 0x1B28;
        case '}':    return 0x1B29;
        case '\\':   return 0x1B2F;
        case '[':    return 0x1B3C;
        case '~':    return 0x1B3D;
        case ']':    return 0x1B3E;
        case '|':    return 0x1B40;
    }

    if (cp >= 0x20 && cp <= 0x7E) return (uint16_t)cp;
    return 0xFFFF; /* Fallback for pure Unicode */
}

/* Encodes Destination Address (DA) in BCD semi-octets */
uint8_t encode_address(const char *number, uint8_t *out) {
    uint8_t len = 0;
    uint8_t i = 0;
    uint8_t out_idx = 2;
    uint8_t digit_count = 0;

    /* Count length and skip '+' */
    const char *p = number;
    if (*p == '+') p++;
    while (p[len]) len++;

    out[0] = len;      /* Address Length (digits) */
    out[1] = 0x91;     /* Type: International, ISDN */

    for (i = 0; i < len; i++) {
        uint8_t val = p[i] - '0';
        if ((digit_count & 1) == 0) {
            out[out_idx] = val; /* Low nibble */
        } else {
            out[out_idx++] |= (val << 4); /* High nibble */
        }
        digit_count++;
    }
    /* Fill padding F if odd number of digits */
    if (digit_count & 1) {
        out[out_idx++] |= 0xF0;
    }
    return out_idx; /* Total bytes written */
}

/* ----------------------------- Main Routine ----------------------------- */

/* receiver: e.g. "+61400000000"
   utf8_msg: Input string
   ref_num:  0-255 incrementing ID for multipart tracking
*/
char sms_send_utf8(const char *receiver, const char *utf8_msg, uint8_t ref_num) {
    bool force_ucs2 = false;
    uint16_t char_count = 0;
    uint16_t gsm_septet_count = 0;
    uint8_t max_chars_per_msg;
    bool concat = false;
    uint8_t total_parts = 1;
    uint8_t current_part = 1;
    
    /* 1. Analyze Content (Pass 1) */
    unsigned char *u_ptr = (unsigned char *)utf8_msg;
    unsigned char *u_start = u_ptr; /* Keep start for Pass 2 */
    
    while (*u_ptr) {
        unsigned long cp = utf8_next_codepoint(&u_ptr);
        uint16_t gsm = unicode_to_gsm(cp);
        
	if (gsm == 0xFFFF) {
	  force_ucs2 = true;
	} else if (gsm > 0xFF) {
	  gsm_septet_count += 2;
	} else {
	  gsm_septet_count++;
	}
	
	// NEW: Count Astral Plane chars (Emojis) as 2 UCS-2 words
	if (cp > 0xFFFF) {
	  char_count += 2;
	} else {
	  char_count++;
	}

    }

    /* 2. Determine Splits */
    if (force_ucs2) {
        /* UCS-2 Limits: 70 single, 67 concat */
        if (char_count > 70) {
            concat = true;
            max_chars_per_msg = 67;
            total_parts = (char_count + 66) / 67;
        } else {
            max_chars_per_msg = 70;
        }
    } else {
        /* GSM Limits: 160 single, 153 concat */
        if (gsm_septet_count > 160) {
            concat = true;
            max_chars_per_msg = 153; /* Septets */
            total_parts = (gsm_septet_count + 152) / 153;
        } else {
            max_chars_per_msg = 160;
        }
    }

    /* 3. Encode & Send Loop (Pass 2) */
    u_ptr = u_start; /* Reset to start */
    
    while (current_part <= total_parts) {
        uint8_t idx = 0;
        
        /* --- HEADER --- */
        g_pdu[idx++] = 0x00; /* SCA (Default SMSC) */
        g_pdu[idx++] = 0x01 | (concat ? 0x40 : 0x00); /* FO: Submit + UDHI? */
        g_pdu[idx++] = 0x00; /* MR */
        
        idx += encode_address(receiver, &g_pdu[idx]); /* DA */
        
        g_pdu[idx++] = 0x00; /* PID */
        g_pdu[idx++] = force_ucs2 ? 0x08 : 0x00; /* DCS */
        
        uint8_t udl_idx = idx++; /* Remember UDL location */
        
        /* --- PAYLOAD SETUP --- */
        uint8_t *ud = &g_pdu[idx];
        uint8_t ud_len = 0;
        
        if (concat) {
            ud[ud_len++] = 0x05; /* UDH Length */
            ud[ud_len++] = 0x00; /* IEI: Concat */
            ud[ud_len++] = 0x03; /* IEL: 3 bytes */
            ud[ud_len++] = ref_num;
            ud[ud_len++] = total_parts;
            ud[ud_len++] = current_part;
        }
        
        /* --- TEXT PACKING --- */
        if (force_ucs2) {
            uint8_t chars_this_part = 0;

	    while (chars_this_part < max_chars_per_msg && *u_ptr) {
                // 1. Peek at the next code point size
                unsigned char *peek_ptr = u_ptr;
                unsigned long next_cp = utf8_next_codepoint(&peek_ptr);
                uint8_t char_cost = (next_cp > 0xFFFF) ? 2 : 1;

                // 2. Check if it fits
                if (chars_this_part + char_cost > max_chars_per_msg) {
                    break; // Doesn't fit, push to next SMS part
                }

                // 3. Process normally
                unsigned long cp = utf8_next_codepoint(&u_ptr); // Actually advance now
                
                if (cp > 0xFFFF) {
                    cp -= 0x10000;
                    uint16_t high = 0xD800 + (cp >> 10);
                    uint16_t low  = 0xDC00 + (cp & 0x3FF);
                    
                    ud[ud_len++] = (high >> 8) & 0xFF;
                    ud[ud_len++] = high & 0xFF;
                    ud[ud_len++] = (low >> 8) & 0xFF;
                    ud[ud_len++] = low & 0xFF;
                    
                    chars_this_part += 2;
                } else {
                    ud[ud_len++] = (cp >> 8) & 0xFF;
                    ud[ud_len++] = cp & 0xFF;
                    chars_this_part++;
                }
            }
	    
            g_pdu[udl_idx] = ud_len; /* UDL is octets for UCS2 */
        } 
        else {
            /* GSM 7-bit packing */
            /* We use g_septets buffer to store unpacked 7-bit values */
            uint8_t septet_cnt = 0;
            
            while (septet_cnt < max_chars_per_msg && *u_ptr) {
                /* Peek ahead mechanism needed because utf8_next advances */
                unsigned char *peek = u_ptr;
                unsigned long cp = utf8_next_codepoint(&peek);
                uint16_t gsm = unicode_to_gsm(cp);
                
                if (gsm > 0xFF) { /* Double septet char */
                    if (septet_cnt + 2 > max_chars_per_msg) break; /* Fits in next part */
                    g_septets[septet_cnt++] = 0x1B;
                    g_septets[septet_cnt++] = gsm & 0xFF;
                    u_ptr = peek; /* Commit advance */
                } else {
                    if (septet_cnt + 1 > max_chars_per_msg) break;
                    g_septets[septet_cnt++] = gsm & 0xFF;
                    u_ptr = peek; /* Commit advance */
                }
            }

            /* Calculate Alignment */
            uint8_t bit_offset = 0;
            if (concat) {
                /* UDH = 6 bytes (48 bits). Next septet starts at bit 49. */
                bit_offset = 49; 
                /* UDL includes the padding septets */
                g_pdu[udl_idx] = septet_cnt + 7; 
            } else {
                g_pdu[udl_idx] = septet_cnt;
            }

            /* Bit Packing into 'ud' (which already contains UDH if concat) */
            /* Optimization: Reuse 'i' for septet index */
            uint8_t s;
            for (s = 0; s < septet_cnt; s++) {
                uint16_t bitpos = bit_offset + (s * 7);
                uint8_t bytepos = bitpos >> 3; /* / 8 */
                uint8_t shift = bitpos & 7;    /* % 8 */
                
                uint8_t val = g_septets[s];
                
                /* Combine bits. Note: ud[bytepos] might already have data */
                ud[bytepos] |= (val << shift);
                
                /* Spill over to next byte */
                if (shift > 1) { 
                    /* If shift=1, we use 7 bits (1..7), fits exactly in byte.
                       If shift>1, we spill. */
                    ud[bytepos + 1] = (val >> (8 - shift));
                }
            }
            
            /* Final Byte Length calculation */
            uint16_t total_bits = bit_offset + (septet_cnt * 7);
            ud_len = (total_bits + 7) >> 3;
        }

        /* --- SENDING --- */
        uint8_t total_len = idx + ud_len;
        
        /* 1. Generate Hex */
        bytes_to_hex(g_pdu, total_len, g_pdu_hex);
        
        /* 2. Format AT Command */
        /* AT+CMGS=<length> where length is octets EXCLUDING SMSC part (first byte) */
        sprintf(g_cmd_buf, "AT+CMGS=%d", total_len - 1);
        
        /* 3. Call Hardware Stub */
        if (send_at_command(g_cmd_buf, g_pdu_hex)) return 0xff;
        
        /* Prepare next loop */
        /* Zero out buffers for safety? Not strictly needed if logic is tight, 
           but good for partial bytes in 7-bit packing */
        if (!force_ucs2) memset(g_pdu, 0, MAX_PDU_SIZE); 
        
        current_part++;

	// Be nice to the modem if sending multiple parts
	if (current_part < total_parts) sleep(1);
    }

    return 0;
}
