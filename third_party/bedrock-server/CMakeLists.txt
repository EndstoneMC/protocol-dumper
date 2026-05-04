include(ExternalProject)
# minecraft.net's CDN HTTP/2-framing-errors libcurl's default UA; any browser
# UA works. EXCLUDE_FROM_ALL keeps the 60+ MB fetch off the default build.
ExternalProject_Add(bedrock-server
        URL https://www.minecraft.net/bedrockdedicatedserver/bin-linux-preview/bedrock-server-1.26.30.26.zip
        URL_HASH SHA256=fa3dd3c6c4df171734d2264b0b5e216ed05ebb35675ae630fa66bbe038d2f3f2
        HTTP_HEADER "User-Agent: Mozilla/5.0"
        SOURCE_DIR ${CMAKE_BINARY_DIR}/bedrock-server
        CONFIGURE_COMMAND ""
        BUILD_COMMAND ""
        INSTALL_COMMAND ""
        EXCLUDE_FROM_ALL TRUE
)
# Two-target split: the kebab one above drives the fetch (add_dependencies
# can't target IMPORTED), the snake one exposes the path via $<TARGET_FILE:>.
add_executable(bedrock_server IMPORTED GLOBAL)
set_target_properties(bedrock_server PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/bedrock-server/bedrock_server)
