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

find_package(JNI)
if (NOT JNI_FOUND)
    message(WARNING "JNI not found, skipping Java projects")
    return()
endif()

# todo: why does JAVA_HOME have to be set?
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

set_property(TARGET ${TARGET} PROPERTY CXX_STANDARD 14)

target_link_libraries(${TARGET} filamat)

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

