set DISC_NAME=C
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin/bin"
cd "%DISC_NAME%:/cygwin/bin"

g++ "%CURRENT_DIR%/src/apps/BackgroundMemory/main.cpp" -o "%CURRENT_DIR%/dst/App/x86/background-memory.bin" -O3 -Wl,--strip-all -mwindows -I%CURRENT_DIR%/src/

pause