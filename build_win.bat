@rem batch script, run in 'Visual Studio Developer Prompt'

@rem 

@echo off

cl /nologo /c /O2 /W3 circlet.c
@if errorlevel 1 goto :BUILDFAIL

cl /nologo /c /O2 /W3 mongoose.c
@if errorlevel 1 goto :BUILDFAIL

@rem TODO find the janet lib location on the system
link /nologo /dll ..\..\janet.lib /out:circletc.dll *.obj
if errorlevel 1 goto :BUILDFAIL

@echo .
@echo ======
@echo Build Succeeded.
@echo =====
exit /b 0

:BUILDFAIL
@echo .
@echo =====
@echo BUILD FAILED. See Output For Details.
@echo =====
@echo .
exit /b 1
