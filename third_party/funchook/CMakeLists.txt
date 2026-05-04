include(FetchContent)
# Git, not URL: the release tarball drops the distorm submodule. capstone backend
# is out — it sub-fetches capstone 4.0.2, which sets CMP0048 OLD (now rejected).
FetchContent_Declare(funchook
        GIT_REPOSITORY https://github.com/kubo/funchook.git
        GIT_TAG v1.1.3
        GIT_SHALLOW TRUE
        GIT_SUBMODULES distorm
)
# Static-only; skip funchook's own tests/install.
set(FUNCHOOK_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(FUNCHOOK_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(FUNCHOOK_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FUNCHOOK_INSTALL OFF CACHE BOOL "" FORCE)
set(FUNCHOOK_DISASM "distorm" CACHE STRING "" FORCE)
FetchContent_MakeAvailable(funchook)
