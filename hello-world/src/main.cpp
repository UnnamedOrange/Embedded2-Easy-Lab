#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <thread>

#include <fpioa.h>
#include <gpio.h>
#include <pwm.h>
#include <sysctl.h>
#include <timer.h>
#include <uart.h>

using namespace std::literals;
using namespace std::chrono;

class Main {
    using Self = Main;

private:
    static constexpr auto MY_TIMER_DEVICE = timer_device_number_t::TIMER_DEVICE_0;
    static constexpr auto MY_TIMER_CHANNEL = timer_channel_number_t::TIMER_CHANNEL_0;
    static constexpr auto PIN_LED = 10;
    static constexpr auto FUNC_LED = fpioa_function_t::FUNC_TIMER1_TOGGLE1;
    static constexpr auto MY_PWM_DEVICE = pwm_device_number_t::PWM_DEVICE_1;
    static constexpr auto MY_PWM_CHANNEL = pwm_channel_number_t::PWM_CHANNEL_0;

    static constexpr auto MAX_STEP = 200;

private:
    int step{};

private:
    static int my_timer_callback(void* ctx) {
        auto& self = *reinterpret_cast<Self*>(ctx);
        self.update_step();
        self.update_led();
        return 0;
    }

private:
    void update_step() {
        step++;
        if (step >= MAX_STEP) {
            step = 0;
        }
    }
    void update_led() {
        pwm_set_frequency(MY_PWM_DEVICE, MY_PWM_CHANNEL, 200000, //
                          static_cast<double>(std::abs(step - MAX_STEP / 2)) / (MAX_STEP / 2));
    }

public:
    Main() {
        // 配置管脚。
        init_io();

        // 启动中断控制器。
        plic_init();
        sysctl_enable_irq();

        // 配置按键中断。

        // 配置时钟中断。
        timer_init(MY_TIMER_DEVICE);
        timer_set_interval(MY_TIMER_DEVICE, MY_TIMER_CHANNEL, duration_cast<nanoseconds>(10ms).count());
        timer_irq_register(MY_TIMER_DEVICE, MY_TIMER_CHANNEL,
                           false, // is_single_shot
                           1,     // priority
                           &Self::my_timer_callback,
                           this); // ctx

        // 配置串口发送完成中断。

        // 启动时钟。
        timer_set_enable(MY_TIMER_DEVICE, MY_TIMER_CHANNEL, true);

        // 消息循环。
        while (true) {
        }
    }

private:
    void init_io() {
        // 设置 LED。
        fpioa_set_function(PIN_LED, FUNC_LED);
        pwm_init(MY_PWM_DEVICE);
        pwm_set_enable(MY_PWM_DEVICE, MY_PWM_CHANNEL, true);
    }
};

int main() {
    std::make_unique<Main>();
}
