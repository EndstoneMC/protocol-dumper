include(FetchContent)

if (BEDROCK_SERVER_VERSION_MINOR LESS 26)
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/7111549c7c31e64ba0a10356b25652ce7d08b4a3.tar.gz)
elseif (BEDROCK_SERVER_VERSION_PATCH LESS 50)
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/1333fa53129e7cfded5a9640c4336a254049917b.tar.gz)
else ()
    # 1.26.50 picked up "meta: refine meta_data_node", which reshaped meta_data_node
    # and moved the descriptor containers behind entt::stl.
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/2b9d0684e0f1cc4714258428f85fd82f7623b5af.tar.gz)
endif ()
FetchContent_MakeAvailable(entt)
