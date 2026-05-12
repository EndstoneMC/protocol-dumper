cmake_minimum_required(VERSION 3.28)
# CMP0135 NEW: FetchContent URL extracts get extraction-time mtimes; the OLD
# behaviour confuses mtime-based rebuilds after a re-fetch.
cmake_policy(SET CMP0135 NEW)
project(protocol_dumper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BEDROCK_SERVER_VERSION "latest" CACHE STRING "Bedrock preview server version (e.g. 1.26.30.26), or 'latest'")

add_subdirectory(third_party)

add_library(protocol_dumper SHARED
        src/main.cpp
        src/visitor.cpp
)
target_include_directories(protocol_dumper PRIVATE src)
# Pin expected-lite's nonstd::expected backend; the default flips to
# std::expected on toolchains that ship it, which has different ADL.
target_compile_definitions(protocol_dumper PRIVATE
        nsel_CONFIG_SELECT_EXPECTED=nsel_EXPECTED_NONSTD
        BEDROCK_SERVER_VERSION_MAJOR=${BEDROCK_SERVER_VERSION_MAJOR}
        BEDROCK_SERVER_VERSION_MINOR=${BEDROCK_SERVER_VERSION_MINOR}
        BEDROCK_SERVER_VERSION_PATCH=${BEDROCK_SERVER_VERSION_PATCH}
        BEDROCK_SERVER_VERSION_BUILD=${BEDROCK_SERVER_VERSION_BUILD}
)
target_link_libraries(protocol_dumper PRIVATE EnTT::EnTT nlohmann_json::nlohmann_json libhat::libhat expected-lite funchook-static spdlog::spdlog)
# Default shared-lib linking lets undefined refs slip to runtime; surface
# them as link errors instead of LD_PRELOAD-time loader failures.
target_link_options(protocol_dumper PRIVATE -Wl,--no-undefined)

find_program(PATCHELF patchelf REQUIRED)
# bedrock-server is EXCLUDE_FROM_ALL; we don't link it but POST_BUILD
# rewrites its binary, so force the fetch alongside protocol_dumper.
add_dependencies(protocol_dumper bedrock-server)
# Drop the .so beside bedrock_server, mark it DT_NEEDED, set rpath to $ORIGIN.
# --remove-needed first keeps re-builds idempotent (otherwise duplicates).
add_custom_command(TARGET protocol_dumper POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:protocol_dumper> $<TARGET_FILE_DIR:bedrock_server>/
        COMMAND ${PATCHELF} --remove-needed $<TARGET_FILE_NAME:protocol_dumper> $<TARGET_FILE:bedrock_server>
        COMMAND ${PATCHELF} --add-needed $<TARGET_FILE_NAME:protocol_dumper> $<TARGET_FILE:bedrock_server>
        COMMAND ${PATCHELF} --set-rpath $ORIGIN $<TARGET_FILE:bedrock_server>
        COMMENT "Injecting $<TARGET_FILE_NAME:protocol_dumper> into $<TARGET_FILE_NAME:bedrock_server>"
        VERBATIM
)
