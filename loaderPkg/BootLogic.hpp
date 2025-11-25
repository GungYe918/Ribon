#pragma once
#include <Uefi.h>
#include "LoaderBase.hpp"

namespace ribon::loaderPkg::boot {

    /**
     * @brief BootEngine
     *
     * - 여러 Loader 타입(multiboot, fiasco, custom)을 모두 관리
     * - probe -> load -> entry 결정
     * - 최종적으로 kernel jump까지 실행
     * - UEFI 환경에서 안전하게 작동
     */
    class BootLogic {
    public:
        BootLogic() = default;

        template<typename LoaderT>
        constexpr void registerLoader(LoaderT* loader) {
            if (loaderCount_ < MAX_LOADERS) {
                loaders_[loaderCount_++] = reinterpret_cast<LoaderBase<void>*>(loader);
            }
        }

        // 내부적으로 kernel file 저장
        constexpr void setKernel(const void* file, UINTN size) {
            kernelFileBase_ = const_cast<void*>(file);
            kernelFileSize_ = size;
        }

        // probe + load + entry 추출
        bool boot() {
            if (!kernelFileBase_) return false;

            for (UINTN i = 0; i < loaderCount_; i++) {
                auto* L = loaders_[i];

                if (L->probe(kernelFileBase_, kernelFileSize_)) {

                    if (!L->load(kernelFileBase_, kernelFileSize_))
                        return false;

                    entry_ = L->entryPoint();
                    return true;
                }
            }

            return false;
        }

        // kernelEntry 주소 반환
        constexpr UINT64 entryPoint() const {
            return entry_;
        }

        /// 엔트리 주소로 실제 점프
        ///
        /// NOTE: UEFI -> 커널 점프에서는 ExitBootServices를 반드시 호출.
        ///       여기서는 호출을 분리하여 외부에서 먼저 수행하도록 맡긴다.
        ///
        void jumpToKernel() const {
            if (entry_ == 0)
                return;

            using KernelEntry = void (*)();
            KernelEntry func = reinterpret_cast<KernelEntry>(entry_);
            func();  // 커널 진입
        }



        
    private:
        static constexpr UINTN MAX_LOADERS = 16;

        void* kernelFileBase_ = nullptr;
        UINTN kernelFileSize_ = 0;

        LoaderBase<void>* loaders_[MAX_LOADERS];
        UINTN loaderCount_ = 0;

        UINT64 entry_ = 0;
    };  


} // namespace ribon::loaderPkg::boot