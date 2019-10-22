//
// Created by Mike Smith on 2019/10/21.
//

#include "../src/random.h"
#include "../src/frame_data.h"
#include "../src/ray_data.h"
#include "../src/intersection_data.h"
#include "../src/material_data.h"
#include "../src/light_data.h"
#include "../src/onb.h"
#include "../src/sampling.h"
#include "../src/luminance.h"

using namespace metal;

kernel void sample_lights(
    device RayData *ray_buffer [[buffer(0)]],
    device const IntersectionData *intersection_buffer [[buffer(1)]],
    device const LightData *light_buffer [[buffer(2)]],
    device ShadowRayData *shadow_ray_buffer [[buffer(3)]],
    constant uint &light_count [[buffer(4)]],
    constant FrameData &frame_data [[buffer(5)]],
    uint2 tid [[thread_position_in_grid]]) {
    
    if (tid.x < frame_data.size.x && tid.y < frame_data.size.y) {
        
        auto index = tid.y * frame_data.size.x + tid.x;
        
        auto ray = ray_buffer[index];
        auto its_dist = intersection_buffer[index].distance;
        
        ShadowRayData shadow_ray{};
        if (ray.max_distance <= 0.0f || its_dist <= 0.0f) {  // no intersection
            shadow_ray.max_distance = -1.0f;  // terminate the ray
        } else {  // has an intersection
            auto P = ray.origin + its_dist * ray.direction;
            auto light = light_buffer[min(static_cast<uint>(halton(ray.seed) * light_count), light_count - 1u)];
            auto L = light.position - P;
            auto dist = length(L);
            auto inv_dist = 1.0f / dist;
            shadow_ray.direction = L * inv_dist;
            shadow_ray.origin = P + 1e-3f * shadow_ray.direction;
            shadow_ray.min_distance = 1e-3f;
            shadow_ray.max_distance = dist - 1e-3f;
            shadow_ray.light_radiance = light.emission * inv_dist * inv_dist;
            shadow_ray.light_pdf = 1.0f / light_count;
        }
        ray_buffer[index].seed = ray.seed;
        shadow_ray_buffer[index] = shadow_ray;
    }
}

kernel void trace_radiance(
    device RayData *ray_buffer [[buffer(0)]],
    device const ShadowRayData *shadow_ray_buffer [[buffer(1)]],
    device const IntersectionData *its_buffer [[buffer(2)]],
    device const ShadowIntersectionData *shadow_its_buffer [[buffer(3)]],
    device const Vec3f *n_buffer [[buffer(4)]],
    device const uint *material_id_buffer [[buffer(5)]],
    device const MaterialData *material_buffer [[buffer(6)]],
    constant FrameData &frame_data [[buffer(7)]],
    uint2 tid [[thread_position_in_grid]]) {
    
    if (tid.x < frame_data.size.x && tid.y < frame_data.size.y) {
        
        auto index = tid.y * frame_data.size.x + tid.x;
        auto ray = ray_buffer[index];
        auto its = its_buffer[index];
        
        if (ray.max_distance <= 0.0f || its.distance <= 0.0f) {  // no intersection
            ray.max_distance = -1.0f;  // terminate the ray
//            if (ray.depth == 0) {
//                ray.radiance = PackedVec3f{0.3f, 0.2f, 0.1f};  // background
//            }
        } else {
            auto uvw = Vec3f(its.barycentric, 1.0f - its.barycentric.x - its.barycentric.y);
            auto P = ray.origin + ray.direction * its.distance;
            auto material = material_buffer[material_id_buffer[its.triangle_index]];
            auto V = normalize(-ray.direction);
            auto i0 = its.triangle_index * 3;
            auto N = normalize(uvw.x * n_buffer[i0] + uvw.y * n_buffer[i0 + 1] + uvw.z * n_buffer[i0 + 2]);
            auto NdotV = dot(N, V);
            if (NdotV < 0.0f) {
                N = -N;
                NdotV = -NdotV;
            }
            
            // direct lighting
            auto shadow_its = shadow_its_buffer[index];
            if (!material.is_mirror && shadow_its.distance < 0.0f) {  // not occluded
                auto shadow_ray = shadow_ray_buffer[index];
                auto L = shadow_ray.direction;
                auto NdotL = max(dot(N, L), 0.0f);
                ray.radiance += ray.throughput * material.albedo * NdotL * shadow_ray.light_radiance / shadow_ray.light_pdf;
            }
            
            // sampling brdf
            ray.max_distance = INFINITY;
            if (material.is_mirror) {
                ray.direction = normalize(2.0f * NdotV * N - V);
            } else {
                ray.direction = normalize(Onb{N}.inverse_transform(Vec3f(cosine_sample_hemisphere(halton(ray.seed), halton(ray.seed)))));
                ray.depth++;
                if (ray.depth > 3) {  // RR
                    auto q = max(0.05f, 1.0f - luminance(ray.throughput));
                    if (halton(ray.seed) < q) {
                        ray.max_distance = -1.0f;
                    } else {
                        ray.throughput /= 1.0f - q;
                    }
                }
            }
            ray.throughput *= material.albedo;
            ray.origin = P + 1e-3f * ray.direction;
        }
        ray_buffer[index] = ray;
    }
    
}