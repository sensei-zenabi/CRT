cmake_minimum_required(VERSION 3.16)
project(CRT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(crt src/main.cpp)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(X11)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if (MSVC)
  target_compile_options(crt PRIVATE /W4 /WX)
else()
  target_compile_options(crt PRIVATE -Wall -Wextra -Werror -pedantic)
endif()

target_link_libraries(crt PRIVATE SDL2::SDL2 OpenGL::GL)

if (X11_FOUND)
  target_compile_definitions(crt PRIVATE CRT_ENABLE_X11)
  if (TARGET X11::X11)
    target_link_libraries(crt PRIVATE X11::X11)
  endif()
  if (TARGET X11::Xext)
    target_link_libraries(crt PRIVATE X11::Xext)
  endif()
endif()

target_include_directories(crt PRIVATE ${SDL2_INCLUDE_DIRS})

