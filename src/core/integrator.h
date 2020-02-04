//
// Created by Mike Smith on 2020/2/1.
//

#pragma once

#include "data_types.h"
#include "node.h"

namespace luisa {

class Integrator : public Node {

private:
    LUISA_MAKE_NODE_CREATOR_REGISTRY(Integrator);

};

}
