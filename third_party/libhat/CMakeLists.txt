include(FetchContent)
FetchContent_Declare(libhat
        URL https://github.com/BasedInc/libhat/archive/refs/tags/v0.9.0.tar.gz
        URL_HASH SHA256=b1e9e16e804d9f9bec7ccc4df4b8a1cdd281b6ec0c1377244ab28a7944a3e13a
)
FetchContent_MakeAvailable(libhat)
target_compile_options(libhat PRIVATE -include algorithm)
target_include_directories(libhat INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(libhat INTERFACE elf)
