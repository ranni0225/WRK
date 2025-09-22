PUSHD %~dp0..
SET wrk=%CD%\WRK-V1.2
SET projectoz=%CD%\ProjectOZ
POPD

PATH %wrk%\TOOLS\X86;%PATH%
nmake -nologo x86=
