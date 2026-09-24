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

## Routing mux CRAM coordinates

`routing-mux-cram` checks the physical configuration footprint of seven
routing muxes isolated from an outside-slot ZX81 composition difference,
plus an inside-slot mux. Logical wire tile coordinates do not necessarily
identify the tile containing its programmable bits. The test checks exact
coordinates, half-open boundaries, output replacement, unknown nodes and a
fixed connection with no programmable mux.

```sh
build/tests/routing-mux-cram
```

Coordinates from `rnode_mux_cram_bits`, `rnode_inverter_cram_bit`, and
`rnode_cram_footprint` are mistral's decoded CRAM `(x, y)` on a
`cram_sx` by `cram_sy` grid, the same space `CycloneV::diff` prints. They
are not RBF frame/byte/bit offsets.

`rnode_mux_cram_bits` is only the programmable routing-mux selector.
`rnode_inverter_cram_bit` is the separate programmable routing inverter
written by `inv_set` / `inv_default_set` (`pos_and_def` with the default
nibble cleared, then split by `cram_sx`). On `5CSEBA6U23I7` that bit is
outside the mux's own bounding box for 1,987 nodes, by as much as 3 CRAM
positions (`GOUT.001.000.0021` is `(64, 43)`, one row below its mux).
`rnode_cram_footprint` is the union of the two. A full frozen-shell check
also needs block mux/BEL configuration, including dcram-indexed bits, for
blocks in or near the slot.

`routing-inverter-cram` checks that split on `5CSEBA6U23I7`. It counts the
11,895 inverter nodes, checks the `GOUT.001.000.0021` coordinate, rejects
an unknown node, and returns an empty result for nodes with no inverter.
The oracle clears to the model's default, flips each inverter with
`inv_set`, and reads the written coordinate back from `CycloneV::diff`.
Pass `--sample N` to oracle every Nth inverter instead of all of them.

```sh
build/tests/routing-inverter-cram
build/tests/routing-inverter-cram --sample 20
```

This is an offline table/accessor regression. It does not establish physical
hardware acceptance or replace the final bitstream boundary comparison.

## LAB/MLAB block-mux CRAM coordinates

`bmux_cram_bits` is the non-dcram block-mux path for one LAB or MLAB field
instance. `midx` is 0 for a lab-global field and `0 .. span-1` for a per-ALM
field. Coordinates are the same decoded CRAM `(x, y)` grid as
`CycloneV::diff`. M10K, DSP, HPS clocks, dcram, pram, and oram return false
from this slice. `MISTRAL_BMUX_CRAM_BITS` is the feature macro.

`bmux-cram-bits` checks that contract on `5CSEBA6U23I7`. The oracle writes
every LAB field and every MLAB field through `bmux_b_set`, `bmux_m_set`,
`bmux_n_set`, or `bmux_r_set` and requires the CRAM coordinates from
`CycloneV::diff` to equal the query. It also rejects an empty tile, a M10K
tile, a bad `midx`, and an MLAB-only mux asked of a LAB.

```sh
build/tests/bmux-cram-bits
```

This is an offline table/accessor regression. It does not establish physical
hardware acceptance or replace the final bitstream boundary comparison.
