#include <Ribon/Common.hpp>

namespace ribon {

    static CommonConfig gCommonConfig {};

    CommonConfig& GetCommonConfig() {
        return gCommonConfig;
    }

}