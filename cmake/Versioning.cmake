if(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE result
            OUTPUT_VARIABLE v3crtplib_GIT_HASH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(result)
        message(STATUS "Failed to get git hash")
    else()
        message(STATUS "Got git hash: ${v3crtplib_GIT_HASH}")
    endif()
endif()

if(uvgrtp_GIT_HASH)
    SET(v3crtplib_GIT_HASH "${v3crtplib_GIT_HASH}")
endif()

if(UVGRTP_RELEASE_COMMIT)
    set (LIBRARY_VERSION ${PROJECT_VERSION})
elseif(uvgrtp_GIT_HASH)
    set (LIBRARY_VERSION ${PROJECT_VERSION} + "-" + ${v3crtplib_GIT_HASH})
else()
    set (LIBRARY_VERSION ${PROJECT_VERSION} + "-source")
    set(uvgrtp_GIT_HASH "source")
endif()

configure_file(cmake/version.cpp.in version.cpp
        @ONLY
        )
add_library(${PROJECT_NAME}_version OBJECT
        ${CMAKE_CURRENT_BINARY_DIR}/version.cpp)
target_include_directories(${PROJECT_NAME}_version
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

if (V3CRTPLIB_RELEASE_COMMIT)
    target_compile_definitions(${PROJECT_NAME}_version PRIVATE V3CRTPLIB_RELEASE_COMMIT)
endif()