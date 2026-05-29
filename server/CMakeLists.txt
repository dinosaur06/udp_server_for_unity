cmake_minimum_required(VERSION 3.16)
project(FortressServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)
add_compile_options(-O2 -Wall)

file(GLOB SRC "src/*.cpp")
add_executable(fortress_server ${SRC})
target_include_directories(fortress_server PRIVATE src)
target_link_libraries(fortress_server pthread)
