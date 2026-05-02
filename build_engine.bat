rmdir /s /q build
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build