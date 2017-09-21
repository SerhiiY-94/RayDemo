cmake_minimum_required(VERSION 3.2)
project(test_game_shared)

IF(WIN32)

ELSE(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    set(LIBS pthread)
ENDIF(WIN32)

set(SOURCE_FILES main.cpp
                 test_common.h
                 test_components.cpp
                 test_make_cnt.cpp
                 test_transactions.cpp
                 test_veh.cpp)

add_executable(test_game_shared ${SOURCE_FILES})
target_link_libraries(test_game_shared ${LIBS} game_shared)