
set DISC_NAME=D
set CURRENT_DIR=%cd%
set path="%DISC_NAME%:/cygwin64/bin"
cd "%DISC_NAME%:/cygwin64/bin"		

x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse.bin"  -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/

x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_msse4_2.bin" -maes -msse4.2 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_core2.bin" -maes -march=core2 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_corei7.bin" -maes -march=corei7 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_corei7_avx.bin" -maes -march=corei7-avx -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_core_avx2.bin" -maes -march=core-avx2 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_bdver1.bin" -maes -march=bdver1 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_bdver2.bin" -maes -march=bdver2 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_bdver3.bin" -maes -march=bdver3 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/sse/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-sse__maes_march_btver1.bin" -maes -march=btver1 -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/
x86_64-w64-mingw32-gcc "%CURRENT_DIR%/src/apps/workers/legacy/main.cpp" -o "%CURRENT_DIR%/dst/App/x64/worker-x64-legacy.bin"  -O3 -Wl,--strip-all -shared -I%CURRENT_DIR%/src/


pause