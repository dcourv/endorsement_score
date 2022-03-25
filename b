#!/bin/sh
cmake -B build -S . -G Ninja && cmake --build build
