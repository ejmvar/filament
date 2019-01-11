cmake_minimum_required(VERSION 3.4.1)
project(filamat-java)

if (NOT ENABLE_JAVA)
    return()
endif()

find_package(Java)
if (NOT Java_FOUND)
    message(WARNING "JDK not found, skipping Java projects")
    return()
endif()

# Android already has the JNI headers in its system include path.
if (NOT ANDROID)
    find_package(JNI)
    if (NOT JNI_FOUND)
        message(WARNING "JNI not found, skipping Java projects")
        return()
    endif()
endif()

if (NOT DEFINED ENV{JAVA_HOME})
    message(WARNING "The JAVA_HOME environment variable must be set to compile Java projects")
    message(WARNING "Skipping Java projects")
    return()
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

# This is necessary to avoid a CMake error due to setting RPATH on non-elf platforms.
# Setting this also causes CMake to issue a warning:
# "Policy CMP0068 is not set: RPATH settings on macOS do not affect install_name." which can be
# safely ignored.
set(CMAKE_SKIP_RPATH TRUE)

set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 14
        COMPILE_FLAGS "-fno-exceptions -fvisibility=hidden"
        LINK_FLAGS "${GC_SECTIONS} ${EXPORTED_SYMBOLS}")

target_link_libraries(${TARGET} filamat)

# We need a way to detect if we're building via build.sh
SET(INSTALL_FILAMAT_ALONGSIDE_FILAMENT FALSE CACHE BOOL "Install filamat libraries alongside filament, as opposed to inside of")
message(STATUS ${INSTALL_FILAMAT_ALONGSIDE_FILAMENT})
if (INSTALL_FILAMAT_ALONGSIDE_FILAMENT)
    install(TARGETS ${TARGET} LIBRARY DESTINATION ../filamat/lib/${DIST_DIR})
else()
    install(TARGETS ${TARGET} LIBRARY DESTINATION filamat/lib/${DIST_DIR})
endif()

# ==================================================================================================
# Java APIs
# ==================================================================================================

# Android builds its Java bindings for filamat through Gradle.
if (ANDROID)
    return()
endif()

set(TARGET filamat-java)

include(UseJava)

set(CMAKE_JAVA_COMPILE_FLAGS "-source" "1.8" "-target" "1.8")

set(JAVA_SOURCE_FILES
        src/main/java/com/google/android/filament/filamat/MaterialBuilder.java
        src/main/java/com/google/android/filament/filamat/MaterialPackage.java)

add_jar(${TARGET}
        SOURCES ${JAVA_SOURCE_FILES}
        INCLUDE_JARS ../../../java/lib/support-annotations.jar)

install_jar(${TARGET} lib)
