//
// Created by Mike on 2021/12/8.
//

#pragma once

#include <dsl/syntax.h>
#include <scene/scene_node.h>

namespace luisa::render {

using compute::Expr;
using compute::Float;
using compute::Float2;

class Sampler : public SceneNode {

public:
    struct Instance : public SceneNode::Instance {
        virtual void start(Expr<uint2> pixel, Expr<uint> sample_index) noexcept = 0;
        virtual void save_state() noexcept = 0;
        virtual void load_state(Expr<uint2> pixel) noexcept = 0;
        [[nodiscard]] virtual Float generate_1d() noexcept = 0;
        [[nodiscard]] virtual Float2 generate_2d() noexcept = 0;
    };

public:
    Sampler(Scene *scene, const SceneNodeDesc *desc) noexcept;
    [[nodiscard]] virtual luisa::unique_ptr<Instance> build(Stream &stream, Pipeline &pipeline, uint2 resolution, uint spp) const noexcept = 0;
};

}// namespace luisa::render
