cmake_minimum_required(VERSION 3.20)
project(gasm)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

add_executable(gasm 
    src/main.cpp 
    src/lexer.cpp 
    src/parser.cpp 
    src/transpiler.cpp 
    src/constant.cpp)
target_include_directories(gasm PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(gasm PRIVATE
    -Wall
    -Wextra
    -Wshadow
    -Wunreachable-code
)