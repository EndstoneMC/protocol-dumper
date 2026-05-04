cmake_minimum_required(VERSION 3.29)
cmake_policy(SET CMP0135 NEW)
project(protocol_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party)

add_library(protocol_dumper SHARED
        src/main.cpp
        src/visitor.cpp
)
target_include_directories(protocol_dumper PRIVATE src)
target_compile_definitions(protocol_dumper PRIVATE nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD)
target_link_libraries(protocol_dumper PRIVATE EnTT::EnTT nlohmann_json::nlohmann_json libhat::libhat expected-lite funchook-static)
target_link_options(protocol_dumper PRIVATE -Wl,--no-undefined)

find_program(PATCHELF patchelf REQUIRED)
add_dependencies(protocol_dumper bedrock-server)
add_custom_command(TARGET protocol_dumper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE:protocol_dumper> $<TARGET_FILE_DIR:bedrock_server>/
        COMMAND ${PATCHELF} --remove-needed $<TARGET_FILE_NAME:protocol_dumper> $<TARGET_FILE:bedrock_server>
        COMMAND ${PATCHELF} --add-needed $<TARGET_FILE_NAME:protocol_dumper> $<TARGET_FILE:bedrock_server>
        COMMAND ${PATCHELF} --set-rpath $ORIGIN $<TARGET_FILE:bedrock_server>
        COMMENT "Injecting $<TARGET_FILE_NAME:protocol_dumper> into $<TARGET_FILE_NAME:bedrock_server>"
        VERBATIM
)
