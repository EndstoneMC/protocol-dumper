include(ExternalProject)
ExternalProject_Add(bedrock_server
        URL https://www.minecraft.net/bedrockdedicatedserver/bin-linux-preview/bedrock-server-1.26.30.26.zip
        URL_HASH SHA256=fa3dd3c6c4df171734d2264b0b5e216ed05ebb35675ae630fa66bbe038d2f3f2
        HTTP_HEADER "User-Agent: Mozilla/5.0"
        SOURCE_DIR ${CMAKE_BINARY_DIR}/bedrock-server
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
)
