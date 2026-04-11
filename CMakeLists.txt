cmake_minimum_required(VERSION 3.21)
project(proto_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- entt (header-only, for cereal meta reflection) ---
include(FetchContent)
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        main
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(entt)

add_library(proto_dumper SHARED
    src/main.cpp
    src/generator.cpp
    src/reader.cpp
)
target_link_libraries(proto_dumper PRIVATE EnTT::EnTT)

add_executable(injector src/injector.cpp)
