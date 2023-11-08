#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

#include <sysctl.h>
#include <wdt.h>

using namespace std::literals;

class Main {
    using Self = Main;

private:
    static constexpr auto WDT_ID = wdt_device_number_t::WDT_DEVICE_0;

private:
    static int irq_wdt0(void* ctx) {
        const auto result = wdt_init(Self::WDT_ID, 0, nullptr, nullptr);
        printf("Watchdog starved, reboot in %u ms\n", result);
        return 0;
    }

public:
    Main() {
        // 启动中断控制器。
        plic_init();
        sysctl_enable_irq();

        // 启动看门狗。
        {
            const auto result = wdt_init(Self::WDT_ID, 100, &Main::irq_wdt0, this);
            printf("WDT timeout: %u\n", result);
        }

        // 输出调试信息。
        printf("Start to handle something...\n");

        for (int i = 0; i < 100; i++) {
            wdt_feed(Self::WDT_ID);
            std::this_thread::sleep_for(50ms);
        }
    }
};

int main() {
    const auto _ = std::make_unique<Main>();
    printf("Stuck...\n");
    while (true)
        ;
}
