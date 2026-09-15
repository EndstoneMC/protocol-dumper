#pragma once
// BDS version packed one byte per component as 0xMMmm_ppbb, à la PYBIND11_VERSION_HEX.
//
// A stable build carries every change its update line's previews made but restarts the
// build number well below them (1.26.50.5 ships what preview 1.26.50.27 introduced), so
// its build byte saturates - a stable sorts above every preview of the same patch line.

#ifndef BEDROCK_SERVER_VERSION_BUILD
#define BEDROCK_SERVER_VERSION_BUILD 0
#endif

#ifndef BEDROCK_SERVER_VERSION_STABLE
#define BEDROCK_SERVER_VERSION_STABLE 0
#endif

#if BEDROCK_SERVER_VERSION_STABLE
#define BEDROCK_SERVER_VERSION_GATE_BUILD 0xFF
#else
#define BEDROCK_SERVER_VERSION_GATE_BUILD BEDROCK_SERVER_VERSION_BUILD
#endif

#define BEDROCK_SERVER_VERSION_ENCODE(major, minor, patch, build) \
    (((major) << 24) | ((minor) << 16) | ((patch) << 8) | (build))

#define BEDROCK_SERVER_VERSION_HEX                                                            \
    BEDROCK_SERVER_VERSION_ENCODE(BEDROCK_SERVER_VERSION_MAJOR, BEDROCK_SERVER_VERSION_MINOR, \
                                  BEDROCK_SERVER_VERSION_PATCH, BEDROCK_SERVER_VERSION_GATE_BUILD)
