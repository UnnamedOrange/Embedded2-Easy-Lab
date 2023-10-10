#include <chrono>
#include <cstdio>
#include <thread>

#include <fpioa.h>
#include <gpio.h>

using namespace std::literals;

int main() {
    fpioa_set_function(10, FUNC_GPIO3);

    gpio_init();
    gpio_set_drive_mode(3, GPIO_DM_OUTPUT);
    auto value = static_cast<bool>(GPIO_PV_HIGH);
    gpio_set_pin(3, static_cast<gpio_pin_value_t>(value));
    while (1) {
        std::this_thread::sleep_for(1s);
        value = !value;
        gpio_set_pin(3, static_cast<gpio_pin_value_t>(value));
    }
}
