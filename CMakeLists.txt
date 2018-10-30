cmake_minimum_required(VERSION 3.8)
project(StackVM)

include_directories(include)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -O0 -Wall -Wextra -g")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -Wall -Wextra -Wpedantic -Wnull-dereference -Wuninitialized -Winit-self -Wmissing-include-dirs -Wunused -Wunknown-pragmas")

add_executable(my_test main.cpp)
