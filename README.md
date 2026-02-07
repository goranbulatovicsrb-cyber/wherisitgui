# WhereIsByLabelQt (Qt GUI)

Windows GUI app for indexing a disk by Volume Label and searching an index.

## Build (Qt Creator)
Open the folder as a CMake project and select a Qt 6 kit.

## Build (CMake)
Open "x64 Native Tools Command Prompt for VS" and run:

cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release

EXE:
build\Release\WhereIsByLabelQt.exe
