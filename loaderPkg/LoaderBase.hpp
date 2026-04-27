// loaderPkg/LoaderBase.hpp
#pragma once

#include <Uefi.h>

namespace ribon::loaderPkg {

template <typename Derived>
class LoaderBase {
public:
    constexpr LoaderBase() = default;

    constexpr bool probe(const void* file, UINTN size) const {
        return static_cast<const Derived*>(this)->probe(file, size);
    }

    template <typename ImageT>
    constexpr bool loadImage(const void* file, UINTN size, ImageT& out) {
        return static_cast<Derived*>(this)->loadImage(file, size, out);
    }

    constexpr auto probeResult() const {
        return static_cast<const Derived*>(this)->probeResult();
    }
};

} // namespace ribon::loaderPkg
