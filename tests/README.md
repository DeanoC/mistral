# Tests

Build the library as usual, then either `ctest` from the build directory or
the commands below.  No hardware is accessed.

## DSP port round trips

`dsp-ports` walks every DATAIN and RESULT port on every DSP site of the
selected model.  It fails when the upper routing tile is missing at the end
of a BEL span.

```sh
cmake -S . -B build
cmake --build build -j$(nproc)
build/tests/dsp-ports 5CSEBA6U23I7
```

`5CSEBA6U23I7` has 112 DSP blocks.  Without marking `T_DSP2` on the row
above each base, reverse lookup fails for ports that live on that upper
tile when the span ends on the base row.

## LAB/MLAB clock field oracle

The original mux tables interchanged the physical addresses of
`CLK0/1/2_INV` and `CLK0/1/2_SEL`.  Setting inversion selected CLKB instead,
leaving a flip-flop without a clock when only CLKA was routed.  The corrected
tables keep the public field names and defaults.  `MISTRAL_CORRECT_LAB_CLOCK_MUXES`
lets consumers detect the corrected mapping.

Quartus 17.0.2 fixed-placement positive/negative references with three
independently enabled `cyclonev_ff` instances isolate every inversion bit.
The two RBFs differ only in those bits and the generated JTAG identifier.
The test loads the positive reference, sets all three `CLKx_INV` fields,
normalizes `JTAG_ID`, and requires byte-for-byte equality after serialization.

Compressed references and a SHA-256 manifest live in
[fixtures/lab-clock](fixtures/lab-clock/README.md):

```sh
python3 tests/run-lab-clock-oracle.py build/tests/lab-clock-oracle
```

To regenerate the references with Quartus 17.0.2, run each build in a
separate empty directory:

```sh
quartus_sh -t /absolute/path/tests/lab-clock-oracle.tcl pos MLAB
quartus_sh --flow compile top
# In another directory:
quartus_sh -t /absolute/path/tests/lab-clock-oracle.tcl neg MLAB
quartus_sh --flow compile top
```

The fixtures use MLAB X8/Y32 or LAB X7/Y32 on `5CSEBA6U23I7`.
