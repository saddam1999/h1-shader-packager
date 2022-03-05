cmake_minimum_required(VERSION 3.22.1)

project(h1sp)

add_executable(${PROJECT_NAME}
    src/archive.cpp
    src/crypt.cpp
    src/names.cpp
    src/main.cpp)

target_compile_features(
    ${PROJECT_NAME}
    PRIVATE
        cxx_std_20
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        crypto
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)