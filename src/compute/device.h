//
// Created by Mike Smith on 2020/8/8.
//

#pragma once

#include <memory>

#include <core/context.h>
#include <compute/buffer.h>
#include <compute/kernel.h>
#include <compute/function.h>

namespace luisa::render {
class Geometry;
struct Acceleration;
}

namespace luisa::compute {

class Device : Noncopyable {

protected:
    Context *_context{nullptr};
    std::vector<std::unique_ptr<Buffer>> _buffers;
    
    [[nodiscard]] virtual std::unique_ptr<Buffer> _allocate_buffer(size_t size) = 0;
    [[nodiscard]] virtual std::unique_ptr<Kernel> _compile_kernel(const dsl::Function &function) = 0;

public:
    explicit Device(Context *context) noexcept : _context{context} {}
    virtual ~Device() noexcept = default;
    
    template<typename Def, std::enable_if_t<std::is_invocable_v<Def, dsl::Function &>, int> = 0>
    [[nodiscard]] std::unique_ptr<Kernel> compile_kernel(std::string name, Def &&def) {
        dsl::Function function{std::move(name)};
        def(function);
        return _compile_kernel(function);
    }
    
    template<typename T>
    [[nodiscard]] BufferView<T> create_buffer(size_t size) {
        return _buffers.emplace_back(_allocate_buffer(size * sizeof(T)))->view<T>();
    }
    
    [[nodiscard]] virtual std::unique_ptr<render::Acceleration> build_acceleration(render::Geometry &geometry) = 0;
    
    virtual void launch(const std::function<void(Dispatcher &)> &dispatch) = 0;
    virtual void synchronize() = 0;
    
    static std::unique_ptr<Device> create(Context *context, std::string_view name);
};

using DeviceCreator = Device *(Context *);

#define LUISA_EXPORT_DEVICE_CREATOR(DeviceClass)  \
    LUISA_DLL_EXPORT ::luisa::compute::Device *create(::luisa::Context *context) { return new DeviceClass{context}; }

}
