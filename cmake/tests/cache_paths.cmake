# Run with: cmake -P cmake/tests/cache_paths.cmake
cmake_minimum_required(VERSION 3.16)

if(CAPI_CACHE_PATH_CHILD)
    # Match the toolchain's cache initialization in an external project.
    set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Library directory" FORCE)
    set(CAPI_TEST_FILE "config/test file.cmake" CACHE FILEPATH "Config file")
    if(NOT CMAKE_INSTALL_LIBDIR STREQUAL "lib")
        message(FATAL_ERROR "Library installation escaped the install prefix: ${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(NOT CAPI_TEST_FILE STREQUAL "config/test file.cmake")
        message(FATAL_ERROR "Relative file path changed: ${CAPI_TEST_FILE}")
    endif()
    return()
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../Utils.cmake")
set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Library directory" FORCE)
set(CAPI_TEST_FILE "config/test file.cmake" CACHE FILEPATH "Config file" FORCE)
utils_get_cmake_args_from_cache_vars(LIBRARY_ARGS FILTER_PREFIX CMAKE_INSTALL_LIBDIR)
utils_get_cmake_args_from_cache_vars(FILE_ARGS FILTER_PREFIX CAPI_TEST_FILE)

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${LIBRARY_ARGS} ${FILE_ARGS}
        -DCAPI_CACHE_PATH_CHILD=ON -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Forwarded cache paths changed in the child process:\n${output}${error}")
endif()
message(STATUS "Relative cache paths survive forwarding to a child CMake process")
