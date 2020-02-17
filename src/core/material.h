//
// Created by Mike Smith on 2020/2/4.
//

#pragma once

#include <string_view>
#include <unordered_map>
#include "node.h"

namespace luisa {

class Material : public Node {

private:
    LUISA_MAKE_NODE_CREATOR_REGISTRY(Material);

private:
    std::unordered_map<std::string_view, uint32_t> _bsdf_type_ids;

public:
    [[nodiscard]] virtual bool is_emissive() const noexcept { return false; }
};

}
