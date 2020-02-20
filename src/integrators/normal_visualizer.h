//
// Created by Mike Smith on 2020/2/19.
//

#pragma once

#include <core/data_types.h>
#include <core/colorspaces.h>

namespace luisa::integrator::normal {

LUISA_DEVICE_CALLABLE inline void colorize_normals(
    LUISA_DEVICE_SPACE float3 *normals,
    LUISA_DEVICE_SPACE const bool *valid_buffer,
    uint pixel_count,
    uint tid) noexcept {
    
    if (tid < pixel_count) {
        auto n = valid_buffer[tid] ? normals[tid] : make_float3(-1.0f);
        normals[tid] = XYZ2ACEScg(RGB2XYZ(n * 0.5f + 0.5f));
    }
    
}

}

#ifndef LUISA_DEVICE_COMPATIBLE

#include <core/integrator.h>
#include <core/geometry.h>

namespace luisa {

class NormalVisualizer : public Integrator {

protected:
    std::unique_ptr<Buffer<uint>> _ray_count;
    std::unique_ptr<Buffer<Ray>> _ray_buffer;
    std::unique_ptr<Buffer<float2>> _ray_pixel_buffer;
    std::unique_ptr<Buffer<float3>> _ray_throughput_buffer;
    std::unique_ptr<Buffer<ClosestHit>> _hit_buffer;
    InteractionBufferSet _interaction_buffers;
    
    // kernels
    std::unique_ptr<Kernel> _colorize_normals_kernel;
    
    void _prepare_for_frame() override;

public:
    NormalVisualizer(Device *device, const ParameterSet &parameter_set[[maybe_unused]]);
    void render_frame(KernelDispatcher &dispatch) override;
    
};

LUISA_REGISTER_NODE_CREATOR("Normal", NormalVisualizer)

}

#endif
