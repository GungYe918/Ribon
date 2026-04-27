#include <Ribon/policy/AutoBootPolicy.hpp>

namespace ribon::policy {

ribon::boot::BootPolicyConfig DefaultAutoBootPolicy() {
    ribon::boot::BootPolicyConfig config{};
    config.kernel_path = "\\kernel\\kernel.elf";
    config.boot_cmdline = "ribon.mode=autoboot";
    config.ui_enabled = false;
    return config;
}

} // namespace ribon::policy
