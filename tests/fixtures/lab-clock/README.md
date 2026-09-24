# Clock-field reference artifacts

Gzip-compressed RBFs used by the whole-RBF regression in `tests/`.
`manifest.json` records the SHA-256 and byte length of each **decompressed**
file.  The runner programs no hardware.

| Files | Provenance |
| --- | --- |
| `lab-pos.rbf.gz`, `lab-neg.rbf.gz` | Quartus Prime Lite 17.0.2, `5CSEBA6U23I7`, three independently enabled FFs fixed in LAB X7/Y32. |
| `mlab-pos.rbf.gz`, `mlab-neg.rbf.gz` | Same compiler and device, FFs fixed in MLAB X8/Y32. |

The references were generated from `tests/lab-clock-pos.v`,
`tests/lab-clock-neg.v` and `tests/lab-clock-oracle.tcl`.  Each pair differs
only in the three physical inversion bits and the generated JTAG identifier.
The comparator clears that identifier before comparing complete serialized
configurations.
