set DISC_NAME=C
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin/bin"
cd "%DISC_NAME%:/cygwin/bin"

g++ "%CURRENT_DIR%/src/apps/startup/main.cpp" -o "%CURRENT_DIR%/dst/App/x86/startup.bin" -O3 -Wl,--strip-all -maes -I%CURRENT_DIR%/src/ -lws2_32

pause