#!/bin/bash
mkdir -p build
cd build
cmake .. -G "Unix Makefiles"
cmake --build .
cd ..
