include(FetchContent)

if (BEDROCK_SERVER_VERSION_MINOR LESS 26)
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/7111549c7c31e64ba0a10356b25652ce7d08b4a3.tar.gz)
elseif (BEDROCK_SERVER_VERSION_PATCH LESS 50)
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/1333fa53129e7cfded5a9640c4336a254049917b.tar.gz)
elseif (BEDROCK_SERVER_VERSION_PATCH LESS 70)
    # 1.26.50 picked up "meta: refine meta_data_node", which reshaped meta_data_node
    # and moved the descriptor containers behind entt::stl.
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/refs/tags/v4.0.0.tar.gz)
else ()
    # 1.26.70 picked up "meta: use meta_object as a base of meta_type", which swapped
    # meta_type's node and ctx. The nodes are unchanged, but every meta_type BDS returns
    # by value - meta_func::arg, the descriptors' resolvers - arrives transposed.
    FetchContent_Declare(entt URL https://github.com/skypjack/entt/archive/1dcd7aab6548786f7672ff578639188bd2f0d837.tar.gz)
endif ()
FetchContent_MakeAvailable(entt)
