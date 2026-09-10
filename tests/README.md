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
