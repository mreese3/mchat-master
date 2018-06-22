project(mchat_curses_ui C)
cmake_minimum_required(VERSION 2.8)

# Turns out cmake version before 3.1 did NOT include CMAKE_C_STANDARD as a property.
# This macro is to make up for this shortfall.
# Borrowed From: https://stackoverflow.com/questions/24840030/forcing-c99-in-cmake-to-use-for-loop-initial-declaration
macro(use_c99)
  if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set (CMAKE_C_FLAGS "--std=gnu99 ${CMAKE_C_FLAGS}")
    endif ()
  else ()
    set (CMAKE_C_STANDARD 99)
  endif ()
endmacro(use_c99)

find_package(Curses REQUIRED)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})
use_c99()
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME mchat)
add_subdirectory(libmchat)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libmchat/include ${CURSES_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${CURSES_LIBRARIES} libmchat_shared)

#Qt Creator specific directives
file(GLOB CURSES_UI_HEADER_FILES "${PROJECT_SOURCE_DIR}/src/*.h")
add_custom_target(curses_ui_headers SOURCES ${CURSES_UI_HEADER_FILES})
