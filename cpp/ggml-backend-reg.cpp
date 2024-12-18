#include "ggml-backend-impl.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
#include "ggml-impl.h"
#include <cstring>
#include <vector>

// Backend registry

#ifdef WSP_GGML_USE_CUDA
#include "ggml-cuda.h"
#endif

#ifdef WSP_GGML_USE_METAL
#include <TargetConditionals.h>

#if !TARGET_OS_SIMULATOR
#include "ggml-metal.h"
#endif

#endif

#ifdef WSP_GGML_USE_SYCL
#include "ggml-sycl.h"
#endif

#ifdef WSP_GGML_USE_VULKAN
#include "ggml-vulkan.h"
#endif

#ifdef WSP_GGML_USE_BLAS
#include "ggml-blas.h"
#endif

#ifdef WSP_GGML_USE_RPC
#include "ggml-rpc.h"
#endif

#ifdef WSP_GGML_USE_AMX
#  include "ggml-amx.h"
#endif

#ifdef WSP_GGML_USE_CANN
#include "ggml-cann.h"
#endif

#ifdef WSP_GGML_USE_KOMPUTE
#include "ggml-kompute.h"
#endif

struct wsp_ggml_backend_registry {
    std::vector<wsp_ggml_backend_reg_t> backends;
    std::vector<wsp_ggml_backend_dev_t> devices;

    wsp_ggml_backend_registry() {
#ifdef WSP_GGML_USE_CUDA
        register_backend(wsp_ggml_backend_cuda_reg());
#endif
#ifdef WSP_GGML_USE_METAL

#if !TARGET_OS_SIMULATOR
        register_backend(wsp_ggml_backend_metal_reg());
#endif

#endif
#ifdef WSP_GGML_USE_SYCL
        register_backend(wsp_ggml_backend_sycl_reg());
#endif
#ifdef WSP_GGML_USE_VULKAN
        register_backend(wsp_ggml_backend_vk_reg());
#endif
#ifdef WSP_GGML_USE_CANN
        register_backend(wsp_ggml_backend_cann_reg());
#endif
#ifdef WSP_GGML_USE_BLAS
        register_backend(wsp_ggml_backend_blas_reg());
#endif
#ifdef WSP_GGML_USE_RPC
        register_backend(wsp_ggml_backend_rpc_reg());
#endif
#ifdef WSP_GGML_USE_AMX
        register_backend(wsp_ggml_backend_amx_reg());
#endif
#ifdef WSP_GGML_USE_KOMPUTE
        register_backend(wsp_ggml_backend_kompute_reg());
#endif

        register_backend(wsp_ggml_backend_cpu_reg());
    }

    void register_backend(wsp_ggml_backend_reg_t reg) {
        if (!reg) {
            return;
        }

#ifndef NDEBUG
        WSP_GGML_LOG_DEBUG("%s: registered backend %s (%zu devices)\n",
            __func__, wsp_ggml_backend_reg_name(reg), wsp_ggml_backend_reg_dev_count(reg));
#endif
        backends.push_back(reg);
        for (size_t i = 0; i < wsp_ggml_backend_reg_dev_count(reg); i++) {
            register_device(wsp_ggml_backend_reg_dev_get(reg, i));
        }
    }

    void register_device(wsp_ggml_backend_dev_t device) {
#ifndef NDEBUG
        WSP_GGML_LOG_DEBUG("%s: registered device %s (%s)\n", __func__, wsp_ggml_backend_dev_name(device), wsp_ggml_backend_dev_description(device));
#endif
        devices.push_back(device);
    }
};

static wsp_ggml_backend_registry & get_reg() {
    static wsp_ggml_backend_registry reg;
    return reg;
}

// Internal API
void wsp_ggml_backend_register(wsp_ggml_backend_reg_t reg) {
    get_reg().register_backend(reg);
}

void wsp_ggml_backend_device_register(wsp_ggml_backend_dev_t device) {
    get_reg().register_device(device);
}

// Backend (reg) enumeration
size_t wsp_ggml_backend_reg_count() {
    return get_reg().backends.size();
}

wsp_ggml_backend_reg_t wsp_ggml_backend_reg_get(size_t index) {
    WSP_GGML_ASSERT(index < wsp_ggml_backend_reg_count());
    return get_reg().backends[index];
}

wsp_ggml_backend_reg_t wsp_ggml_backend_reg_by_name(const char * name) {
    for (size_t i = 0; i < wsp_ggml_backend_reg_count(); i++) {
        wsp_ggml_backend_reg_t reg = wsp_ggml_backend_reg_get(i);
        if (std::strcmp(wsp_ggml_backend_reg_name(reg), name) == 0) {
            return reg;
        }
    }
    return NULL;
}

// Device enumeration
size_t wsp_ggml_backend_dev_count() {
    return get_reg().devices.size();
}

wsp_ggml_backend_dev_t wsp_ggml_backend_dev_get(size_t index) {
    WSP_GGML_ASSERT(index < wsp_ggml_backend_dev_count());
    return get_reg().devices[index];
}

wsp_ggml_backend_dev_t wsp_ggml_backend_dev_by_name(const char * name) {
    for (size_t i = 0; i < wsp_ggml_backend_dev_count(); i++) {
        wsp_ggml_backend_dev_t dev = wsp_ggml_backend_dev_get(i);
        if (strcmp(wsp_ggml_backend_dev_name(dev), name) == 0) {
            return dev;
        }
    }
    return NULL;
}

wsp_ggml_backend_dev_t wsp_ggml_backend_dev_by_type(enum wsp_ggml_backend_dev_type type) {
    for (size_t i = 0; i < wsp_ggml_backend_dev_count(); i++) {
        wsp_ggml_backend_dev_t dev = wsp_ggml_backend_dev_get(i);
        if (wsp_ggml_backend_dev_type(dev) == type) {
            return dev;
        }
    }
    return NULL;
}

// Convenience functions
wsp_ggml_backend_t wsp_ggml_backend_init_by_name(const char * name, const char * params) {
    wsp_ggml_backend_dev_t dev = wsp_ggml_backend_dev_by_name(name);
    if (!dev) {
        return NULL;
    }
    return wsp_ggml_backend_dev_init(dev, params);
}

wsp_ggml_backend_t wsp_ggml_backend_init_by_type(enum wsp_ggml_backend_dev_type type, const char * params) {
    wsp_ggml_backend_dev_t dev = wsp_ggml_backend_dev_by_type(type);
    if (!dev) {
        return NULL;
    }
    return wsp_ggml_backend_dev_init(dev, params);
}

wsp_ggml_backend_t wsp_ggml_backend_init_best(void) {
    wsp_ggml_backend_dev_t dev = wsp_ggml_backend_dev_by_type(WSP_GGML_BACKEND_DEVICE_TYPE_GPU);
    if (!dev) {
        dev = wsp_ggml_backend_dev_by_type(WSP_GGML_BACKEND_DEVICE_TYPE_CPU);
    }
    if (!dev) {
        return NULL;
    }
    return wsp_ggml_backend_dev_init(dev, NULL);
}
