#include "rg_system.h"
#include "rg_audio.h"

#if RG_AUDIO_USE_INT_DAC || RG_AUDIO_USE_EXT_DAC

#ifndef ESP_PLATFORM
#error "I2S support can only be built inside esp-idf!"
#elif !CONFIG_IDF_TARGET_ESP32 && RG_AUDIO_USE_INT_DAC
#error "Your chip has no DAC! Please set RG_AUDIO_USE_INT_DAC to 0 in your target file."
#endif

#include <driver/gpio.h>
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif

#ifdef RG_GPIO_SND_AMP_ENABLE_INVERT
#define MUTE_ENABLE 1
#define MUTE_DISABLE 0
#else
#define MUTE_ENABLE 0
#define MUTE_DISABLE 1
#endif

// We can safely assume that no application will submit more than 640 audio frames per call to
// driver_submit (32000/50). Using a single large buffer risks blocking the call needlessly because
// some apps submit more than once per cycle or there could be occasional jitter (early submission).
#define DMA_BUFFER_COUNT 4
#define DMA_BUFFER_LEN 180

static struct {
    const char *last_error;
    int device;
    int volume;
    bool muted;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    i2s_chan_handle_t tx_chan;
    int sample_rate;
#endif
} state;

static bool driver_init(int device, int sample_rate)
{
    state.last_error = NULL;
    state.device = device;

    if (state.device == 0)
    {
    #if RG_AUDIO_USE_INT_DAC
        esp_err_t ret = i2s_driver_install(I2S_NUM_0, &(i2s_config_t){
            .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
            .sample_rate = sample_rate,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_MSB,
            .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1
            .dma_buf_count = DMA_BUFFER_COUNT,
            .dma_buf_len = DMA_BUFFER_LEN,
        }, 0, NULL);
        if (ret == ESP_OK)
            ret = i2s_set_dac_mode(RG_AUDIO_USE_INT_DAC);
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
    #else
        state.last_error = "This device does not support internal DAC mode!";
    #endif
    }
    else if (state.device == 1)
    {
    #if RG_AUDIO_USE_EXT_DAC
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
        RG_LOGI("I2S: creating TX channel, sample_rate=%d\n", sample_rate);
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        chan_cfg.dma_desc_num = DMA_BUFFER_COUNT;
        chan_cfg.dma_frame_num = 256;

        esp_err_t ret = i2s_new_channel(&chan_cfg, &state.tx_chan, NULL);
        RG_LOGI("I2S: new_channel returned %s\n", esp_err_to_name(ret));
        if (ret == ESP_OK)
        {
            i2s_std_config_t std_cfg = {
                .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
                .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
                .gpio_cfg = {
                    .mclk = I2S_GPIO_UNUSED,
                    .bclk = RG_GPIO_SND_I2S_BCK,
                    .ws = RG_GPIO_SND_I2S_WS,
                    .dout = RG_GPIO_SND_I2S_DATA,
                    .din = I2S_GPIO_UNUSED,
                    .invert_flags = {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
                },
            };
            RG_LOGI("I2S: init_std pins bclk=%d ws=%d dout=%d\n",
                RG_GPIO_SND_I2S_BCK, RG_GPIO_SND_I2S_WS, RG_GPIO_SND_I2S_DATA);
            ret = i2s_channel_init_std_mode(state.tx_chan, &std_cfg);
            RG_LOGI("I2S: init_std returned %s\n", esp_err_to_name(ret));
        }
        if (ret == ESP_OK)
        {
            RG_LOGI("I2S: enabling channel\n");
            ret = i2s_channel_enable(state.tx_chan);
            RG_LOGI("I2S: enable returned %s\n", esp_err_to_name(ret));
        }
        if (ret == ESP_OK)
            state.sample_rate = sample_rate;
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
#else
        esp_err_t ret = i2s_driver_install(I2S_NUM_0, &(i2s_config_t){
            .mode = I2S_MODE_MASTER | I2S_MODE_TX,
            .sample_rate = sample_rate,
            .bits_per_sample = 16,
            .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
            .communication_format = I2S_COMM_FORMAT_STAND_I2S,
            .intr_alloc_flags = 0, // ESP_INTR_FLAG_LEVEL1
            .dma_buf_count = DMA_BUFFER_COUNT,
            .dma_buf_len = DMA_BUFFER_LEN,
        #if CONFIG_IDF_TARGET_ESP32
            .use_apll = true, // External DAC may care about accuracy
        #endif
        }, 0, NULL);
        if (ret == ESP_OK)
        {
            ret = i2s_set_pin(I2S_NUM_0, &(i2s_pin_config_t) {
                .mck_io_num = GPIO_NUM_NC,
                .bck_io_num = RG_GPIO_SND_I2S_BCK,
                .ws_io_num = RG_GPIO_SND_I2S_WS,
                .data_out_num = RG_GPIO_SND_I2S_DATA,
                .data_in_num = GPIO_NUM_NC
            });
        }
        if (ret != ESP_OK)
            state.last_error = esp_err_to_name(ret);
#endif
    #else
        state.last_error = "This device does not support external DAC mode!";
    #endif
    }
    #ifdef RG_GPIO_SND_AMP_ENABLE
        gpio_reset_pin(RG_GPIO_SND_AMP_ENABLE);
        gpio_set_level(RG_GPIO_SND_AMP_ENABLE, MUTE_ENABLE);
        gpio_set_direction(RG_GPIO_SND_AMP_ENABLE, GPIO_MODE_OUTPUT);
    #endif
    return state.last_error == NULL;
}

static bool driver_set_sample_rates(int sampleRate)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (state.device == 1 && state.tx_chan)
    {
        if (i2s_channel_disable(state.tx_chan) != ESP_OK)
            return false;
        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
        if (i2s_channel_reconfig_std_clock(state.tx_chan, &clk_cfg) != ESP_OK)
            return false;
        if (i2s_channel_enable(state.tx_chan) != ESP_OK)
            return false;
        state.sample_rate = sampleRate;
        return true;
    }
#else
    return i2s_set_sample_rates(I2S_NUM_0, sampleRate) == ESP_OK;
#endif
    return false;
}

static bool driver_deinit(void)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
    if (state.device == 1 && state.tx_chan)
    {
        i2s_channel_disable(state.tx_chan);
        i2s_del_channel(state.tx_chan);
        state.tx_chan = NULL;
    }
#else
    i2s_driver_uninstall(I2S_NUM_0);
#endif
    if (state.device == 0)
    {
    #if RG_AUDIO_USE_INT_DAC
        i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);
    #endif
    }
    else if (state.device == 1)
    {
    #if RG_AUDIO_USE_EXT_DAC
        gpio_reset_pin(RG_GPIO_SND_I2S_BCK);
        gpio_reset_pin(RG_GPIO_SND_I2S_DATA);
        gpio_reset_pin(RG_GPIO_SND_I2S_WS);
    #endif
    }
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_reset_pin(RG_GPIO_SND_AMP_ENABLE);
    #endif
    return true;
}

static bool driver_submit(const rg_audio_frame_t *frames, size_t count)
{
    float volume = state.muted ? 0.f : (state.volume * 0.01f);
    bool use_internal_dac = state.device == 0;
    rg_audio_frame_t buffer[DMA_BUFFER_LEN];
    size_t pos = 0;

    for (size_t i = 0; i < count; ++i)
    {
        int left = frames[i].left * volume;
        int right = frames[i].right * volume;

        if (use_internal_dac)
        {
        #if RG_AUDIO_USE_INT_DAC == 1
            left = ((left + right) >> 1) + 0x8000; // the internal DAC expects unsigned data
            right = 0;
        #elif RG_AUDIO_USE_INT_DAC == 2
            left = 0;
            right = ((left + right) >> 1) + 0x8000; // the internal DAC expects unsigned data
        #elif RG_AUDIO_USE_INT_DAC == 3
            // In two channel mode we use left and right as a differential mono output to increase resolution.
            int sample = (left + right) >> 1;
            if (sample > 0x7F00)
            {
                left = 0x8000 + (sample - 0x7F00);
                right = -0x8000 + 0x7F00;
            }
            else if (sample < -0x7F00)
            {
                left = 0x8000 + (sample + 0x7F00);
                right = -0x8000 + -0x7F00;
            }
            else
            {
                left = 0x8000;
                right = -0x8000 + sample;
            }
        #endif
        }

        // Clipping   (not necessary, we have (int16 * vol) and volume is never more than 1.0)
        // if (left > 32767) left = 32767; else if (left < -32768) left = -32767;
        // if (right > 32767) right = 32767; else if (right < -32768) right = -32767;

        // Queue
        buffer[pos].left = left;
        buffer[pos].right = right;

        pos++;

        if (i == count - 1 || pos == RG_COUNT(buffer))
        {
            size_t written;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0)
            if (i2s_channel_write(state.tx_chan, (void *)buffer, pos * 4, &written, 20) != ESP_OK)
#else
            if (i2s_write(I2S_NUM_0, (void *)buffer, pos * 4, &written, 20) != ESP_OK)
#endif
                RG_LOGW("I2S Submission error! Written: %d/%d\n", written, pos * 4);
            pos = 0;
        }
    }
    return true;
}

static bool driver_set_mute(bool mute)
{
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
    i2s_zero_dma_buffer(I2S_NUM_0);
#endif
    #ifdef RG_GPIO_SND_AMP_ENABLE
    gpio_set_level(RG_GPIO_SND_AMP_ENABLE, mute ? MUTE_ENABLE : MUTE_DISABLE);
    #endif
    state.muted = mute;
    return true;
}

static bool driver_set_volume(int volume)
{
    state.volume = volume;
    return true;
}

static const char *driver_get_error(void)
{
    return state.last_error;
}

const rg_audio_driver_t rg_audio_driver_i2s = {
    .name = "i2s",
    .init = driver_init,
    .deinit = driver_deinit,
    .submit = driver_submit,
    .set_mute = driver_set_mute,
    .set_volume = driver_set_volume,
    .set_sample_rate = driver_set_sample_rates,
    .get_error = driver_get_error,
};

#endif // RG_AUDIO_USE_INT_DAC || RG_AUDIO_USE_EXT_DAC
