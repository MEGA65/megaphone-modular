
unsigned long utf8_next_codepoint(unsigned char **s)
{
  unsigned char *p;
  unsigned long cp = 0;

  if (!s || !(*s)) return 0L;

  p = *s;
  
  if (p[0] < 0x80) {
    cp = p[0];
    (*s)++;
    return cp;
  }

  // 2-byte sequence: 110xxxxx 10xxxxxx
  if ((p[0] & 0xE0) == 0xC0) {
    if ((p[1] & 0xC0) != 0x80) return 0xFFFDL; // invalid continuation
    cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
    *s += 2;
    return cp;
  }

  // 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx
  if ((p[0] & 0xF0) == 0xE0) {
    if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80) return 0xFFFDL;
    cp = ((unsigned long)(p[0] & 0x0F) << 12) |
      ((unsigned long)(p[1] & 0x3F) << 6) |
         (p[2] & 0x3F);
    *s += 3;
    return cp;
  }

  // 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
  // e.g., f0 9f 8d 92
  if ((p[0] & 0xF8) == 0xF0) {
    if ((p[1] & 0xC0) != 0x80 || (p[2] & 0xC0) != 0x80 || (p[3] & 0xC0) != 0x80)
      return 0xFFFDL;
    cp = (((unsigned long)(p[0] & 0x07)) << 18) |
      (((unsigned long)(p[1] & 0x3F)) << 12) |
       (((unsigned long)(p[2] & 0x3F)) << 6) |
      (p[3] & 0x3F);
    *s += 4;
    return cp;
  }

  // Invalid or unsupported UTF-8 byte
  (*s)++;
  return 0xFFFDL;
}
