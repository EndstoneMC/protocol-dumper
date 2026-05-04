include(FetchContent)
FetchContent_Declare(expected-lite
        URL https://github.com/nonstd-lite/expected-lite/archive/refs/tags/v0.10.0.tar.gz
        URL_HASH SHA256=cfe082e4ffedeeedac47763504102646a39c080599c7c1fe99299d6a1f99af92
)
FetchContent_MakeAvailable(expected-lite)
