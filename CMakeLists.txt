cmake_minimum_required(VERSION 3.2)

function(add_ispcs ISPC_OBJECTS FILES)
    set(OBJECT_FILES "")
    foreach(SOURCE_FILE IN LISTS FILES)
        string(REPLACE ".ispc" ".ispc.o" OBJECT_FILE ${SOURCE_FILE})
        get_filename_component(OBJECT_FILE ${OBJECT_FILE} NAME)
        set(OBJECT_FILE ${CMAKE_CURRENT_BINARY_DIR}/${OBJECT_FILE})
        set(OBJECT_FILES "${OBJECT_FILES};${OBJECT_FILE}")
        string(REPLACE ".ispc" ".ispc.h" HEADER_FILE ${SOURCE_FILE})
        message("header:${HEADER_FILE}")
        add_custom_command(OUTPUT ${OBJECT_FILE}
            COMMAND ${ISPC_BIN} ${SOURCE_FILE} -o ${OBJECT_FILE} --target=${ISPC_TARGET} --arch=${ISPC_ARCH} --cpu=${ISPC_CPU} -h ${HEADER_FILE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endforeach()
    set(${ISPC_OBJECTS} ${OBJECT_FILES} PARENT_SCOPE)
endfunction(add_ispcs)

set(CMAKE_CONFIGURATION_TYPES "Debug" "Release")

set(ProjectName resamcpp)
project(${ProjectName})

if(USE_ISPC)
    set(ISPC_VERSION "v1.17.0")
    set(ISPC_ARCH x86-64)
    set(ISPC_TARGET avx2)
    set(ISPC_CPU core-avx2)

    if(CMAKE_HOST_WIN32)
        set(ISPC_ARCHIVE "${CMAKE_CURRENT_SOURCE_DIR}/ispc.zip")
        if(NOT (EXISTS "${ISPC_ARCHIVE}"))
            file(DOWNLOAD "https://github.com/ispc/ispc/releases/download/${ISPC_VERSION}/ispc-${ISPC_VERSION}-windows.zip" "${ISPC_ARCHIVE}" SHOW_PROGRESS)
            file(ARCHIVE_EXTRACT INPUT "${ISPC_ARCHIVE}" DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
        set(ISPC_BIN "${CMAKE_CURRENT_SOURCE_DIR}/ispc-${ISPC_VERSION}-windows/bin/ispc.exe")
    else()
        set(ISPC_BIN "ispc")
    endif()
endif()

set(HEADERS "resamcpp.h")
set(SOURCES "main.cpp;resamcpp.cpp")

if(USE_ISPC)
    set(ISPCS "${CMAKE_CURRENT_SOURCE_DIR}/resamcpp.ispc")
endif()

########################################################################
# Sources
source_group("include" FILES ${HEADERS})
source_group("src" FILES ${SOURCES})
source_group("src" FILES ${ISPCS})

set(FILES ${HEADERS} ${SOURCES})

set(OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${OUTPUT_DIRECTORY}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${OUTPUT_DIRECTORY}")

if(USE_ISPC)
    add_ispcs(ISPC_OBJECTS "${ISPCS}")
    add_executable(${ProjectName} ${FILES} ${ISPC_OBJECTS})
else()
    add_executable(${ProjectName} ${FILES})
endif()

if(MSVC)
    set(DEFAULT_CXX_FLAGS "/DWIN32 /D_WINDOWS /D_MSBC /DRESAMCPP_WAV /W4 /WX- /nologo /fp:precise /arch:AVX2 /Zc:wchar_t /TP /Gd")
    if("1800" VERSION_LESS MSVC_VERSION)
        set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} /EHsc")
    endif()
    if(USE_ISPC)
        set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} /DRESAMCPP_ISPC")
    endif()

    set(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")
    set(CMAKE_CXX_FLAGS_DEBUG "/D_DEBUG /MDd /Zi /Ob0 /Od /RTC1 /Gy /GR- /GS /Gm-")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /GL /GR- /DNDEBUG")

elseif(UNIX)
    if(USE_ISPC)
        set(DEFAULT_CXX_FLAGS "${DEFAULT_CXX_FLAGS} -DRESAMCPP_ISPC")
    endif()

    set(DEFAULT_CXX_FLAGS "-DRESAMCPP_WAV -Wall -O2 -std=c++17 -std=gnu++17 -march=native")
    set(CMAKE_CXX_FLAGS "${DEFAULT_CXX_FLAGS}")
elseif(APPLE)
endif()

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
set_target_properties(${ProjectName}
    PROPERTIES
        OUTPUT_NAME_DEBUG "${ProjectName}" OUTPUT_NAME_RELEASE "${ProjectName}"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

