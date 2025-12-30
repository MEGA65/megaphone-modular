echo "int8_t ascii_font[4096]={" > src/telephony/ascii-font.c
hexdump -vC asciifont.bin | cut -c10-58 | sed 's/  / /g' | sed 's/ /, 0x/g' | sed 's/^,//g' | sed 's/$/,/g' | head -256 >> src/telephony/ascii-font.c 
echo "};" >> src/telephony/ascii-font.c

