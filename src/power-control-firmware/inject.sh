#!/bin/sh
# inject_cfg.sh
# Usage: ./inject_cfg.sh template.vhdl config_message.txt out.vhdl

set -eu

tmpl="${1:?template.vhdl}"
msg="${2:?config_message.txt}"
out="${3:?out.vhdl}"

# Make a CRLF version of the message (without requiring unix2dos)
# - remove any existing CR to avoid doubling
# - append CR before each LF
crlf_tmp="$(mktemp)"
trap 'rm -f "$crlf_tmp"' EXIT

# This ensures LF -> CRLF and strips stray CRs first.
# (If your file already has CRLF, it remains CRLF, not CRCRLF.)
sed 's/\r$//' "$msg" | sed 's/$/\r/' > "$crlf_tmp"

# Generate VHDL initialiser entries: "  0 => x"NN"," etc.
# Use od (portable, reliable). util-linux hexdump is fine too, but od is simpler.
hex_block="$(
  od -An -tx1 -v "$crlf_tmp" | awk '
    BEGIN { n = 0 }
    {
      for (i = 1; i <= NF; i++) {
        printf "      %d => x\"%s\",\n", n, toupper($i)
        n++
      }
    }
  '
)"

# Replace placeholder with generated block.
# Keep indentation sane: the generated lines already start with 6 spaces.
# This replaces only the first occurrence of HEXGOESHERE.
awk -v repl="$hex_block" '
  {
    if (!done && index($0, "HEXGOESHERE")) {
      sub(/HEXGOESHERE/, repl)
      done=1
    }
    print
  }
  END {
    if (!done) {
      print "ERROR: placeholder HEXGOESHERE not found" > "/dev/stderr"
      exit 2
    }
  }
' "$tmpl" > "$out"

echo "Wrote: $out"
