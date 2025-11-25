#pragma once
#include <Uefi.h>
#include <Ribon/EfiContext.hpp>

#include "LoaderBase.hpp"

namespace ribon::loaderPkg::boot {

    /**
     * @brief BootLogic
     *
     * - 여러 Loader 타입(multiboot, fiasco, custom)을 모두 관리
     * - probe -> load -> entry 결정
     * - 최종적으로 kernel jump까지 실행
     * - UEFI 환경에서 안전하게 작동
     */
    class BootLogic {
    public:
        BootLogic() = default;

        // 로더 등록
        // LoaderT는 반드시 LoaderBase<LoaderT>를 public 상속해야 한다.
        template<typename LoaderT>
        constexpr void registerLoader(LoaderT* loader) {
            if (!loader || loaderCount_ >= MAX_LOADERS) return;

            using Base = LoaderBase<LoaderT>;

            LoaderSlot& slot = slots_[loaderCount_++];

            // CRTP Base 포인터로 저장 (실제 객체는 Derived)
            slot.instance = static_cast<Base*>(loader);
            slot.ops      = makeOps<LoaderT>();
        }

        // 내부적으로 kernel file 저장
        constexpr void setKernel(const void* file, UINTN size) {
            kernelFileBase_ = const_cast<void*>(file);
            kernelFileSize_ = size;
        }

        // probe + load + entry 추출
        bool boot() {
            if (!kernelFileBase_ || loaderCount_ == 0)
                return false;

            for (UINTN i = 0; i < loaderCount_; ++i) {
                LoaderSlot& s = slots_[i];

                if (s.ops.probe(s.instance, kernelFileBase_, kernelFileSize_)) {

                    if (!s.ops.load(s.instance, kernelFileBase_, kernelFileSize_))
                        return false;

                    entry_       = s.ops.entryPoint(s.instance);
                    chosenIndex_ = i;
                    return true;
                }
            }

            return false;
        }

        // 선택된 로더 기반 ExitBootServices 호출
        bool exitBootServices() {
            if (chosenIndex_ >= loaderCount_) return false;
            LoaderSlot& s = slots_[chosenIndex_];
            return s.ops.exitBootServices(s.instance);
        }

        /// kernelEntry 주소 반환
        constexpr UINT64 entryPoint() const {
            return entry_;
        }

        // 엔트리 주소로 실제 점프
        //
        // NOTE: UEFI -> 커널 점프에서는 ExitBootServices를
        //       먼저 외부에서 호출해야 한다.
        //
        void jumpToKernel() const {
            using KernelEntry = void (*)();
            if (entry_ == 0) return;

            KernelEntry func = reinterpret_cast<KernelEntry>(entry_);
            func();  // 커널 진입 (반환 없음이 정상)
        }

    private:
        // 로더 하나에 대한 함수 포인터 테이블
        struct LoaderOps {
            bool   (*probe)(const void* self, const void* file, UINTN size);
            bool   (*load)(void* self, const void* file, UINTN size);
            UINT64 (*entryPoint)(const void* self);
            bool   (*exitBootServices)(void* self);
        };

        struct LoaderSlot {
            void*     instance = nullptr;
            LoaderOps ops{};
        };

        static constexpr UINTN MAX_LOADERS = 16;

        template<typename LoaderT>
        static constexpr LoaderOps makeOps() {
            using Base = LoaderBase<LoaderT>;
            return LoaderOps{
                // probe
                [](const void* self, const void* file, UINTN size) -> bool {
                    auto* base = static_cast<const Base*>(self);
                    return base->probe(file, size);
                },
                // load
                [](void* self, const void* file, UINTN size) -> bool {
                    auto* base = static_cast<Base*>(self);
                    return base->load(file, size);
                },
                // entryPoint
                [](const void* self) -> UINT64 {
                    auto* base = static_cast<const Base*>(self);
                    return base->entryPoint();
                },
                // exitBootServices
                [](void* self) -> bool {
                    auto* base = static_cast<Base*>(self);
                    return base->exitBootServices();
                }
            };
        }

        LoaderSlot slots_[MAX_LOADERS]{};
        UINTN      loaderCount_  = 0;
        UINTN      chosenIndex_  = MAX_LOADERS;

        void* kernelFileBase_ = nullptr;
        UINTN kernelFileSize_ = 0;

        UINT64 entry_ = 0;
    };

} // namespace ribon::loaderPkg::boot
