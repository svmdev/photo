cmake_minimum_required(VERSION 3.5)
project(photo)

add_subdirectory(/t/dlib/dlib-20.0 /t/dlib/build)
set(OpenCV_DIR /t/opencv/build)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

set(TARGET_SRC photo.cpp)
add_executable(photo main.cpp ${TARGET_SRC})
target_link_libraries(photo PRIVATE ${OpenCV_LIBS} dlib::dlib)