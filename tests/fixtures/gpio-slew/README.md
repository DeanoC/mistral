# Slew-rate reference artifacts

Gzip-compressed RBFs used by `tests/run-multibit-bool-oracle.py`.
`manifest.json` records the SHA-256 and byte length of each **decompressed**
file.  The runner programs no hardware.

| Files | Provenance |
| --- | --- |
| `slew-fast.rbf.gz`, `slew-slow.rbf.gz` | Quartus Prime Lite 17.0.2, `5CSEBA6U23I7`, `assign y = a;` with `a` on PIN_V12 and `y` on PIN_AD26 (3.3-V LVTTL).  The slow reference adds `set_instance_assignment -name SLEW_RATE 0 -to y`. |

The pair differs only in the generated JTAG identifier and both bits of the
pad's two-bit `slew_rate_slow` field (PRAM 04.01178 and 04.01179).  The
comparator clears the identifier, sets `SLEW_RATE_SLOW` on the fast
reference and requires a byte-identical serialized configuration.
