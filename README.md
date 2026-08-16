# dsPIC33 Simulator

Native C simulator and conformance suite for the dsPIC33E architecture and the
dsPIC33EP512MU810 device.

The instruction engine implements dsPIC33E CPU behavior. The current memory
layout, SFR map, interrupt topology, peripherals, and silicon errata model are
specific to the dsPIC33EP512MU810 B1 device revision.

## Build

```powershell
cmake -S . -B build/simulator -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/simulator --parallel
```

The primary targets are:

- `dspic33::simulator`: static simulator library
- `dspic33::firmware_runner`: JSON scenario runner
- `dspic33_*_conformance`: native conformance executables

## Use from CMake

Add this repository as a Git submodule, then include it without adding its
standalone targets to the parent default build:

```cmake
add_subdirectory(third_party/dspic33-sim EXCLUDE_FROM_ALL)
target_link_libraries(your_target PRIVATE dspic33::simulator)
```

Applications that use the supplied scenario runner can build the
`dspic33_firmware_runner` target directly.

## Verify

Run the complete device and processor conformance gate:

```powershell
python tests/verify_simulator.py
```

The gate validates the generated SFR inventory, builds the XC16 conformance
firmware, runs every native conformance executable, and compares the simulator
against the exact conformance image bound to the frozen external-oracle ledger.
The current native CTest suite covers behavior added after that frozen oracle
was recorded.
