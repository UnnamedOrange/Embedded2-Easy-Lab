#include <array>
#include <chrono>
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

#include <fpioa.h>
#include <gpiohs.h>
#include <sysctl.h>
#include <timer.h>

#include "board_config.h"
#include "lcd.h"
#include "nt35310.h"

using namespace std::literals;
using namespace std::chrono;

/**
 * @brief Convert HSV to RGB565.
 * https://blog.csdn.net/lly_3485390095/article/details/104570885
 *
 * @param h From 0 to 360.
 * @param s From 0 to 100.
 * @param v From 0 to 100.
 * @return uint16_t RGB565 to be directly used.
 */
static constexpr uint16_t hsv_to_rgb565(int h, int s, int v) {
    float R = 0, G = 0, B = 0;

    const auto S = float(s) / 100.0F;
    const auto V = float(v) / 100.0F;
    if (S == 0) {
        R = G = B = V;
    } else {
        const auto H = float(h) / 60.0F;
        const auto sector = static_cast<int>(H);
        const auto remainder = H - sector;

        const auto X = V * (1 - S);
        const auto Y = V * (1 - S * remainder);
        const auto Z = V * (1 - S * (1 - remainder));
        switch (sector) {
        case 0:
            R = V;
            G = Z;
            B = X;
            break;
        case 1:
            R = Y;
            G = V;
            B = X;
            break;
        case 2:
            R = X;
            G = V;
            B = Z;
            break;
        case 3:
            R = X;
            G = Y;
            B = V;
            break;
        case 4:
            R = Z;
            G = X;
            B = V;
            break;
        case 5:
            R = V;
            G = X;
            B = Y;
            break;
        default:
            // Unexpected.
            R = G = B = 0;
        }
    }
    const auto r = static_cast<uint16_t>(R * 255.0F);
    const auto g = static_cast<uint16_t>(G * 255.0F);
    const auto b = static_cast<uint16_t>(B * 255.0F);
    return ((b >> 3) << 0) | ((g >> 2) << 5) | ((r >> 3) << 11);
}
struct Colors {
    std::array<uint16_t, 360> value{};
    constexpr Colors() {
        for (size_t i = 0; i < value.size(); i++) {
            value[i] = hsv_to_rgb565(i, 100, 100);
        }
    }
};
inline static constexpr Colors _colors;
inline static const auto& colors = _colors.value;

static int counter = 0;

static constexpr auto MY_TIMER_DEVICE = timer_device_number_t::TIMER_DEVICE_0;
static constexpr auto MY_TIMER_CHANNEL = timer_channel_number_t::TIMER_CHANNEL_0;
int on_timer(void*) {
    counter++;
    return 0;
}

static std::array<uint16_t, LCD_X_MAX * LCD_Y_MAX> g_lcd_gram;
static void my_draw_string(const std::string& str, int x, int y, uint16_t font_color, uint16_t bg_color) {
    const int width = 8 * str.length();
    const int height = 16;
    std::vector<uint16_t> buf(width * height);
    lcd_ram_draw_string(const_cast<char*>(str.data()), reinterpret_cast<uint32_t*>(buf.data()), font_color, bg_color);

    for (int i = 0; i < width && x + i < LCD_Y_MAX; i++) {
        for (int j = 0; j < height && y + j < LCD_X_MAX; j++) {
            g_lcd_gram[(y + j) * LCD_Y_MAX + (x + i)] = buf[j * width + i];
        }
    }
}

static void io_set_power(void) {
    // Note: 可以设置电源域。
    sysctl_set_power_mode(SYSCTL_POWER_BANK6, SYSCTL_POWER_V18);
    sysctl_set_power_mode(SYSCTL_POWER_BANK7, SYSCTL_POWER_V18);
}

static void io_init(void) {
    fpioa_set_function(LCD_DC_PIN, FUNC_GPIOHS2); // LCD_DC_IO = 2
    fpioa_set_function(LCD_CS_PIN, FUNC_SPI0_SS3);
    fpioa_set_function(LCD_RW_PIN, FUNC_SPI0_SCLK);
    fpioa_set_function(LCD_RST_PIN, FUNC_GPIOHS0); // LCD_RST_IO = 0

    sysctl_set_spi0_dvp_data(1);

    // LCD Backlight
    fpioa_set_function(LCD_BLIGHT_PIN, FUNC_GPIOHS17); // LCD_BLIGHT_IO = 17
    gpiohs_set_drive_mode(LCD_BLIGHT_IO, GPIO_DM_OUTPUT);
    gpiohs_set_pin(LCD_BLIGHT_IO, GPIO_PV_LOW);
}

int main() {
    plic_init();
    sysctl_enable_irq();

    io_init();
    io_set_power();
    lcd_init();
    lcd_set_direction(DIR_YX_RLDU);

    timer_init(MY_TIMER_DEVICE);
    timer_set_interval(MY_TIMER_DEVICE, MY_TIMER_CHANNEL, duration_cast<nanoseconds>(1s).count());
    timer_irq_register(MY_TIMER_DEVICE, MY_TIMER_CHANNEL,
                       false, // is_single_shot
                       1,     // priority
                       on_timer,
                       nullptr); // ctx
    timer_set_enable(MY_TIMER_DEVICE, MY_TIMER_CHANNEL, true);

    using Clock = steady_clock;
    std::queue<Clock::time_point> frame_time_points;
    while (true) {
        for (const auto color : colors) {
            const auto now = Clock::now();
            frame_time_points.push(now);
            while (now - frame_time_points.front() > 1s) {
                frame_time_points.pop();
            }
            const auto fps = frame_time_points.size();
            std::fill(g_lcd_gram.begin(), g_lcd_gram.end(), color);

            char buf[8];
            std::sprintf(buf, "%u", static_cast<unsigned>(fps));
            my_draw_string(buf, 0, 0, BLACK, color);
            std::sprintf(buf, "%d", counter);
            my_draw_string(buf, 0, LCD_X_MAX - 16, WHITE, color);
            lcd_draw_picture(0, 0, LCD_Y_MAX, LCD_X_MAX, reinterpret_cast<uint32_t*>(g_lcd_gram.data()));
        }
    }
}
