cmake_minimum_required(VERSION 3.21)
project(proto_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.x
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(entt spdlog argparse)

add_library(proto_dumper SHARED
    src/main.cpp
    src/generator.cpp
    src/reader.cpp
)
target_link_libraries(proto_dumper PRIVATE EnTT::EnTT spdlog::spdlog)

add_executable(injector src/injector.cpp)
target_link_libraries(injector PRIVATE spdlog::spdlog argparse::argparse)
