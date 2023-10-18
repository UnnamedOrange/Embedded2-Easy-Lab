#include <array>
#include <cstdint>

#include <fpioa.h>
#include <gpiohs.h>
#include <sysctl.h>

#include "board_config.h"
#include "lcd.h"
#include "nt35310.h"

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

static uint16_t g_lcd_gram[LCD_X_MAX * LCD_Y_MAX] __attribute__((aligned(128)));

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
    io_init();
    io_set_power();
    lcd_init();
    lcd_set_direction(DIR_YX_RLDU);

    while (true) {
        for (const auto color : colors) {
            lcd_clear(color);
        }
    }
}
