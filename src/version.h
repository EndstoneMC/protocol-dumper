#pragma once
// BDS version packed one byte per component as 0xMMmm_ppbb, à la PYBIND11_VERSION_HEX.

#ifndef BEDROCK_SERVER_VERSION_BUILD
#define BEDROCK_SERVER_VERSION_BUILD 0
#endif

#define BEDROCK_SERVER_VERSION_ENCODE(major, minor, patch, build) \
    (((major) << 24) | ((minor) << 16) | ((patch) << 8) | (build))

#define BEDROCK_SERVER_VERSION_HEX                                                            \
    BEDROCK_SERVER_VERSION_ENCODE(BEDROCK_SERVER_VERSION_MAJOR, BEDROCK_SERVER_VERSION_MINOR, \
                                  BEDROCK_SERVER_VERSION_PATCH, BEDROCK_SERVER_VERSION_BUILD)
