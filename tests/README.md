# DSP port round trips

`dsp-ports.cc` checks every DATAIN and RESULT port on every DSP site of the
selected model. It catches missing upper routing tiles at the end of BEL
spans. With a standalone Mistral CMake build in `build/`, run from the source
root:

```sh
c++ -std=c++14 -Ilibmistral -Ibuild/tools -Ibuild/libmistral \
  tests/dsp-ports.cc build/libmistral/libmistral.a -llzma -o build/dsp-ports
build/dsp-ports 5CSEBA6U23I7
```

For the library built inside nextpnr, replace `build/tools` and
`build/libmistral` with the nextpnr build's `mistral/tools` and
`mistral/libmistral` directories. No hardware is accessed.

The target device has 112 physical DSP blocks and 20,384 checked ports.
The base `bfa096c1deac6180a3eee784693c28dac491ab18` fails 182 round trips;
marking the upper tile when inserting each DSP base resolves those failures.

# LAB/MLAB clock field oracle

The original tables interchanged `CLK0/1/2_INV` and `CLK0/1/2_SEL` physical
addresses. Setting inversion instead selected CLKB, leaving a FF without a
clock when only CLKA was routed. Corrected tables keep the public field names
and defaults, but associate them with the proper configuration bits.
`MISTRAL_CORRECT_LAB_CLOCK_MUXES` lets consumers reject inverted clocks when
built against an older library.

Quartus 17.0.2 fixed-placement positive/negative references with three
independently enabled `cyclonev_ff` instances isolate every inversion bit.
The two RBFs differ only in those bits and the generated JTAG identifier.
The test loads the positive reference, sets all three `CLKx_INV` fields,
normalizes JTAG_ID in both models and requires byte-for-byte equality after
serialization. This checks against Quartus, not a self-consistent decompiler.

Run each Quartus build in a separate empty directory, replacing `MLAB` with
`LAB` to exercise the other block type:

```sh
quartus_sh -t /absolute/path/tests/lab-clock-oracle.tcl pos MLAB
quartus_sh --flow compile top
# In another directory:
quartus_sh -t /absolute/path/tests/lab-clock-oracle.tcl neg MLAB
quartus_sh --flow compile top
```

Build and run the comparator from the source root:

```sh
c++ -std=c++14 -Ilibmistral -Ibuild/tools -Ibuild/libmistral \
  tests/lab-clock-oracle.cc build/libmistral/libmistral.a -llzma -o build/lab-clock-oracle
build/lab-clock-oracle /path/pos/output_files/top.rbf /path/neg/output_files/top.rbf MLAB 3
```

The fixtures use MLAB X8/Y32 or LAB X7/Y32 on `5CSEBA6U23I7`. The original
`328cfb8046d6bcb979fa69df7cfb95bd6f7e73f8` fails; corrected tables pass for both
block types. Separate 25%/75% mixed-edge references and the DE10-Nano
functional capture diagnostic confirm the affected path. This is not a
measurement of PLL duty accuracy or setup/hold margins on hardware.
