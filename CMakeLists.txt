cmake_minimum_required(VERSION 3.31)
project(Sockets C)

set(CMAKE_C_STANDARD 11)

add_executable(Sockets main.c
        Sockets.c Sockets.h
        ff_File.c ff_File.h)
