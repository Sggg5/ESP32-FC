#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/i2s_std.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEST_I2S_BCLK GPIO_NUM_40
#define TEST_I2S_WS   GPIO_NUM_41
#define TEST_I2S_DOUT GPIO_NUM_39

#define SAMPLE_RATE 32000
#define TONE_HZ     440
#define AMPLITUDE   12000
#define FRAMES      256

static const char *TAG = "i2s_audio_test";

static void fill_tone(int16_t *buffer, size_t frames, uint32_t *phase)
{
    for (size_t i = 0; i < frames; ++i)
    {
        float t = (float)(*phase) / SAMPLE_RATE;
        int16_t sample = (int16_t)(sinf(2.0f * (float)M_PI * TONE_HZ * t) * AMPLITUDE);
        buffer[i * 2 + 0] = sample;
        buffer[i * 2 + 1] = sample;
        *phase = (*phase + 1) % SAMPLE_RATE;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "MAX98357A I2S test starting");
    ESP_LOGI(TAG, "Pins: DIN=%d BCLK=%d LRC=%d", TEST_I2S_DOUT, TEST_I2S_BCLK, TEST_I2S_WS);

    i2s_chan_handle_t tx_chan = NULL;
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = 4;
    chan_cfg.dma_frame_num = FRAMES;

    ESP_LOGI(TAG, "Calling i2s_new_channel");
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));
    ESP_LOGI(TAG, "i2s_new_channel OK");

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = TEST_I2S_BCLK,
            .ws = TEST_I2S_WS,
            .dout = TEST_I2S_DOUT,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_LOGI(TAG, "Calling i2s_channel_init_std_mode");
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_LOGI(TAG, "Calling i2s_channel_enable");
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_LOGI(TAG, "I2S enabled, playing %d Hz tone", TONE_HZ);

    int16_t samples[FRAMES * 2];
    uint32_t phase = 0;

    while (true)
    {
        fill_tone(samples, FRAMES, &phase);
        size_t written = 0;
        esp_err_t err = i2s_channel_write(tx_chan, samples, sizeof(samples), &written, pdMS_TO_TICKS(1000));
        if (err != ESP_OK || written != sizeof(samples))
        {
            ESP_LOGW(TAG, "write err=%s written=%u/%u", esp_err_to_name(err), (unsigned)written, (unsigned)sizeof(samples));
        }
    }
}
