cmake_minimum_required(VERSION 3.10)
project(malloc_allocator C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -O2")

# Source files
set(SOURCES
    mm.c
    memlib.c
    main.c
)

# Create executable named 'code'
add_executable(code ${SOURCES})
