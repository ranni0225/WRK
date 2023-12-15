# README

0. ensure there is no space character in your path (fatal error U1077, return code 0x2)

1. create build directories by executing `python3 PREBUILD.py`

2. open "WRK.sln" with Visual Studio 2022 (or Visual Studio 2019)

3. build solution with pre-defined "x86" (for Windows Server 2003 SP1) configuration

4. copy wrkx86.exe ("WRK-V1.2\BASE\NTOS\BUILD\EXE\wrkx86.exe") and correct hal.sys ("WRK-V1.2\WS03SP1HALS\X86") to "C:\Windows\system32"

5. add WRK entries into "C:\boot.ini" (an example can be found at "WRK-V1.2\WS03SP1HALS\BOOT.INI")
