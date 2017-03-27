cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)

project(unitydata)

set(LIB_SRC
	src/Serialize.cpp
	src/SerializedFile.cpp)

set(CLI_SRC
	src/main.cpp)

set(FEATURES
	cxx_strong_enums)

add_library(unitypack SHARED ${LIB_SRC})
target_compile_features(unitypack PRIVATE ${FEATURES})

add_executable(unityextract ${LIB_SRC} ${CLI_SRC})
target_compile_features(unityextract PRIVATE ${FEATURES})
