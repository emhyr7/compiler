@echo off
setlocal

if not exist build mkdir build

set CFLAGS=-O0 -std=c23 -g -Wno-gnu-variable-sized-type-not-at-end
set LFLAGS=

clang %CFLAGS% -o build\compiler.obj -c compiler.c %LFLAGS%
clang %CFLAGS% -o build\compiler.exe win32_compiler.c build\compiler.obj %LFLAGS%
