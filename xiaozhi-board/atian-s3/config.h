#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <hal/adc_types.h>

// Audio I2S - MAX98357A (output) + INMP441 (input), sharing BCK/WS
// Keep the I2S clock at the speech pipeline's native 16 kHz rate. This avoids
// allocating a resampling buffer for every 10 ms microphone frame.
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

#define AUDIO_I2S_GPIO_BCLK      GPIO_NUM_40  // Bit clock (shared)
#define AUDIO_I2S_GPIO_WS        GPIO_NUM_41  // Word select (shared)
#define AUDIO_I2S_GPIO_DOUT      GPIO_NUM_39  // MAX98357A DIN
#define AUDIO_I2S_GPIO_DIN       GPIO_NUM_38  // INMP441 DOUT

// Display - ILI9341 SPI
#define DISPLAY_BACKLIGHT_PIN      GPIO_NUM_21
#define DISPLAY_RST_PIN            GPIO_NUM_46
#define DISPLAY_SCK_PIN            GPIO_NUM_12
#define DISPLAY_DC_PIN             GPIO_NUM_9
#define DISPLAY_CS_PIN             GPIO_NUM_10
#define DISPLAY_MOSI_PIN           GPIO_NUM_11
#define DISPLAY_MISO_PIN           GPIO_NUM_13
#define DISPLAY_SPI_SCLK_HZ        (40 * 1000 * 1000)

#define LCD_SPI_HOST               SPI3_HOST

#define LCD_TYPE_ILI9341_SERIAL
#define DISPLAY_WIDTH              320
#define DISPLAY_HEIGHT             240
#define DISPLAY_MIRROR_X           true
#define DISPLAY_MIRROR_Y           true
#define DISPLAY_SWAP_XY            true
#define DISPLAY_INVERT_COLOR       false
#define DISPLAY_RGB_ORDER          LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X           0
#define DISPLAY_OFFSET_Y           0
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false
#define DISPLAY_SPI_MODE           0

// Buttons
#define BOOT_BUTTON_GPIO           GPIO_NUM_0
#define RADIO_BUTTON_GPIO          GPIO_NUM_47
#define MENU_BUTTON_GPIO           GPIO_NUM_3
#define JOYSTICK_SW_GPIO           GPIO_NUM_4
#define JOYSTICK_X_CHANNEL         ADC_CHANNEL_4  // GPIO5
#define JOYSTICK_Y_CHANNEL         ADC_CHANNEL_5  // GPIO6
#define BUILTIN_LED_GPIO           GPIO_NUM_NC

// SHT30 temperature and humidity sensor
#define SHT30_SDA_PIN              GPIO_NUM_17
#define SHT30_SCL_PIN              GPIO_NUM_18
#define SHT30_I2C_ADDRESS          0x44

#endif  // _BOARD_CONFIG_H_
