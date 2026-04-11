cmake_minimum_required(VERSION 3.21)
project(proto_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(entt
    URL      https://github.com/skypjack/entt/archive/refs/tags/v3.16.0.tar.gz
    URL_HASH SHA256=7d7b4037b737992342049ffab14f22fa10243e01664f8c3a0657aa247ac52f71
)

FetchContent_Declare(spdlog
    URL      https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
)

FetchContent_Declare(argparse
    URL      https://github.com/p-ranav/argparse/archive/refs/tags/v3.2.tar.gz
    URL_HASH SHA256=9dcb3d8ce0a41b2a48ac8baa54b51a9f1b6a2c52dd374e28cc713bab0568ec98
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
