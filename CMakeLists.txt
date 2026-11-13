cmake_minimum_required(VERSION 3.20)
project(mwccps2_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "Generated code needs Clang (guaranteed tail calls)")
endif()
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
include(cmake/Dependencies.cmake)

set(MWCC_ORIGINAL "${PROJECT_SOURCE_DIR}/orig/mwccps2.exe" CACHE FILEPATH "Original compiler")
set(MWCC_LICENSE "${PROJECT_SOURCE_DIR}/orig/LMGR326B.dll" CACHE FILEPATH "Original license library")

file(GLOB runtime_sources CONFIGURE_DEPENDS runtime/*.cpp)
add_library(mwcc_runtime STATIC ${runtime_sources})
target_include_directories(mwcc_runtime PUBLIC runtime)
target_compile_definitions(mwcc_runtime PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)
target_link_libraries(mwcc_runtime PRIVATE softfloat mimalloc-static)
if(UNIX)
    target_link_libraries(mwcc_runtime PUBLIC pthread)
endif()

# recompile(<target> <key in config/images.json> <image file> <source count>)
function(recompile target name image parts)
    set(directory "${PROJECT_BINARY_DIR}/generated/${name}")
    set(sources "${directory}/image.cpp")
    math(EXPR last "${parts} - 1")
    foreach(index RANGE ${last})
        list(APPEND sources "${directory}/part_${index}.cpp")
    endforeach()
    file(GLOB generator CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/recomp/*.py")
    add_custom_command(OUTPUT ${sources}
        BYPRODUCTS "${directory}/functions.h"
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/recomp/generate.py" ${name}
            --image "${image}" --out "${directory}" --parts ${parts}
        DEPENDS "${image}" ${generator} "${PROJECT_SOURCE_DIR}/config/images.json"
        COMMENT "Recompiling ${name}" VERBATIM)
    add_library(${target} STATIC ${sources})
    target_include_directories(${target} PRIVATE "${directory}")
    target_link_libraries(${target} PUBLIC mwcc_runtime)
endfunction()

recompile(mwcc_compiler compiler "${MWCC_ORIGINAL}" 64)
recompile(mwcc_license license "${MWCC_LICENSE}" 2)

add_executable(mwccps2 native/main.cpp native/patches.cpp)
target_link_libraries(mwccps2 PRIVATE mwcc_compiler mwcc_license)
target_compile_definitions(mwccps2 PRIVATE _CRT_SECURE_NO_WARNINGS)
# The Linux executable keeps the suffix: the compiler's driver looks for it in its own name.
set_target_properties(mwccps2 PROPERTIES SUFFIX ".exe")
