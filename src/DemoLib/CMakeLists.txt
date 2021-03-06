cmake_minimum_required(VERSION 2.8)
project(DemoLib)

add_definitions(-DBUILDING_DLL)

if(ENABLE_OPENCL)
else()
    add_definitions(-DDISABLE_OCL)
endif()

set(SOURCE_FILES    exports.cpp
					Viewer.h
                    Viewer.cpp)

set(LOAD_SOURCES  load/Load.h
				  load/Load.cpp)

set(STATES_SOURCES  states/GSCreate.h
					states/GSCreate.cpp
					states/GSCPUTest.h
					states/GSCPUTest.cpp
					states/GSHDRTest.h
					states/GSHDRTest.cpp
					states/GSHybTest.h
					states/GSHybTest.cpp
					states/GSRayTest.h
					states/GSRayTest.cpp
					states/GSRayBucketTest.h
					states/GSRayBucketTest.cpp
					states/GSLightmapTest.h
					states/GSLightmapTest.cpp
					states/GSSamplingTest.h
					states/GSSamplingTest.cpp)

set(UI_SOURCES  ui/FontStorage.h)

if(${RENDERER} STREQUAL "GL")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                         renderer/RendererGL.cpp)
elseif(${RENDERER} STREQUAL "SW")
    set(RENDERER_SOURCES ${RENDERER_SOURCES}
                         renderer/RendererSW.cpp)
endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES ${LOAD_SOURCES})
source_group("src\\load" FILES ${LOAD_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
set_source_files_properties(${STATES_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\states" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UI_SOURCES})
set_source_files_properties(${UI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\ui" FILES ${UI_SOURCES})

list(APPEND ALL_SOURCE_FILES states/_states.cpp)
source_group("src\\states" FILES states/_states.cpp)

add_library(DemoLib SHARED ${ALL_SOURCE_FILES})
target_link_libraries(DemoLib Eng Ray Ren Sys ${LIBS})

# dot not prefix dll with 'lib' in MinGW
set_target_properties(DemoLib PROPERTIES PREFIX "")

add_custom_command(TARGET DemoLib
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:DemoLib> ${WORKING_DIRECTORY})
