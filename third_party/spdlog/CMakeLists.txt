include(FetchContent)
FetchContent_Declare(spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.17.0.tar.gz
        URL_HASH SHA256=d8862955c6d74e5846b3f580b1605d2428b11d97a410d86e2fb13e857cd3a744
)
# We link the static spdlog into a SHARED library, so it needs PIC.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
FetchContent_MakeAvailable(spdlog)
