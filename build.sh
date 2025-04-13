#!/bin/bash
x86_64-w64-mingw32-windres ditto.rc -o ditto.res -O coff
x86_64-w64-mingw32-g++ ditto.cpp ditto.res -o ditto.exe -static -lkernel32 -Wall -Wextra -std=c++17 -municode -O2
strip ditto.exe
