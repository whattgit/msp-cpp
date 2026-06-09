cmake_minimum_required(VERSION 3.20)
project(mspc LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mspc
    src/app/x7k9_entry.cpp
    src/internal/x7k9_a3f1.cpp
    src/internal/x7k9_b2e4.cpp
    src/internal/x7k9_c5d6.cpp
    src/internal/x7k9_d7f8.cpp
    src/internal/x7k9_e1g2.cpp
    src/internal/x7k9_f3h4.cpp
    src/internal/x7k9_g5i6.cpp
    src/internal/x7k9_h7j8.cpp
    src/internal/x7k9_i9k0.cpp
    src/internal/x7k9_j1l2.cpp
    src/internal/x7k9_k3m4.cpp
    src/internal/x7k9_m9p1.cpp
    vendor/miniz/miniz.c
)

target_include_directories(mspc PRIVATE
    src
    vendor/miniz
)

if(MSVC)
    target_compile_definitions(mspc PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    target_compile_options(mspc PRIVATE /W4 /utf-8)
else()
    target_compile_options(mspc PRIVATE -Wall -Wextra)
endif()

target_link_libraries(mspc PRIVATE wininet crypt32)

if(WIN32)
    set_target_properties(mspc PROPERTIES OUTPUT_NAME "mspc")
endif()
