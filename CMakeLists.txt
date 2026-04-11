cmake_minimum_required(VERSION 3.21)
project(proto_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(entt
    URL      https://github.com/skypjack/entt/archive/1333fa53129e7cfded5a9640c4336a254049917b.tar.gz
    URL_HASH SHA256=7905419d002027a2486005fa2483dda3b26532eb1294bc791b2dcd243dcf9c1f
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
