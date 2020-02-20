//
// Created by Mike Smith on 2020/2/9.
//

#pragma once

#include "node.h"
#include "parser.h"
#include "mathematics.h"

namespace luisa {

class Transform : public Node {

private:
    LUISA_MAKE_NODE_CREATOR_REGISTRY(Transform);

public:
    explicit Transform(Device *device) noexcept : Node{device} {}  // for creating default, i.e., identity transforms
    Transform(Device *device, const ParameterSet &) : Node{device} {}
    [[nodiscard]] virtual float4x4 static_matrix() const { return math::identity(); }
    [[nodiscard]] virtual float4x4 dynamic_matrix(float time[[maybe_unused]]) const {
        LUISA_ERROR_IF_NOT(is_static(), "Transform::dynamic_matrix() not implemented in dynamic transform");
        return math::identity();
    }
    [[nodiscard]] virtual bool is_static() const noexcept { return true; }
};

}
