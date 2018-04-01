cmake_minimum_required(VERSION 3.9)
project(hw3)

set(CMAKE_CXX_STANDARD 11)

add_executable(hw3
        blkdev.h
        homework.c
        image.c
        mirror-test.c)
