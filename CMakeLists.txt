cmake_minimum_required(VERSION 3.10)
project(JigglyMouser)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the executable
add_executable(JigglyMouser JigglyMouser.cpp)

# Add compile and link flags
target_link_libraries(JigglyMouser
    -static
    -static-libgcc
    -static-libstdc++
    -pthread
)
