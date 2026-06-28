cmake_minimum_required(VERSION 3.15)
project(CodeDuplicationDetector)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(include)
include_directories("C:/Program Files/LLVM/include")
include_directories("C:/onnxruntime-win-x64-1.26.0/include")

add_executable(detector
    src/main.cpp
    src/scanner.cpp
    src/reader.cpp
    src/extractor.cpp
    src/detector.cpp
    src/reporter.cpp
    src/normalizer.cpp
    src/clang_extractor.cpp
    src/winnowing.cpp
    src/gitscanner.cpp
    src/config.cpp
    src/embedder.cpp
    src/json_parser.cpp
    src/tokenizer.cpp
    src/similarity.cpp
    src/suggestion.cpp
)

target_link_libraries(detector
    "${CMAKE_SOURCE_DIR}/libs/libclang.dll.a"
    "${CMAKE_SOURCE_DIR}/libs/onnxruntime.dll.a"
)

# Copy libclang.dll next to the .exe so it can be found at runtime
add_custom_command(TARGET detector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/Program Files/LLVM/bin/libclang.dll"
    $<TARGET_FILE_DIR:detector>
)

# Copy onnxruntime.dll next to the .exe so it can be found at runtime
add_custom_command(TARGET detector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "C:/onnxruntime-win-x64-1.26.0/lib/onnxruntime.dll"
    $<TARGET_FILE_DIR:detector>
)