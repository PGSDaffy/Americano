@echo off
setlocal enabledelayedexpansion
set PATH=C:\msys64\ucrt64\bin;%PATH%
cd /d C:\Users\Administrator\Desktop\Americano_v2\Americano\src
echo === Plan B Clean Build ===
taskkill /f /im americano.exe 2>nul
del *.o 2>nul
echo Compiling...
gcc -Wall -Wextra -std=c99 -O2 -c *.c
if %ERRORLEVEL% neq 0 goto :err
echo Linking...
gcc -std=c99 -O2 *.o -o ..\bin\americano.exe
if %ERRORLEVEL% neq 0 goto :err
echo === Testing ===
for %%f in (..\examples\*.pla) do (
  ..\bin\americano.exe "%%f" >nul 2>&1
  if %ERRORLEVEL% equ 0 (echo PASS: %%~nxf) else (echo FAIL: %%~nxf)
)
goto :end
:err
echo BUILD FAILED
:end
pause
