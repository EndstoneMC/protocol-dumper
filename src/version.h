#pragma once
// BDS version packed one byte per component as 0xMMmm_ppbb, à la PYBIND11_VERSION_HEX.

#ifndef BEDROCK_SERVER_VERSION_BUILD
#define BEDROCK_SERVER_VERSION_BUILD 0
#endif

#define BEDROCK_SERVER_VERSION_HEX                                                 \
    ((BEDROCK_SERVER_VERSION_MAJOR << 24) | (BEDROCK_SERVER_VERSION_MINOR << 16) | \
     (BEDROCK_SERVER_VERSION_PATCH << 8) | (BEDROCK_SERVER_VERSION_BUILD))
