@echo off
cd /d "%~dp0"
setlocal

if not exist build mkdir build

set CFLAGS=-std=c99 -O0 -g -Wall -Wextra

clang %CFLAGS% -o build\compiler.exe code\*.c -luser32.lib || exit /b 1
