set DISC_NAME=D
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin64/bin"
cd "%DISC_NAME%:/cygwin64/bin"

x86_64-w64-mingw32-g++ "%CURRENT_DIR%/src/apps/wrapper/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/wrapper.bin" -O3 -Wl,--strip-all   -maes -I%CURRENT_DIR%/src/

pause