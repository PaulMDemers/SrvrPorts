# Exploratory cross configuration: feature checks must link against the SDK.
get_filename_component(SRVROS_REPOSITORY_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(SRVROS_SDK "${SRVROS_REPOSITORY_ROOT}/build/sysroot/srvros")
set(CMAKE_SYSTEM_NAME SrvrOS)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "${CMAKE_CURRENT_LIST_DIR}/cc")
set(CMAKE_CXX_COMPILER "${CMAKE_CURRENT_LIST_DIR}/cxx")
set(CMAKE_FIND_ROOT_PATH "${SRVROS_SDK}")
if (DEFINED SRVROS_ICU_ROOT)
    list(PREPEND CMAKE_FIND_ROOT_PATH "${SRVROS_ICU_ROOT}")
endif ()
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
# Do not use STATIC_LIBRARY try_compile: FindThreads must test actual linking.

# JSC-only opt-in: use the separately proven target filesystem implementation.
# Propagate the option into real executable try_compile checks as well.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES SRVROS_JSC_FILESYSTEM)
if (SRVROS_JSC_FILESYSTEM)
    set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -D_LIBCPP_HAS_FILESYSTEM=1 -D_LIBCPP_HAS_LOCALIZATION=0")
    set(_srvros_runtime "${SRVROS_REPOSITORY_ROOT}/build/browser-runtime")
    set(_srvros_filesystem "${SRVROS_REPOSITORY_ROOT}/build/browser-filesystem/libcxx-filesystem.a")
    set(CMAKE_CXX_STANDARD_LIBRARIES_INIT "-Wl,--start-group \"${_srvros_filesystem}\" \"${_srvros_runtime}/libcxx-threaded.a\" \"${_srvros_runtime}/libcxxabi-rtti.a\" \"${SRVROS_SDK}/lib/libsrvros.a\" -Wl,--end-group")
endif ()
