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
