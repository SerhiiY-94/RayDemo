cmake_minimum_required(VERSION 3.1)
project(DemoApp)

IF(WIN32)
    IF(SDL_STATIC)
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            set(LIBS :libSDL2main.a :libSDL2.a :libSDL2_mixer.a winmm imm32 version)
		elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
            set(LIBS SDL2main.lib SDL2.lib winmm imm32 version user32)
        endif()
    ELSE(SDL_STATIC)
		if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
			set(LIBS opengl32 SDL2 SDL2_mixer)
		elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
			set(LIBS opengl32 SDL2 SDL2_mixer)
		endif()
    ENDIF(SDL_STATIC)
ELSEIF(UNIX AND NOT APPLE)
    #set(LIBS X11 :libSDL2.a dl pthread GL glut c)
    set(LIBS X11 SDL2 dl pthread GL c)
ELSE()
    set(LIBS SDL2)
ENDIF()

set(SOURCE_FILES    main.cpp
                    DemoApp.h
                    DemoApp.cpp)

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
#set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

add_executable(DemoApp ${ALL_SOURCE_FILES})
target_link_libraries(DemoApp Eng ${LIBS})

add_dependencies(DemoApp DemoLib Sys)

add_custom_command(TARGET DemoApp 
                   POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:DemoApp> ${WORKING_DIRECTORY})
				   
set_target_properties(DemoApp PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${WORKING_DIRECTORY}")
