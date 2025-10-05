#!/usr/bin/env python3
import sys
import re

USAGE = f"usage: {sys.argv[0]} <mapfile> <output.c>"

if len(sys.argv) != 3:
    print(USAGE)
    sys.exit(1)

mapfile, outfile = sys.argv[1], sys.argv[2]

entries = []
in_text_symbol_dump = False

text_start = None
text_size  = None
ro_start   = None
ro_size    = None

# Regex that matches the section summary lines:
# "  834      834     51d7     1 .text"
sec_re = re.compile(r"^\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+([0-9A-Fa-f]+)\s+\d+\s+(\.\w+)\s*$")

# Regex for your symbol dump lines inside .text:
# " a7b      a7b     196b     1                 main"
sym_re = re.compile(r"\s*([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+\d+\s+(\S+)$")

with open(mapfile, "r") as f:
    for line in f:
        # Section summary lines
        msec = sec_re.match(line)
        if msec:
            vma  = int(msec.group(1), 16)
            size = int(msec.group(2), 16)
            name = msec.group(3)

            if name == ".text":
                text_start = vma
                text_size  = size
                in_text_symbol_dump = True      # start collecting .text symbols
                continue

            if name == ".rodata":
                ro_start = vma
                ro_size  = size
                # we also use hitting .rodata as a natural end for the .text symbol scan
                in_text_symbol_dump = False
                continue

        # Collect .text symbols until we hit .rodata section summary
        if in_text_symbol_dump:
            msym = sym_re.match(line)
            if msym:
                addr = int(msym.group(1), 16)
                name = msym.group(2)
                # Skip synthetic/file markers if any
                if name.startswith("bin") or name.endswith(".o:"):
                    continue
                entries.append((addr, name))

# Basic validation
missing = []
if text_start is None or text_size is None:
    missing.append(".text")
if ro_start is None or ro_size is None:
    missing.append(".rodata")

if missing:
    sys.stderr.write(f"error: could not find required section summary lines for {', '.join(missing)} in {mapfile}\n")
    sys.exit(2)

# Compute end addresses (end = start + size), then split to bytes (lo/hi)
def lo(x): return x & 0xFF
def hi(x): return (x >> 8) & 0xFF

text_end = text_start + text_size
ro_end   = ro_start + ro_size

wp_bytes = [
    lo(text_start), hi(text_start),
    lo(text_end),   hi(text_end),
    lo(ro_start),   hi(ro_start),
    lo(ro_end),     hi(ro_end),
    0x11            # flags
]

with open(outfile, "w") as out:
    out.write("/* auto-generated from map file */\n\n")

    # Function table
    out.write("const struct function_table function_table[] = {\n")
    for addr, name in entries:
        out.write(f"  {{ 0x{addr:04x}, \"{name}\" }},\n")
    out.write("};\n")
    out.write(f"const unsigned int function_table_count = {len(entries)};\n\n")

    # WP register bytes (ready to lcopy in one shot)
    out.write("/* write-protect register bytes computed from linker map */\n")
    out.write("const unsigned char __wp_regs[9] = {\n")
    out.write(f"  0x{wp_bytes[0]:02x}, 0x{wp_bytes[1]:02x},  /* .text start  = 0x{text_start:04x} */\n")
    out.write(f"  0x{wp_bytes[2]:02x}, 0x{wp_bytes[3]:02x},  /* .text end    = 0x{text_end:04x} (start+size) */\n")
    out.write(f"  0x{wp_bytes[4]:02x}, 0x{wp_bytes[5]:02x},  /* .rodata start= 0x{ro_start:04x} */\n")
    out.write(f"  0x{wp_bytes[6]:02x}, 0x{wp_bytes[7]:02x},  /* .rodata end  = 0x{ro_end:04x} (start+size) */\n")
    out.write(f"  0x{wp_bytes[8]:02x}                       /* flags        */\n")
    out.write("};\n")



