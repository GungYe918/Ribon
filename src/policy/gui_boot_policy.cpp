#include <Ribon/policy/GuiBootPolicy.hpp>

namespace ribon::policy {

ribon::boot::BootPolicyConfig DefaultGuiBootPolicy() {
    ribon::boot::BootPolicyConfig config{};
    config.kernel_path = "\\kernel\\kernel.elf";
    config.boot_cmdline = "ribon.mode=gui";
    config.ui_enabled = true;
    return config;
}

} // namespace ribon::policy
