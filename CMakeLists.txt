cmake_minimum_required(VERSION 3.15)
project(CodeDuplicationDetector)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)

add_executable(detector
    src/main.cpp
    src/scanner.cpp
    src/reader.cpp
    src/extractor.cpp
    src/detector.cpp
    src/reporter.cpp
)