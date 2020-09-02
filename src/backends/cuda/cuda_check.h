//
// Created by Mike on 8/27/2020.
//

#pragma once

#include <core/logging.h>

#define NVRTC_CHECK(x)                                                                                  \
    [&] {                                                                                               \
        nvrtcResult result = x;                                                                         \
        LUISA_ERROR_IF_NOT(                                                                             \
            result == NVRTC_SUCCESS, "MVRTC call [ " #x " ] failed with ", nvrtcGetErrorString(result), \
            " (", __FILE__, ":", __LINE__, ").");                                                       \
    }()

#define CUDA_CHECK(x)                                                                                   \
    [&] {                                                                                               \
        CUresult result = x;                                                                            \
        if (result != CUDA_SUCCESS) {                                                                   \
            const char *msg;                                                                            \
            cuGetErrorName(result, &msg);                                                               \
            LUISA_ERROR("CUDA call [ " #x " ] failed with ", msg, " (", __FILE__, ":", __LINE__, ")."); \
        }                                                                                               \
    }()
