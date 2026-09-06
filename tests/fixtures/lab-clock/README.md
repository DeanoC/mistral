# Clock-field reference artifacts

These gzip-compressed RBFs are versioned inputs to the whole-RBF regression.
`manifest.json` records SHA-256 and byte length of each **decompressed** file.
Run `python3 tests/run-lab-clock-oracle.py build/lab-clock-oracle` from the
repository root after building the comparator as described in the tests README.
The runner programs no hardware.

| Files | Provenance |
| --- | --- |
| `lab-pos.rbf.gz`, `lab-neg.rbf.gz` | Quartus Prime Lite 17.0.2, `5CSEBA6U23I7`, three independently enabled FFs fixed in LAB X7/Y32. |
| `mlab-pos.rbf.gz`, `mlab-neg.rbf.gz` | Same compiler/device, FFs fixed in MLAB X8/Y32. |
| `duty-25-diagnostic.rbf.gz` | OSS functional hardware diagnostic described below; checksum-verified but not used as a Quartus oracle. |

The four Quartus references were generated from `tests/lab-clock-pos.v`,
`tests/lab-clock-neg.v` and the matching assignments in
`tests/lab-clock-oracle.tcl`, included at commit
`b28e30a36b5139aaed5a5d361a30b542e6b7c758`. Each pair differs only in the three
physical inversion bits and the generated JTAG identifier. The comparator
normalizes that identifier before comparing complete serialized configurations.
The tests README includes commands to regenerate both pairs.

The diagnostic uses nextpnr's `mistral/tests/pll/duty.v` at 25% duty, with
nextpnr `69d64eadc535ac0e803d1567c2687f781ab232c0`, Mistral
`b28e30a36b5139aaed5a5d361a30b542e6b7c758` and Yosys
`13b43f8c85ec430a33ee55d058fb4c32b42b6910`. It was checked on the designated
DE10-Nano on 2026-09-06. `hardware-probe.log` records 20 input changes with
signature `0xD718`, lock asserted at GPI[1], and capture at GPI[0] following
GPO[0]. This is functional evidence, not an analog duty/phase measurement.
The artifact is provided for inspection; running the host regression does
not load it onto a device.
