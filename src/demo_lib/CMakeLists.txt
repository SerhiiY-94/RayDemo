cmake_minimum_required(VERSION 2.8)
project(demo_lib)

add_definitions(-DBUILDING_DLL)

set(SOURCE_FILES    exports.cpp
					Viewer.h
                    Viewer.cpp)

set(COMP_SOURCES    comp/Drawable.h
                    comp/Drawable.cpp
                    comp/Transform.h
                    comp/Transform.cpp)

set(STATES_SOURCES  states/GSCreate.h
					states/GSCreate.cpp
					states/GSClipTest.h
					states/GSClipTest.cpp
					states/GSColorsTest.h
					states/GSColorsTest.cpp
					states/GSCurveTest.h
					states/GSCurveTest.cpp
					states/GSDrawTest.h
                    states/GSDrawTest.cpp
					states/GSPackTest.h
					states/GSPackTest.cpp
					states/GSRayTest.h
					states/GSRayTest.cpp
					states/GSSamplingTest.h
					states/GSSamplingTest.cpp)

set(RENDERER_SOURCES renderer/Renderer.cpp
                     renderer/Renderer.h)
					 
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

list(APPEND ALL_SOURCE_FILES ${COMP_SOURCES})
set_source_files_properties(${COMP_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\comp" FILES ${COMP_SOURCES})

list(APPEND ALL_SOURCE_FILES ${RENDERER_SOURCES})
set_source_files_properties(${RENDERER_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\renderer" FILES ${RENDERER_SOURCES})

list(APPEND ALL_SOURCE_FILES ${STATES_SOURCES})
set_source_files_properties(${STATES_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\states" FILES ${STATES_SOURCES})

list(APPEND ALL_SOURCE_FILES ${UI_SOURCES})
set_source_files_properties(${UI_SOURCES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src\\ui" FILES ${UI_SOURCES})

list(APPEND ALL_SOURCE_FILES comp/_comp.cpp)
source_group("src\\comp" FILES comp/_comp.cpp)

list(APPEND ALL_SOURCE_FILES renderer/_renderer.cpp)
source_group("src\\renderer" FILES renderer/_renderer.cpp)

list(APPEND ALL_SOURCE_FILES states/_states.cpp)
source_group("src\\states" FILES states/_states.cpp)

#list(APPEND ALL_SOURCE_FILES _game_client.cpp)
#source_group("src" FILES _game_client.cpp)

add_library(demo_lib SHARED ${ALL_SOURCE_FILES})
target_link_libraries(demo_lib engine ray ren sys ${LIBS})

add_custom_command(TARGET demo_lib 
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:demo_lib> ${WORKING_DIRECTORY})