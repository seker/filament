cmake_minimum_required(VERSION 3.19)
project(filamat-android)

option(FILAMENT_ENABLE_MATDBG "Enables Material debugger" OFF)

set(FILAMENT_DIR ${FILAMENT_DIST_DIR})

set(FILAMAT_FLAVOR "filamat")

if (FILAMENT_SUPPORTS_VULKAN)
    message("Library filamat ignores Vulkan settings")
endif()

if (FILAMENT_SUPPORTS_WEBGPU)
    # See third_party/dawn/tnt/CMakeLists.txt and libs/filamat/CMakeLists.txt
    add_library(tint STATIC IMPORTED)
    set_target_properties(tint PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libtint_combined.a)
endif()

add_library(${FILAMAT_FLAVOR} STATIC IMPORTED)
set_target_properties(${FILAMAT_FLAVOR} PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/lib${FILAMAT_FLAVOR}.a)

add_library(utils STATIC IMPORTED)
set_target_properties(utils PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libutils.a)

add_library(perfetto STATIC IMPORTED)
set_target_properties(perfetto PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libperfetto.a)

add_library(filabridge STATIC IMPORTED)
set_target_properties(filabridge PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilabridge.a)

add_library(smol-v STATIC IMPORTED)
set_target_properties(smol-v PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libsmol-v.a)

add_library(shaders STATIC IMPORTED)
set_target_properties(shaders PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libshaders.a)

set(FILAMAT_INCLUDE_DIRS
        ../../libs/utils/include
        ../../third_party/perfetto
        )

include_directories(${FILAMENT_DIR}/include)

set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--version-script=${CMAKE_SOURCE_DIR}/libfilamat-jni.map")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

add_library(filamat-jni SHARED src/main/cpp/MaterialBuilder.cpp)
target_include_directories(filamat-jni PRIVATE ${FILAMAT_INCLUDE_DIRS})

target_link_libraries(filamat-jni
        ${FILAMAT_FLAVOR}
        filabridge
        shaders
        utils
        perfetto
        log
        smol-v
        $<$<STREQUAL:${FILAMENT_SUPPORTS_WEBGPU},ON>:tint>
)
