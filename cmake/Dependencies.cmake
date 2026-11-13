# Pinned third-party sources, fetched by tools/setup.py. CMake itself never downloads.
set(MWCC_SOFTFLOAT_SOURCE "${PROJECT_SOURCE_DIR}/build/_deps/softfloat-src" CACHE PATH "Berkeley SoftFloat source")
set(MWCC_MIMALLOC_SOURCE "${PROJECT_SOURCE_DIR}/build/_deps/mimalloc-src" CACHE PATH "mimalloc source")
if(NOT EXISTS "${MWCC_SOFTFLOAT_SOURCE}/source/include/softfloat.h" OR NOT EXISTS "${MWCC_MIMALLOC_SOURCE}/include/mimalloc.h")
    message(FATAL_ERROR "Run python tools/setup.py first")
endif()

# SoftFloat: upstream's 64-bit source list with its x87 NaN conventions (the 8086
# specialization, which has no bfloat16 support).
file(READ "${MWCC_SOFTFLOAT_SOURCE}/build/Linux-x86_64-GCC/Makefile" makefile)
set(softfloat_sources)
foreach(group PRIMITIVES SPECIALIZE OTHERS)
    string(REGEX MATCH "OBJS_${group} =[^=]+" objects "${makefile}")
    string(REGEX MATCHALL "[A-Za-z0-9_]+\\$\\(OBJ\\)" objects "${objects}")
    foreach(object IN LISTS objects)
        string(REPLACE "$(OBJ)" ".c" source "${object}")
        if(source MATCHES "[Bb][Ff]16")
            continue()
        elseif(group STREQUAL "SPECIALIZE")
            list(APPEND softfloat_sources "${MWCC_SOFTFLOAT_SOURCE}/source/8086/${source}")
        else()
            list(APPEND softfloat_sources "${MWCC_SOFTFLOAT_SOURCE}/source/${source}")
        endif()
    endforeach()
endforeach()
add_library(softfloat STATIC ${softfloat_sources})
# runtime/float80.cpp wraps this routine to report x87 rounding details.
set_source_files_properties("${MWCC_SOFTFLOAT_SOURCE}/source/s_roundPackToExtF80.c" PROPERTIES
    COMPILE_DEFINITIONS "softfloat_roundPackToExtF80=softfloat_originalRoundPackToExtF80")
target_include_directories(softfloat PRIVATE "${PROJECT_SOURCE_DIR}/runtime/softfloat" "${MWCC_SOFTFLOAT_SOURCE}/source/8086")
target_include_directories(softfloat PUBLIC "${MWCC_SOFTFLOAT_SOURCE}/source/include")
target_compile_definitions(softfloat PUBLIC LITTLEENDIAN=1 SOFTFLOAT_FAST_INT64
    PRIVATE INLINE_LEVEL=5 SOFTFLOAT_FAST_DIV32TO16 SOFTFLOAT_FAST_DIV64TO32)
set_target_properties(softfloat PROPERTIES C_STANDARD 11)

set(MI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(MI_BUILD_OBJECT OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
set(MI_PADDING OFF CACHE BOOL "" FORCE)
add_subdirectory("${MWCC_MIMALLOC_SOURCE}" "${PROJECT_BINARY_DIR}/mimalloc" EXCLUDE_FROM_ALL)
target_compile_definitions(mimalloc-static PRIVATE MI_DEBUG=0)

configure_file("${MWCC_SOFTFLOAT_SOURCE}/COPYING.txt" "${PROJECT_BINARY_DIR}/SoftFloat-LICENSE.txt" COPYONLY)
configure_file("${MWCC_MIMALLOC_SOURCE}/LICENSE" "${PROJECT_BINARY_DIR}/mimalloc-LICENSE.txt" COPYONLY)
