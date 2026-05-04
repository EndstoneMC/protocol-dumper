cmake_minimum_required(VERSION 3.29)
cmake_policy(SET CMP0135 NEW)
project(protocol_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/entt)
add_subdirectory(third_party/json)
add_subdirectory(third_party/libhat)
add_subdirectory(third_party/expected-lite)
add_subdirectory(third_party/funchook)
add_subdirectory(third_party/bedrock-server)

add_library(protocol_dumper SHARED
        src/main.cpp
        src/visitor.cpp
)
target_include_directories(protocol_dumper PRIVATE src)
target_compile_definitions(protocol_dumper PRIVATE nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD)
target_link_libraries(protocol_dumper PRIVATE EnTT::EnTT nlohmann_json::nlohmann_json libhat::libhat expected-lite funchook-static)
