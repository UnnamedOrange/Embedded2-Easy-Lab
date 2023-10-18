#include <fpioa.h>
#include <gpiohs.h>
#include <sysctl.h>

#include "board_config.h"
#include "lcd.h"
#include "nt35310.h"

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
    lcd_clear(RED);
}
