@ECHO OFF

IF NOT EXIST %~dp0\lib_x64\SDL2.DLL (
  %~dp0\MSVC_DEPS.EXE -y -o%~dp0
) ELSE (
  ECHO Files already prepared.
)

EXIT /B