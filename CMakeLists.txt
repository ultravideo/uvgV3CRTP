cmake_minimum_required(VERSION 3.0...3.31)
cmake_policy(SET CMP0048 NEW)

#
# Project Configuration
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/ProjectDetails.cmake)
project(uvgv3crtp
        VERSION ${uvgv3crtp_VER}
        DESCRIPTION ${uvgv3crtp_DESCR}
        LANGUAGES CXX)

include(GNUInstallDirs)

option(UVGV3CRTP_DISABLE_EXAMPLES "Do not build examples" OFF)
option(UVGV3CRTP_DISABLE_INSTALL "Do not install the library" OFF)
option(UVGV3CRTP_DISABLE_WERROR "Ignore compiler warnings" ON)

# Take care of external dependencies (e.g. uvgRTP)
include(dependencies/FindDependencies.cmake)

include(cmake/Versioning.cmake)

# Check necessary dependencies
# PThread
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
#find_package( Threads REQUIRED )

add_library(${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES
                                      SOVERSION "${PROJECT_VERSION_MAJOR}"
                                      VERSION "${LIBRARY_VERSION}")

set(UVGV3CRTP_CXX_FLAGS "")
set(UVGV3CRTP_LINKER_FLAGS "")

target_sources(${PROJECT_NAME} PRIVATE
    src/V3C.cpp           src/V3C.h
    src/V3C_Gof.cpp       src/V3C_Gof.h
    src/V3C_Unit.cpp      src/V3C_Unit.h
    src/Nalu.cpp          src/Nalu.h
    src/Sample_Stream.cpp src/Sample_Stream.h
    src/V3C_Receiver.cpp  src/V3C_Receiver.h
    src/V3C_Sender.cpp    src/V3C_Sender.h
    src/Timestamp.cpp     src/Timestamp.h
    src/v3c_api.cpp       

    include/uvgv3crtp/v3c_api.h
    include/uvgv3crtp/version.h
    include/uvgv3crtp/global.h
)

source_group(src src/.*)
source_group(include include/.*)

# Add other libraries that should be linked
#target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ${PROJECT_NAME}_version)
target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_version uvgrtp)
# Make sure uvgRTP is linked as well when linking to the library

target_include_directories(${PROJECT_NAME}
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        PUBLIC $<INSTALL_INTERFACE:include>
        )
        
        
if(CMAKE_BUILD_TYPE)
    message(STATUS "Selecting build type: ${CMAKE_BUILD_TYPE}")
elseif (NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected. Selecting Release. You may use CMAKE_BUILD_TYPE to select a different build type.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /Zc:__cplusplus /W4)
else()
    if (UVGV3CRTP_DISABLE_WERROR)
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
    else ()
        target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

# Try to take care of lib installation related things
if (UNIX)
    # Try finding if pkg-config installed in the system
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        list(APPEND UVGV3CRTP_LINKER_FLAGS "-luvgv3crtp")
        if(CMAKE_USE_PTHREADS_INIT AND NOT CMAKE_HAVE_LIBC_PTHREAD)
            list(APPEND UVGV3CRTP_LINKER_FLAGS "-lpthread")
        endif()
        # Check PKG_CONFIG_PATH, if not defined, use lib/pkgconfig
        if(NOT DEFINED ENV{PKG_CONFIG_PATH})
            set(PKG_CONFIG_PATH "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
            message("PKG_CONFIG_PATH is not set. Setting it to ${PKG_CONFIG_PATH}")
        endif(NOT DEFINED ENV{PKG_CONFIG_PATH})

        # Generate and install .pc file
        string(REPLACE ";" " " UVGV3CRTP_CXX_FLAGS "${UVGV3CRTP_CXX_FLAGS}")
        string(REPLACE ";" " " UVGV3CRTP_LINKER_FLAGS "${UVGV3CRTP_LINKER_FLAGS}")
        configure_file("cmake/uvgv3crtp.pc.in" "uvgv3crtp.pc" @ONLY)
        if (NOT UVGV3CRTP_DISABLE_INSTALL)
            install(FILES ${CMAKE_CURRENT_BINARY_DIR}/uvgv3crtp.pc DESTINATION ${PKG_CONFIG_PATH}/)
        endif()
    else()
        message("pkg-config not found. Not generating pc file")
    endif(PkgConfig_FOUND)
endif (UNIX)

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework Security")
endif()

if (NOT UVGV3CRTP_DISABLE_EXAMPLES)
    add_subdirectory(examples EXCLUDE_FROM_ALL)
endif()

if (NOT UVGV3CRTP_DISABLE_INSTALL)
    # Install
    #
    # Define install target, install libraries and archives (static libraries) to "<prefix>/..."
    install(TARGETS ${PROJECT_NAME} ${PROJECT_NAME}_version EXPORT ${PROJECT_NAME}Targets
            LIBRARY
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT ${PROJECT_NAME}_Runtime
            ARCHIVE
                DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT ${PROJECT_NAME}_Runtime
            RUNTIME
                DESTINATION ${CMAKE_INSTALL_BINDIR}
                COMPONENT ${PROJECT_NAME}_Runtime)

    #Copy all header files to the <prefix>/include/uvgv3crtp directory
    file(GLOB DEPLOY_FILES_AND_DIRS "${CMAKE_SOURCE_DIR}/include/uvgv3crtp/*")
    install(FILES ${DEPLOY_FILES_AND_DIRS}
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}/
            COMPONENT ${PROJECT_NAME}_Develop)

    #Create a File representing the current library version
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
            COMPATIBILITY SameMajorVersion
    )

    #Create a Targets file representing exported targets (for usage within the build tree)
    export(EXPORT ${PROJECT_NAME}Targets
            FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake"
            NAMESPACE ${PROJECT_NAME}::
            )

    #Copy "cmake/uvgv3crtpConfig.cmake" to "${CMAKE_CURRENT_BINARY_DIR}/uvgv3crtp/uvgv3crtpConfig.cmake"
    configure_file(cmake/${PROJECT_NAME}Config.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake"
            COPYONLY
            )

    #Copy "cmake/uvgv3crtpMacros.cmake" to "${CMAKE_CURRENT_BINARY_DIR}/uvgv3crtp/uvgv3crtpMacros.cmake"
    configure_file(cmake/${PROJECT_NAME}Macros.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}Macros.cmake"
            COPYONLY
            )

    #
    # Adding target to installing cmake package
    #
    set(ConfigPackageLocation lib/cmake/${PROJECT_NAME})
    install(EXPORT ${PROJECT_NAME}Targets
            FILE ${PROJECT_NAME}Targets.cmake
            NAMESPACE ${PROJECT_NAME}::
            DESTINATION ${ConfigPackageLocation}
            )

    install(FILES cmake/${PROJECT_NAME}Config.cmake cmake/${PROJECT_NAME}Macros.cmake
            "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake"
            DESTINATION ${ConfigPackageLocation}
            COMPONENT v3cRTPMain
    )

    # Packaging
    #
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        #add_subdirectory(packaging)
    endif()
endif()