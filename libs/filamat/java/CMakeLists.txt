cmake_minimum_required(VERSION 3.4.1)
project(filamat-java)

# todo: can we share this Java-checking logic somewhere?
if (NOT ENABLE_JAVA)
    return()
endif()

find_package(Java)
if (NOT Java_FOUND)
    message(WARNING "JDK not found, skipping Java projects")
    return()
endif()

# TODO: why does this need to be disabled for Android?
# find_package(JNI)
# if (NOT JNI_FOUND)
#     message(WARNING "JNI not found, skipping Java projects")
#     return()
# endif()

# todo: why does JAVA_HOME have to be set?
if (NOT DEFINED ENV{JAVA_HOME})
    message(WARNING "The JAVA_HOME environment variable must be set to compile Java projects")
    message(WARNING "Skipping Java projects")
    return()
endif()

set(FILAMENT_DIR ${FILAMENT_DIST_DIR})
if (FILAMENT_DIR)
    add_library(filamat STATIC IMPORTED)
    set_target_properties(filamat PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilamat.a)

    add_library(utils STATIC IMPORTED)
    set_target_properties(utils PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libutils.a)

    add_library(filabridge STATIC IMPORTED)
    set_target_properties(filabridge PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilabridge.a)

    add_library(smol-v STATIC IMPORTED)
    set_target_properties(smol-v PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libsmol-v.a)

    add_library(glslang STATIC IMPORTED)
    set_target_properties(glslang PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libglslang.a)

    add_library(SPIRV-Tools-opt STATIC IMPORTED)
    set_target_properties(SPIRV-Tools-opt PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libSPIRV-Tools-opt.a)

    add_library(spirv-cross-glsl STATIC IMPORTED)
    set_target_properties(spirv-cross-glsl PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libspirv-cross-glsl.a)

    add_library(spirv-cross-core STATIC IMPORTED)
    set_target_properties(spirv-cross-core PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libspirv-cross-core.a)

    add_library(SPIRV STATIC IMPORTED)
    set_target_properties(SPIRV PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libSPIRV.a)

    add_library(SPVRemapper STATIC IMPORTED)
    set_target_properties(SPVRemapper PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libSPVRemapper.a)

    add_library(OSDependent STATIC IMPORTED)
    set_target_properties(OSDependent PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libOSDependent.a)

    add_library(OGLCompiler STATIC IMPORTED)
    set_target_properties(OGLCompiler PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libOGLCompiler.a)

    add_library(shaders STATIC IMPORTED)
    set_target_properties(shaders PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libshaders.a)

    include_directories(${FILAMENT_DIR}/include)
endif()

# ==================================================================================================
# JNI bindings
# ==================================================================================================
set(TARGET filamat-jni)

set(JNI_SOURCE_FILES
        src/main/cpp/MaterialBuilder.cpp)

add_library(${TARGET} SHARED ${JNI_SOURCE_FILES})

target_include_directories(${TARGET} PRIVATE ${JNI_INCLUDE_DIRS})

# set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fvisibility=hidden")

set(EXPORTED_SYMBOLS)
if (APPLE)
    set(EXPORTED_SYMBOLS "-exported_symbols_list ${CMAKE_CURRENT_SOURCE_DIR}/libfilamat-jni.symbols")
elseif (ANDROID)
    set(EXPORTED_SYMBOLS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libfilamat-jni.map")
endif()

set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 14
        COMPILE_FLAGS "-fno-exceptions -fvisibility=hidden"
        LINK_FLAGS "${GC_SECTIONS} ${EXPORTED_SYMBOLS}")

target_link_libraries(${TARGET} filamat SPIRV SPVRemapper SPIRV-Tools-opt spirv-cross-core spirv-cross-glsl glslang utils filabridge smol-v OSDependent OGLCompiler log shaders)

# ==================================================================================================
# Java APIs
# ==================================================================================================

set(TARGET filamat-java)

include(UseJava)

set(CMAKE_JAVA_COMPILE_FLAGS "-source" "1.8" "-target" "1.8")

set(JAVA_SOURCE_FILES
        src/main/java/com/google/android/filament/filamat/MaterialBuilder.java
        src/main/java/com/google/android/filament/filamat/MaterialPackage.java)

add_jar(${TARGET}
        SOURCES ${JAVA_SOURCE_FILES}
        INCLUDE_JARS ../../../java/lib/support-annotations.jar)

