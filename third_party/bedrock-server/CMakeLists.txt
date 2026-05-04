include(ExternalProject)

set(BEDROCK_SERVER_VERSION "latest" CACHE STRING
        "Bedrock preview server version (e.g. 1.26.30.26), or 'latest' to query Mojang's API")

# 'latest' resolves at configure time via Mojang's download-links endpoint.
# A pinned version skips the API and constructs the URL directly.
if(BEDROCK_SERVER_VERSION STREQUAL "latest")
    set(_links_json ${CMAKE_CURRENT_BINARY_DIR}/download-links.json)
    file(DOWNLOAD
            "https://net-secondary.web.minecraft-services.net/api/v1.0/download/links"
            ${_links_json}
            HTTPHEADER "User-Agent: Mozilla/5.0"
            STATUS _dl_status)
    list(GET _dl_status 0 _dl_code)
    if(NOT _dl_code EQUAL 0)
        list(GET _dl_status 1 _dl_msg)
        message(FATAL_ERROR "Failed to fetch Bedrock download links: ${_dl_msg}")
    endif()
    file(READ ${_links_json} _links)
    string(JSON _count LENGTH ${_links} result links)
    math(EXPR _last "${_count} - 1")
    set(BEDROCK_SERVER_URL "")
    foreach(_i RANGE 0 ${_last})
        string(JSON _type GET ${_links} result links ${_i} downloadType)
        if(_type STREQUAL "serverBedrockPreviewLinux")
            string(JSON BEDROCK_SERVER_URL GET ${_links} result links ${_i} downloadUrl)
            break()
        endif()
    endforeach()
    if(BEDROCK_SERVER_URL STREQUAL "")
        message(FATAL_ERROR "serverBedrockPreviewLinux entry not present in download-links response")
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
