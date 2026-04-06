# SDR-Tetra-Plugin

This branch adds a native SDR++ module scaffold for the old SDR# TETRA plugin and keeps the original C# sources in the repository as the legacy reference implementation.

## Current layout

- `src/main.cpp`: native SDR++ module entry point
- `CMakeLists.txt`: standalone CMake build for SDR++
- `docs/PORTING.md`: migration notes from SDR# to SDR++
- `*.cs`, `Decoder/*.cs`, `Parsers/*.cs`: legacy SDR# implementation

## SDR++ status

The repository is now structured as an SDR++ module:

- native `ModuleManager::Instance` entry point
- SDR++ VFO creation and teardown
- ImGui-based module menu
- persistent module settings
- live IQ stream hook for the future decoder chain
- native decoder foundation for TETRA bit/FEC processing

The TETRA DSP/parser core is not fully ported to native SDR++ yet. The old SDR# code is still present and is the source for the next migration step.

## Build for SDR++

You need an SDR++ source tree with its usual dependencies available.

Example:

```powershell
cmake -S . -B build -DSDRPP_ROOT=C:\path\to\SDRPlusPlus
cmake --build build --config Release
```

The resulting module is named `tetra_decoder`.

## Legacy SDR# code

The original SDR# project is still in the repository:

- `SDRSharp.Plugin.Tetra1.2.csproj`
- `TetraPlugin.cs`
- `TetraPanel.cs`

That code is no longer the primary integration target on this branch.

# Disclaimer

- The program is licensed under MIT.
- Use it at your own risk.
- Check the law in your country before receiving or decoding radio traffic.
- Decoding encrypted traffic without authorization may be illegal.

