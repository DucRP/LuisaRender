#include <compute/device.h>
#include <compute/dsl.h>

#include <render/filter.h>

LUISA_STRUCT(luisa::filter::separable::LUT, w, cdf)
LUISA_STRUCT(luisa::filter::separable::ImportanceSamplePixelsKernelUniforms, tile, radius, scale)

using namespace luisa;
using namespace luisa::compute::dsl;

int main(int argc, char *argv[]) {
    
    auto runtime_directory = std::filesystem::canonical(argv[0]).parent_path().parent_path();
    auto working_directory = std::filesystem::canonical(std::filesystem::current_path());
    Context context{runtime_directory, working_directory};
    auto device = Device::create(&context, "metal");
    
    std::cout << "\n================= CODEGEN =================\n" << std::endl;
    
    auto sample_1d = LUISA_LAMBDA(Copy<float> u, Ref lut) {
        
        UInt32 p{0u};
        Auto count{static_cast<uint32_t>(TABLE_SIZE)};
        While(count > 0) {
            Auto step{count / 2};
            Auto mid{p + step};
            If(lut.$(cdf)[mid] < u) {
                p = mid + 1;
                count -= step + 1;
            } Else {
                count = step;
            };
        };
        
        constexpr auto inv_table_size = 1.0f / static_cast<float>(TABLE_SIZE);
        
        Auto lb{clamp(p, 0u, TABLE_SIZE - 1u)};
        Auto cdf_lower{lut.$(cdf)[lb]};
        Auto cdf_upper{select(lb == TABLE_SIZE - 1u, 1.0f, lut.$(cdf)[lb + 1u])};
        Auto offset{clamp(cast<float>(lb) + (u - cdf_lower) / (cdf_upper - cdf_lower) * inv_table_size, 0.0f, 1.0f)};
        
        constexpr auto weight_table_size_float = static_cast<float>(TABLE_SIZE);
        Auto index_w{offset * weight_table_size_float};
        Auto index_w_lower{floor(index_w)};
        Auto index_w_upper{ceil(index_w)};
        Auto w{lerp(
            lut.$(w)[cast<uint32_t>(index_w_lower)],
            select(index_w_upper >= weight_table_size_float, 0.0f, lut.$(w)[cast<uint32_t>(index_w_upper)]),
            index_w - index_w_lower)};
        return std::make_pair(Auto{offset * 2.0f - 1.0f}, Auto{select(w >= 0.0f, 1.0f, -1.0f)});
    };
    
    luisa::compute::BufferView<int> empty;
    
    auto kernel = device->compile_kernel("foo", LUISA_FUNC {
        
        Arg<const float2 *> random_buffer{empty};
        Arg<float2 *> pixel_location_buffer{empty};
        Arg<float3 *> pixel_weight_buffer{empty};
        Arg<LUT> lut{empty};
        Arg<ImportanceSamplePixelsKernelUniforms> uniforms{empty};
        
        Arg<std::array<Tex2D<luisa::compute::TextureAccess::READ_WRITE>, 10>> texture{empty};
        
        Auto a{0};
        Auto p{&a};
        
        Switch(a) {
            Case(0) {
                Break;
            };
            Default {
                Break;
            };
        };
        
        auto tid = thread_id();
        auto tile = uniforms.$(tile);
        If(tid < tile.$(size).x() * tile.$(size).y()) {
            Auto u{random_buffer[tid]};
            auto[x, wx] = sample_1d(u.x(), lut);
            auto[y, wy] = sample_1d(u.y(), lut);
            pixel_location_buffer[tid] = make_float2(
                cast<float>(tid % tile.$(size).x() + tile.$(origin).y()) + 0.5f + x * uniforms.$(radius),
                cast<float>(tid / tile.$(size).x() + tile.$(origin).y()) + 0.5f + y * uniforms.$(radius));
            pixel_weight_buffer[tid] = make_float3(wx * wy * uniforms.$(scale));
        };
    });
}
