#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string_view>
#include <thread>

#include <fpioa.h>
#include <gpio.h>
#include <gpiohs.h>
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
    static constexpr auto FUNC_LED = fpioa_function_t::FUNC_TIMER1_TOGGLE2; // MY_PWM_DEVICE, MY_PWM_CHANNEL
    static constexpr auto PIN_BUTTON = 13;
    static constexpr auto FUNC_BUTTON = fpioa_function_t::FUNC_GPIOHS0;
    static constexpr auto GPIOHS_BUTTON =
        static_cast<int>(FUNC_BUTTON) - static_cast<int>(fpioa_function_t::FUNC_GPIOHS0);
    static constexpr auto MY_PWM_DEVICE = pwm_device_number_t::PWM_DEVICE_1;
    static constexpr auto MY_PWM_CHANNEL = pwm_channel_number_t::PWM_CHANNEL_1;
    static constexpr auto MY_UART_DEVICE = uart_device_number_t::UART_DEVICE_3;
    static constexpr auto MY_DMAC_CHANNEL = dmac_channel_number_t::DMAC_CHANNEL0;

    static constexpr auto MAX_STEP = 100;
    static constexpr size_t MAX_STRING_LENGTH = 32;
    static constexpr auto STRING_1 = std::string_view("Hello\n");
    static constexpr auto STRING_2 = std::string_view("Hallo\n");
    static constexpr auto STRING_3 = std::string_view("你好\n");
    static constexpr auto STRING_4 = std::string_view("こんにちは\n");
    static constexpr auto STRINGS = std::array{STRING_1, STRING_2, STRING_3, STRING_4};

private:
    int step{};
    size_t string_idx{};
    bool is_button_down{};
    std::array<uint32_t, MAX_STRING_LENGTH> tx_buf;

private:
    static int on_my_timer(void* ctx) {
        auto& self = *reinterpret_cast<Self*>(ctx);
        if (self.is_button_down) {
            self.update_step();
            self.update_led();
        }
        return 0;
    }
    static int on_button_change(void* ctx) {
        auto& self = *reinterpret_cast<Self*>(ctx);
        self.query_button();
        return 0;
    }
    static int on_uart_sent(void* ctx) {
        auto& self = *reinterpret_cast<Self*>(ctx);
        self.update_string_idx();
        self.send_string_dma_irq();
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
    void query_button() {
        is_button_down = !gpiohs_get_pin(GPIOHS_BUTTON);
    }
    void send_string_dma_irq() {
        const auto& s = STRINGS[string_idx];
        for (size_t i = 0; i < s.length(); i++) {
            tx_buf[i] = s[i];
        }
        const auto data = uart_data_t{
            .tx_channel = MY_DMAC_CHANNEL,
            .tx_buf = tx_buf.data(),
            .tx_len = s.length(),
            .transfer_mode = uart_interrupt_mode_t::UART_SEND,
        };
        auto irq = plic_interrupt_t{
            .callback = &Self::on_uart_sent,
            .ctx = this,
            .priority = 1,
        };
        uart_handle_data_dma(MY_UART_DEVICE, data, &irq);
    }
    void update_string_idx() {
        string_idx++;
        if (string_idx >= STRINGS.size()) {
            string_idx = 0;
        }
    }

public:
    Main() {
        // 配置管脚。
        init_io();

        // 启动中断控制器。
        plic_init();
        sysctl_enable_irq();

        // 配置按键中断。
        gpiohs_irq_register(GPIOHS_BUTTON, 1, &Self::on_button_change, this);

        // 配置时钟中断。
        timer_init(MY_TIMER_DEVICE);
        timer_set_interval(MY_TIMER_DEVICE, MY_TIMER_CHANNEL, duration_cast<nanoseconds>(10ms).count());
        timer_irq_register(MY_TIMER_DEVICE, MY_TIMER_CHANNEL,
                           false, // is_single_shot
                           1,     // priority
                           &Self::on_my_timer,
                           this); // ctx

        // 配置串口。
        uart_init(MY_UART_DEVICE);
        uart_configure(MY_UART_DEVICE, 115200,
                       uart_bitwidth_t::UART_BITWIDTH_8BIT, //
                       uart_stopbit_t::UART_STOP_1,         //
                       uart_parity_t::UART_PARITY_NONE);

        // 启动时钟。
        timer_set_enable(MY_TIMER_DEVICE, MY_TIMER_CHANNEL, true);

        // 配置串口发送完成中断（发送第一次）。
        send_string_dma_irq();

        // 死循环。
        while (true) {
        }
    }

private:
    void init_io() {
        // 设置按键。
        fpioa_set_function(PIN_BUTTON, FUNC_BUTTON);
        gpiohs_set_drive_mode(GPIOHS_BUTTON, GPIO_DM_INPUT_PULL_UP);
        gpiohs_set_pin_edge(GPIOHS_BUTTON, GPIO_PE_BOTH);

        // 设置 LED。
        fpioa_set_function(PIN_LED, FUNC_LED);
        pwm_init(MY_PWM_DEVICE);
        pwm_set_enable(MY_PWM_DEVICE, MY_PWM_CHANNEL, true);
    }
};

int main() {
    std::make_unique<Main>();
}
