#!/usr/bin/env python3
import sys
import re

if len(sys.argv) != 3:
    print(f"usage: {sys.argv[0]} <mapfile> <output.c>")
    sys.exit(1)

mapfile, outfile = sys.argv[1], sys.argv[2]

entries = []
in_text = False

with open(mapfile) as f:
    for line in f:
        if ".text" in line and line.strip().endswith(".text"):
            in_text = True
            continue
        if ".rodata" in line:
            break
        if not in_text:
            continue
        # match lines like: " a7b      a7b     196b     1                 main"
        m = re.match(r"\s*([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+\d+\s+(\S+)$", line)
        if m:
            addr = int(m.group(1), 16)
            name = m.group(2)
            # skip synthetic names if you want
            if name.startswith("bin") or name.endswith(".o:"):
                continue
            entries.append((addr, name))

with open(outfile, "w") as out:
    out.write("/* auto-generated from map file */\n")
    out.write("const struct function_table function_table[] = {\n")
    for addr, name in entries:
        out.write(f"  {{ 0x{addr:04x}, \"{name}\" }},\n")
    out.write("};\n")
    out.write(f"const unsigned function_table_count = {len(entries)};\n")
