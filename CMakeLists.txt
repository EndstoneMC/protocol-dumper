cmake_minimum_required(VERSION 3.29)
cmake_policy(SET CMP0135 NEW)
project(protocol_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(entt
        URL https://github.com/skypjack/entt/archive/1333fa53129e7cfded5a9640c4336a254049917b.tar.gz
        URL_HASH SHA256=7905419d002027a2486005fa2483dda3b26532eb1294bc791b2dcd243dcf9c1f
)
FetchContent_Declare(json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_Declare(libhat
        URL https://github.com/BasedInc/libhat/archive/refs/tags/v0.9.0.tar.gz
        URL_HASH SHA256=b1e9e16e804d9f9bec7ccc4df4b8a1cdd281b6ec0c1377244ab28a7944a3e13a
)
FetchContent_Declare(expected-lite
        URL https://github.com/nonstd-lite/expected-lite/archive/refs/tags/v0.10.0.tar.gz
        URL_HASH SHA256=cfe082e4ffedeeedac47763504102646a39c080599c7c1fe99299d6a1f99af92
)
FetchContent_Declare(funchook
        GIT_REPOSITORY https://github.com/kubo/funchook.git
        GIT_TAG v1.1.3
        GIT_SHALLOW TRUE
        GIT_SUBMODULES distorm
)
set(FUNCHOOK_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(FUNCHOOK_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(FUNCHOOK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FUNCHOOK_INSTALL OFF CACHE BOOL "" FORCE)
set(FUNCHOOK_DISASM "distorm" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(entt json libhat expected-lite funchook)

add_library(protocol_dumper SHARED
        src/main.cpp
        src/visitor.cpp
)
target_include_directories(protocol_dumper PRIVATE src)
target_compile_definitions(protocol_dumper PRIVATE nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD)
target_link_libraries(protocol_dumper PRIVATE EnTT::EnTT nlohmann_json::nlohmann_json libhat::libhat expected-lite funchook-static)
