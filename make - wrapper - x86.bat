set DISC_NAME=D
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin/bin"
cd "%DISC_NAME%:/cygwin/bin"

g++ "%CURRENT_DIR%/src/apps/wrapper/main.cpp" -o "%CURRENT_DIR%/dst/App/x86/wrapper.bin" -O3 -Wl,--strip-all   -maes -I%CURRENT_DIR%/src/

pause