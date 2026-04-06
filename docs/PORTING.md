# SDR++ Porting Notes

This repository started as an SDR# plugin written in C# against `SDRSharp.Common` and `SDRSharp.Radio`.

## What changed on `sdrpp-tetra-plugin`

- Added a native SDR++ module entry point in `src/main.cpp`.
- Added a standalone `CMakeLists.txt` that builds the module against an SDR++ source tree.
- Added SDR++ config persistence for mode, bandwidth, sample rate, encrypted-speech handling, and UDP settings.
- Hooked the module up to a real SDR++ VFO and IQ stream so the host integration is native to SDR++.
- Ported the native decoder foundation for bit handling, timing, CRC, depuncturing, descrambling, convolutional decoding, and RM(30,14) decoding into `src/tetra_*.{hpp,cpp}`.

## What is still legacy

The actual TETRA DSP/parser implementation is still the original SDR# code and remains in the repository as reference:

- `TetraPanel.cs`
- `TetraDecoder.cs`
- `Decoder/*.cs`
- `Parsers/*.cs`
- `Model/*.cs`

These files are tightly coupled to:

- WinForms
- `ISharpPlugin` / `ISharpControl`
- `SDRSharp.Radio` buffers, streams, filters, and resamplers

## Next native SDR++ steps

1. Port the burst demodulator from `Decoder/Demodulator.cs` to a native SDR++ DSP block.
2. Port the PHY/MAC parsing chain from `Decoder/*.cs` and `Parsers/*.cs`.
3. Replace the old WinForms state/UI in `TetraPanel.cs` with ImGui panels inside `src/main.cpp`.
4. Reconnect audio output to an SDR++ sink stream once the native voice path exists.
