include(FetchContent)

if (BEDROCK_SERVER_VERSION_MINOR LESS 26)
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/7111549c7c31e64ba0a10356b25652ce7d08b4a3.tar.gz)
else ()
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/1333fa53129e7cfded5a9640c4336a254049917b.tar.gz)
endif ()
FetchContent_MakeAvailable(entt)
