// loaderPkg/LoaderBase.hpp
#pragma once

#include <Uefi.h>


namespace ribon::loaderPkg {

   /**
     * @brief CRTP 기반 Loader Base
     *
     * - compile-time 다형성
     * - Derived 타입이 반드시 다음 멤버함수를 구현 필요
     *
     *      bool   probe(const void* file, UINTN size) const;
     *      bool   load(const void* file, UINTN size);
     *      UINT64 entryPoint() const;
     */
    template <typename Derived, typename KernelParam = void>
    class LoaderBase {
    public:
        constexpr LoaderBase() = default;

        /** @brief 정젹 다형성 Derived::probe 호출 */
        constexpr bool probe(const void* file, UINTN size) const {
            return static_cast<const Derived*>(this)->probe(file, size);
        }

        /** @brief 정젹 다형성 Derived::load 호출 */
        constexpr bool load(const void* file, UINTN size) const {
            return static_cast<Derived*>(this)->load(file, size);
        }

        /** @brief 정젹 다형성 Derived::entryPoint 호출 */
        constexpr UINT64 entryPoint() const {
            return static_cast<const Derived*>(this)->entryPoint();
        }

        /** @brief ExitBootServices 구현 - 각 loader마다 별도 */
        bool exitBootServices() {
            return static_cast<Derived*>(this)->exitBootServices();
        }
    };

} // namespace ribon::loaderPkg