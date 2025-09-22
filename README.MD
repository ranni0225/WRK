# README


## Pre-BUILD

> [!CAUTION]
> Please ensure there are **no** space characters in your project path, or you may encounter errors like `fatal error U1077`.

Create necessary directories **before** building WRK and ProjectOZ: `python3 PREBUILD.PY`.


## Build WRK

1. Open `WRK-V1.2/WRK.sln` with Visual Studio 2022. Visual Studio 2026 may also work, but I haven't tested.

2. Build solution with pre-defined "x86"(Windows Server 2003 SP1) configuration.

3. Copy binary files to `C:/Windows/system32` directory.

    - binary files can be found here: `WRK-V1.2/BASE/NTOS/BUILD/EXE`

4. Copy **correct** `hal.sys` to `C:/Windows/system32` directory.

    - path: `WRK-V1.2/WS03SP1HALS/X86`

5. Add WRK entries in `C:/BOOT.INI`.

    - example: `WRK-V1.2/WS03SP1HALS/BOOT.INI`


## Build ProjectOZ

1. Modify the `wrk` variable to your WRK path.

    - path: `ProjectOZ/makefile`, Line #8

2. Execute `ProjectOZ/BUILD.BAT`.

    - binary files can be found here:`ProjectOZ/runx86`.
