include(ExternalProject)

set(BEDROCK_SERVER_VERSION "latest" CACHE STRING
        "Bedrock preview server version (e.g. 1.26.30.26), or 'latest' to query Mojang's API")

# 'latest' resolves at configure time via Mojang's download-links endpoint;
# a pinned version skips the API and constructs the URL directly.
if(BEDROCK_SERVER_VERSION STREQUAL "latest")
    file(DOWNLOAD
            "https://net-secondary.web.minecraft-services.net/api/v1.0/download/links"
            ${CMAKE_CURRENT_BINARY_DIR}/download-links.json
            HTTPHEADER "User-Agent: Mozilla/5.0"
            STATUS _dl)
    if(NOT _dl MATCHES "^0;")
        message(FATAL_ERROR "Bedrock download-links fetch failed: ${_dl}")
    endif()
    file(READ ${CMAKE_CURRENT_BINARY_DIR}/download-links.json _links)
    string(JSON _count LENGTH ${_links} result links)
    math(EXPR _last "${_count} - 1")
    foreach(_i RANGE 0 ${_last})
        string(JSON _type GET ${_links} result links ${_i} downloadType)
        if(_type STREQUAL "serverBedrockPreviewLinux")
            string(JSON BEDROCK_SERVER_URL GET ${_links} result links ${_i} downloadUrl)
            break()
        endif()
    endforeach()
    if(NOT BEDROCK_SERVER_URL)
        message(FATAL_ERROR "serverBedrockPreviewLinux missing from download-links response")
    endif()
    message(STATUS "Bedrock server (latest preview): ${BEDROCK_SERVER_URL}")
else()
    set(BEDROCK_SERVER_URL "https://www.minecraft.net/bedrockdedicatedserver/bin-linux-preview/bedrock-server-${BEDROCK_SERVER_VERSION}.zip")
endif()

# minecraft.net's CDN HTTP/2-framing-errors libcurl's default UA; any browser
# UA works. EXCLUDE_FROM_ALL keeps the 60+ MB fetch off the default build.
# URL_HASH is intentionally absent: 'latest' is a moving target.
ExternalProject_Add(bedrock-server
        URL ${BEDROCK_SERVER_URL}
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
