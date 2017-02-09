cmake_minimum_required(VERSION 3.5)
project(sigmod_2017)

set(CMAKE_VERBOSE_MAKEFILE on)

OPTION(REAL_RUN "Real run" OFF)

IF(REAL_RUN)
    ADD_DEFINITIONS(-DREAL_RUN)
ENDIF(REAL_RUN)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fopenmp -march=native -std=c++14 -Wall")

set(SOURCE_FILES src/main.cpp src/word.cpp)
add_executable(sigmod_2017 ${SOURCE_FILES})
