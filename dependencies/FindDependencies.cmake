# Check if uvgRTP is already available
find_package(uvgRTP 3.1.6 QUIET) #TODO: update min version to a version with necessary bug fixes etc.

# Try pkgConfig as well
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    if(NOT UVGRTP_FOUND)
	    pkg_search_module(UVGRTP uvgrtp>=3.1.6 uvgRTP>=3.1.6)
	endif()
endif()

# If we did not find uvgRTP build it from source_group
if(UVGRTP_FOUND)
    message(STATUS "Found uvgRTP")
else()
    message(STATUS "uvgRTP not found, building from source...")
	
	#include(FetchContent)
	find_package(Git REQUIRED)
	
	option(GIT_SUBMODULE "Check submodules during build" ON)
	if(GIT_SUBMODULE)
	    message(STATUS "Update submodule")
		execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
		                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
						RESULT_VARIABLE GIT_SUBMOD_RESULT)
	    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
		    message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodule")
		endif()
	endif()
	
	# Add subdir and specify relevant options
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    set(UVGRTP_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_EXAMPLES  ON CACHE BOOL "" FORCE)
    set(UVGRTP_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    
	# TODO: Enable crypto? For now it breaks things so don't include
	set(UVGRTP_DOWNLOAD_CRYPTO OFF CACHE BOOL "" FORCE)

    add_subdirectory(${PROJECT_SOURCE_DIR}/dependencies/uvgRTP)
	
	include_directories(${uvgrtp_SOURCE_DIR}/include)
	link_directories(${uvgrtp_SOURCE_DIR})
	
    unset(BUILD_SHARED_LIBS)

endif()



# include and link directories for dependencies. 
# These are needed when the compilation happens a second time and the library 
# is found so no compilation happens. Not needed in every case, but a nice backup
include_directories(${CMAKE_BINARY_DIR}/include)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/lib)
