# External Dependencies

This folder contains external dependencies that are required for Haiyajan-UI to compile.

<dl>
  <dt>fonts/</dt>
  <dd>Contains fonts that are built into the final executable.</dd>
  <dt>MSVC/</dt>
  <dd>Contains Visual Studio 2019 project files. This may be used as an alternative to the Makefile.</dd>
  <dt>MSVC_DEPS.exe</dt>
  <dd>A self-extracting executable (SFX) that contains prebuilt libraries required to compile Haiyajan-UI with Visual Studio for Win32 and Win64 platforms.</dd>
  <dt>SFXCon.exe</dt>
  <dd>An empty 7z self-extracting executable (SFX) used to create MSVC_DEPS.exe.</dd>
</dl>

## Creating MSVC_DEPS.exe

These instructions are for Windows. 7z is required.

1. Create a 7z archive of the inc, lib_x64 and lib_x86 folders.
2. Within command prompt (cmd), execute the command `copy /b SFXCon.exe + <archive>.7z MSVC_DEPS.exe`, replacing \<archive\> with the name of the 7z archive you created in step 1.
