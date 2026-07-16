#include <esp_log.h>
#include <driver/spi_common.h>
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <driver/i2c_master.h>
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/ringbuf.h>
#include <freertos/task.h>
#include <esp_timer.h>
#include <esp_ota_ops.h>
#include <esp_http_client.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <esp_crt_bundle.h>
#include <freertos/idf_additions.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <cJSON.h>
#include <decoder/impl/esp_mp3_dec.h>
#include <simple_dec/esp_audio_simple_dec.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_ili9341.h>

#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "font_awesome.h"
#include "screen_stream_app.h"

#define TAG "AtianS3"

LV_FONT_DECLARE(font_awesome_30_4);
LV_FONT_DECLARE(font_awesome_16_4);
LV_FONT_DECLARE(font_atian_ui_20_4);
LV_FONT_DECLARE(font_atian_ui_14_4);
LV_IMAGE_DECLARE(cloud_sun);
LV_IMAGE_DECLARE(radio);
LV_IMAGE_DECLARE(bot);
LV_IMAGE_DECLARE(gamepad_2);
LV_IMAGE_DECLARE(home_weather);
LV_IMAGE_DECLARE(home_radio);
LV_IMAGE_DECLARE(home_bot);
LV_IMAGE_DECLARE(home_game);
LV_IMAGE_DECLARE(detail_weather);
LV_IMAGE_DECLARE(detail_radio);
LV_IMAGE_DECLARE(forecast_sun);
LV_IMAGE_DECLARE(forecast_cloud);
LV_IMAGE_DECLARE(forecast_rain);
LV_IMAGE_DECLARE(forecast_storm);
LV_IMAGE_DECLARE(sun);
LV_IMAGE_DECLARE(cloud);
LV_IMAGE_DECLARE(cloud_rain);
LV_IMAGE_DECLARE(cloud_lightning);
LV_IMAGE_DECLARE(snowflake);
LV_IMAGE_DECLARE(cloud_fog);

template <typename T>
class PsramAllocator {
public:
    using value_type = T;

    PsramAllocator() noexcept = default;

    template <typename U>
    PsramAllocator(const PsramAllocator<U>&) noexcept {}

    T* allocate(std::size_t count) {
        void* memory = heap_caps_malloc(count * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (memory == nullptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(memory);
    }

    void deallocate(T* memory, std::size_t) noexcept {
        heap_caps_free(memory);
    }

    template <typename U>
    bool operator==(const PsramAllocator<U>&) const noexcept { return true; }

    template <typename U>
    bool operator!=(const PsramAllocator<U>&) const noexcept { return false; }
};

template <typename T>
using PsramVector = std::vector<T, PsramAllocator<T>>;

using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

static void LogHeapState(const char* phase) {
    ESP_LOGI(TAG, "Heap %s: internal=%u, largest=%u, minimum=%u, psram=%u",
             phase,
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

class AtianWeatherDisplay : public SpiLcdDisplay {
private:
    lv_obj_t* weather_panel_ = nullptr;
    lv_obj_t* temperature_label_ = nullptr;
    lv_obj_t* humidity_label_ = nullptr;
    lv_obj_t* comfort_label_ = nullptr;
    lv_obj_t* outdoor_label_ = nullptr;
    lv_obj_t* weather_label_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* indoor_humidity_label_ = nullptr;
    lv_obj_t* aqi_label_ = nullptr;
    lv_obj_t* radio_panel_ = nullptr;
    lv_obj_t* radio_station_label_ = nullptr;
    lv_obj_t* radio_status_label_ = nullptr;
    lv_obj_t* radio_clock_label_ = nullptr;
    lv_obj_t* menu_panel_ = nullptr;
    lv_obj_t* menu_items_[4] = {};
    lv_obj_t* menu_icons_[4] = {};
    lv_obj_t* menu_detail_labels_[4] = {};
    lv_obj_t* menu_clock_label_ = nullptr;
    lv_obj_t* forecast_labels_[3] = {};
    lv_obj_t* forecast_icons_[3] = {};
    std::atomic<bool> menu_visible_ = false;
    int menu_selection_ = 0;
    float indoor_temperature_ = NAN;
    float indoor_humidity_ = NAN;
    int aqi_ = -1;
    bool has_forecast_ = false;
    bool bottom_module_dirty_ = true;

    struct ForecastDay {
        char condition[16] = "--";
        int low = 0;
        int high = 0;
    } forecast_[3];

    void RefreshBottomModule(time_t) {
        if (!bottom_module_dirty_)
            return;
        bottom_module_dirty_ = false;

        char indoor_text[40];
        char humidity_text[40];
        char air_text[40];
        if (std::isnan(indoor_temperature_)) {
            snprintf(indoor_text, sizeof(indoor_text), "室内\n--.-\xC2\xB0");
        } else {
            snprintf(indoor_text, sizeof(indoor_text), "室内\n%.1f\xC2\xB0", indoor_temperature_);
        }
        if (std::isnan(indoor_humidity_)) {
            snprintf(humidity_text, sizeof(humidity_text), "湿度\n--%%");
        } else {
            snprintf(humidity_text, sizeof(humidity_text), "湿度\n%.0f%%", indoor_humidity_);
        }
        if (aqi_ < 0) {
            snprintf(air_text, sizeof(air_text), "空气\n--");
        } else {
            const char* quality = aqi_ <= 50 ? "优" : aqi_ <= 100 ? "良" :
                                  aqi_ <= 150 ? "轻度" : aqi_ <= 200 ? "中度" : "较差";
            snprintf(air_text, sizeof(air_text), "空气\n%s %d", quality, aqi_);
        }
        lv_label_set_text(temperature_label_, indoor_text);
        lv_label_set_text(indoor_humidity_label_, humidity_text);
        lv_label_set_text(aqi_label_, air_text);

        static const char* day_names[] = {"今天", "明天", "后天"};
        for (int i = 0; i < 3; ++i) {
            if (!forecast_labels_[i])
                continue;
            char forecast_text[32];
            if (has_forecast_) {
                snprintf(forecast_text, sizeof(forecast_text), "%s\n%d/%d\xC2\xB0",
                         day_names[i], forecast_[i].low, forecast_[i].high);
            } else {
                snprintf(forecast_text, sizeof(forecast_text), "%s\n--/--\xC2\xB0", day_names[i]);
            }
            lv_label_set_text(forecast_labels_[i], forecast_text);
        }
    }

    const lv_image_dsc_t* ForecastIconFor(const char* condition) const {
        if (strstr(condition, "雷"))
            return &forecast_storm;
        if (strstr(condition, "雨") || strstr(condition, "雪"))
            return &forecast_rain;
        if (strstr(condition, "晴"))
            return &forecast_sun;
        return &forecast_cloud;
    }

    void CreateRadioPanel() {
        auto title_font = &font_atian_ui_20_4;
        auto detail_font = &font_atian_ui_14_4;
        const lv_color_t primary = lv_color_hex(0xf6f9ff);
        const lv_color_t ink = lv_color_hex(0x3f275f);
        const lv_color_t secondary = lv_color_hex(0x8657a2);
        const lv_color_t accent = lv_color_hex(0x9a62df);

        radio_panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(radio_panel_, 320, 240);
        lv_obj_align(radio_panel_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(radio_panel_, 0, 0);
        lv_obj_set_style_border_width(radio_panel_, 0, 0);
        lv_obj_set_style_pad_all(radio_panel_, 0, 0);
        lv_obj_set_style_bg_color(radio_panel_, lv_color_hex(0x0b2048), 0);
        lv_obj_set_style_bg_grad_color(radio_panel_, lv_color_hex(0x173f83), 0);
        lv_obj_set_style_bg_grad_dir(radio_panel_, LV_GRAD_DIR_VER, 0);
        lv_obj_set_scrollbar_mode(radio_panel_, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* back = lv_label_create(radio_panel_);
        lv_label_set_text(back, FONT_AWESOME_ARROW_LEFT);
        lv_obj_set_style_text_font(back, &font_awesome_16_4, 0);
        lv_obj_set_style_text_color(back, primary, 0);
        lv_obj_set_pos(back, 9, 8);

        lv_obj_t* title = lv_label_create(radio_panel_);
        lv_label_set_text(title, "收音机");
        lv_obj_set_style_text_font(title, title_font, 0);
        lv_obj_set_style_text_color(title, primary, 0);
        lv_obj_set_pos(title, 36, 5);

        lv_obj_t* wifi = lv_label_create(radio_panel_);
        lv_label_set_text(wifi, FONT_AWESOME_WIFI);
        lv_obj_set_style_text_font(wifi, &font_awesome_16_4, 0);
        lv_obj_set_style_text_color(wifi, lv_color_hex(0x50e6dd), 0);
        lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -79, 8);

        radio_clock_label_ = lv_label_create(radio_panel_);
        lv_label_set_text(radio_clock_label_, "--:--");
        lv_obj_set_width(radio_clock_label_, 65);
        lv_obj_set_style_text_font(radio_clock_label_, title_font, 0);
        lv_obj_set_style_text_color(radio_clock_label_, primary, 0);
        lv_obj_set_style_text_align(radio_clock_label_, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(radio_clock_label_, LV_ALIGN_TOP_RIGHT, -7, 5);

        lv_obj_t* info = lv_obj_create(radio_panel_);
        lv_obj_set_size(info, 308, 136);
        lv_obj_set_pos(info, 6, 36);
        lv_obj_set_style_radius(info, 8, 0);
        lv_obj_set_style_border_width(info, 1, 0);
        lv_obj_set_style_border_color(info, lv_color_hex(0xe3cfee), 0);
        lv_obj_set_style_bg_color(info, lv_color_hex(0xf8effb), 0);
        lv_obj_set_style_bg_opa(info, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(info, 0, 0);
        lv_obj_set_scrollbar_mode(info, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* radio_image = lv_image_create(info);
        lv_image_set_src(radio_image, &detail_radio);
        lv_obj_set_size(radio_image, 96, 96);
        lv_obj_set_pos(radio_image, 10, 18);

        lv_obj_t* live = lv_label_create(info);
        lv_label_set_text(live, "LIVE · 正在播放");
        lv_obj_set_style_text_font(live, detail_font, 0);
        lv_obj_set_style_text_color(live, accent, 0);
        lv_obj_set_pos(live, 116, 16);

        radio_station_label_ = lv_label_create(info);
        lv_label_set_text(radio_station_label_, "宁波经济广播");
        lv_obj_set_width(radio_station_label_, 184);
        lv_label_set_long_mode(radio_station_label_, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_font(radio_station_label_, title_font, 0);
        lv_obj_set_style_text_color(radio_station_label_, ink, 0);
        lv_obj_set_style_text_align(radio_station_label_, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_pos(radio_station_label_, 116, 44);

        radio_status_label_ = lv_label_create(info);
        lv_label_set_text(radio_status_label_, "正在连接  ·  按 0 停止");
        lv_obj_set_width(radio_status_label_, 184);
        lv_obj_set_style_text_font(radio_status_label_, detail_font, 0);
        lv_obj_set_style_text_color(radio_status_label_, secondary, 0);
        lv_obj_set_pos(radio_status_label_, 116, 74);

        static const int bar_heights[] = {8, 18, 12, 24, 10, 16, 22, 9, 19, 13};
        for (int i = 0; i < 10; ++i) {
            lv_obj_t* bar = lv_obj_create(info);
            lv_obj_set_size(bar, 4, bar_heights[i]);
            lv_obj_set_pos(bar, 118 + i * 16, 126 - bar_heights[i]);
            lv_obj_set_style_radius(bar, 2, 0);
            lv_obj_set_style_border_width(bar, 0, 0);
            lv_obj_set_style_bg_color(bar, lv_color_hex(0xc99df0), 0);
            lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        }

        static const char* controls[] = {"0  停止", "47  播放", "长按  切台"};
        for (int i = 0; i < 3; ++i) {
            lv_obj_t* control = lv_obj_create(radio_panel_);
            lv_obj_set_size(control, 96, 48);
            lv_obj_set_pos(control, 6 + i * 103, 178);
            lv_obj_set_style_radius(control, 8, 0);
            lv_obj_set_style_border_width(control, i == 1 ? 2 : 1, 0);
            lv_obj_set_style_border_color(control, lv_color_hex(i == 1 ? 0xb985ed : 0xd8c7e6), 0);
            lv_obj_set_style_bg_color(control, lv_color_hex(i == 1 ? 0xe8cff8 : 0xf8effb), 0);
            lv_obj_set_style_bg_opa(control, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_all(control, 0, 0);
            lv_obj_set_scrollbar_mode(control, LV_SCROLLBAR_MODE_OFF);

            lv_obj_t* label = lv_label_create(control);
            lv_label_set_text(label, controls[i]);
            lv_obj_set_width(label, 90);
            lv_obj_set_style_text_font(label, detail_font, 0);
            lv_obj_set_style_text_color(label, i == 1 ? accent : secondary, 0);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
        }
    }

    void CreateMainMenu() {
        auto title_font = &font_atian_ui_20_4;
        auto detail_font = &font_atian_ui_14_4;
        const lv_color_t background = lv_color_hex(0x0b2048);
        const lv_color_t primary = lv_color_hex(0xf6f9ff);
        const lv_color_t accent = lv_color_hex(0x50e6dd);
        menu_panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(menu_panel_, 320, 240);
        lv_obj_align(menu_panel_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(menu_panel_, 0, 0);
        lv_obj_set_style_border_width(menu_panel_, 0, 0);
        lv_obj_set_style_pad_all(menu_panel_, 0, 0);
        lv_obj_set_style_bg_color(menu_panel_, background, 0);
        lv_obj_set_style_bg_grad_color(menu_panel_, lv_color_hex(0x173f83), 0);
        lv_obj_set_style_bg_grad_dir(menu_panel_, LV_GRAD_DIR_VER, 0);
        lv_obj_set_scrollbar_mode(menu_panel_, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* brand = lv_label_create(menu_panel_);
        lv_label_set_text(brand, "XIAOZHI");
        lv_obj_set_style_text_font(brand, title_font, 0);
        lv_obj_set_style_text_color(brand, primary, 0);
        lv_obj_set_pos(brand, 8, 5);

        menu_clock_label_ = lv_label_create(menu_panel_);
        lv_label_set_text(menu_clock_label_, "--:--");
        lv_obj_set_width(menu_clock_label_, 65);
        lv_obj_set_style_text_font(menu_clock_label_, title_font, 0);
        lv_obj_set_style_text_color(menu_clock_label_, primary, 0);
        lv_obj_set_style_text_align(menu_clock_label_, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(menu_clock_label_, LV_ALIGN_TOP_RIGHT, -7, 5);

        lv_obj_t* wifi = lv_label_create(menu_panel_);
        lv_label_set_text(wifi, FONT_AWESOME_WIFI);
        lv_obj_set_style_text_font(wifi, &font_awesome_16_4, 0);
        lv_obj_set_style_text_color(wifi, accent, 0);
        lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -79, 8);

        lv_obj_t* divider = lv_obj_create(menu_panel_);
        lv_obj_set_size(divider, 304, 1);
        lv_obj_set_pos(divider, 8, 33);
        lv_obj_set_style_border_width(divider, 0, 0);
        lv_obj_set_style_bg_color(divider, lv_color_hex(0x2a568e), 0);
        lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(divider, 0, 0);

        static const char* names[] = {"天气", "收音机", "小智", "游戏机"};
        static const char* details[] = {
            "--.-\xC2\xB0 · 更新中", "10 个电台", "按下开始说话", "选择游戏"
        };
        static const lv_image_dsc_t* icons[] = {
            &home_weather, &home_radio, &home_bot, &home_game
        };
        static const uint32_t card_colors[] = {
            0xe9faf6, 0xf7effb, 0xeef6ff, 0xfff4df
        };
        static const uint32_t title_colors[] = {
            0x17335f, 0x3f275f, 0x17335f, 0x51331e
        };
        static const uint32_t detail_colors[] = {
            0x278f91, 0x8657a2, 0x4e78ad, 0xc2782c
        };
        for (int i = 0; i < 4; ++i) {
            menu_items_[i] = lv_obj_create(menu_panel_);
            lv_obj_set_size(menu_items_[i], 151, 94);
            lv_obj_set_pos(menu_items_[i], 6 + (i % 2) * 157, 38 + (i / 2) * 98);
            lv_obj_set_style_border_width(menu_items_[i], 1, 0);
            lv_obj_set_style_border_color(menu_items_[i], lv_color_hex(0xcbd8e8), 0);
            lv_obj_set_style_radius(menu_items_[i], 8, 0);
            lv_obj_set_style_pad_all(menu_items_[i], 0, 0);
            lv_obj_set_style_bg_color(menu_items_[i], lv_color_hex(card_colors[i]), 0);
            lv_obj_set_style_bg_opa(menu_items_[i], LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(menu_items_[i], 0, 0);
            lv_obj_set_scrollbar_mode(menu_items_[i], LV_SCROLLBAR_MODE_OFF);

            menu_icons_[i] = lv_image_create(menu_items_[i]);
            lv_image_set_src(menu_icons_[i], icons[i]);
            lv_obj_set_size(menu_icons_[i], 56, 56);
            lv_obj_set_pos(menu_icons_[i], 7, 19);

            lv_obj_t* label = lv_label_create(menu_items_[i]);
            lv_label_set_text(label, names[i]);
            lv_obj_set_width(label, 80);
            lv_obj_set_style_text_font(label, title_font, 0);
            lv_obj_set_style_text_color(label, lv_color_hex(title_colors[i]), 0);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_pos(label, 67, 20);

            menu_detail_labels_[i] = lv_label_create(menu_items_[i]);
            lv_label_set_text(menu_detail_labels_[i], details[i]);
            lv_obj_set_width(menu_detail_labels_[i], 80);
            lv_obj_set_style_text_font(menu_detail_labels_[i], detail_font, 0);
            lv_obj_set_style_text_color(menu_detail_labels_[i], lv_color_hex(detail_colors[i]), 0);
            lv_obj_set_style_text_align(menu_detail_labels_[i], LV_TEXT_ALIGN_LEFT, 0);
            lv_obj_set_pos(menu_detail_labels_[i], 67, 50);
        }
    }

    void RefreshMainMenu() {
        static const uint32_t card_colors[] = {
            0xe9faf6, 0xf7effb, 0xeef6ff, 0xfff4df
        };
        for (int i = 0; i < 4; ++i) {
            const bool selected = i == menu_selection_;
            lv_obj_set_style_bg_color(menu_items_[i], lv_color_hex(card_colors[i]), 0);
            lv_obj_set_style_border_width(menu_items_[i], selected ? 2 : 1, 0);
            lv_obj_set_style_border_color(menu_items_[i],
                lv_color_hex(selected ? 0x50e6dd : 0xcbd8e8), 0);
            lv_obj_set_style_shadow_width(menu_items_[i], selected ? 7 : 0, 0);
            lv_obj_set_style_shadow_color(menu_items_[i], lv_color_hex(0x50e6dd), 0);
            lv_obj_set_style_shadow_opa(menu_items_[i], selected ? LV_OPA_40 : LV_OPA_TRANSP, 0);
        }
    }

#if 0
    void CreateWeatherPanelLegacy() {
        auto* theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = theme->text_font()->font();

        weather_panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(weather_panel_, 320, 205);
        lv_obj_align(weather_panel_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(weather_panel_, 0, 0);
        lv_obj_set_style_border_width(weather_panel_, 0, 0);
        lv_obj_set_style_pad_all(weather_panel_, 0, 0);
        lv_obj_set_style_bg_color(weather_panel_, lv_color_hex(0xf4f7f8), 0);
        lv_obj_set_scrollbar_mode(weather_panel_, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* title = lv_label_create(weather_panel_);
        lv_label_set_text(title, "慈溪天气");
        lv_obj_set_style_text_font(title, text_font, 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0x52656d), 0);
        lv_obj_align(title, LV_ALIGN_TOP_LEFT, 18, 12);

        lv_obj_t* rule = lv_obj_create(weather_panel_);
        lv_obj_set_size(rule, 284, 2);
        lv_obj_align(rule, LV_ALIGN_TOP_MID, 0, 39);
        lv_obj_set_style_radius(rule, 0, 0);
        lv_obj_set_style_border_width(rule, 0, 0);
        lv_obj_set_style_bg_color(rule, lv_color_hex(0xcbd6da), 0);

        temperature_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(temperature_label_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(temperature_label_, lv_color_hex(0x17272d), 0);
        lv_label_set_text(temperature_label_, "--.-\xC2\xB0");
        lv_obj_align(temperature_label_, LV_ALIGN_LEFT_MID, 18, -12);

        lv_obj_t* indoor_title = lv_label_create(weather_panel_);
        lv_label_set_text(indoor_title, "室内");
        lv_obj_set_style_text_font(indoor_title, text_font, 0);
        lv_obj_set_style_text_color(indoor_title, lv_color_hex(0x52656d), 0);
        lv_obj_align(indoor_title, LV_ALIGN_TOP_LEFT, 20, 52);

        lv_obj_t* divider = lv_obj_create(weather_panel_);
        lv_obj_set_size(divider, 2, 82);
        lv_obj_align(divider, LV_ALIGN_CENTER, 30, -6);
        lv_obj_set_style_radius(divider, 0, 0);
        lv_obj_set_style_border_width(divider, 0, 0);
        lv_obj_set_style_bg_color(divider, lv_color_hex(0xcbd6da), 0);

        weather_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(weather_label_, "天气更新中");
        lv_obj_set_style_text_font(weather_label_, text_font, 0);
        lv_obj_set_style_text_color(weather_label_, lv_color_hex(0x52656d), 0);
        lv_obj_align(weather_label_, LV_ALIGN_TOP_RIGHT, -27, 52);

        outdoor_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(outdoor_label_, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(outdoor_label_, lv_color_hex(0x17272d), 0);
        lv_label_set_text(outdoor_label_, "--.-\xC2\xB0");
        lv_obj_align(outdoor_label_, LV_ALIGN_TOP_RIGHT, -28, 76);

        humidity_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(humidity_label_, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(humidity_label_, lv_color_hex(0x167d9a), 0);
        lv_label_set_text(humidity_label_, "--%");
        lv_obj_align(humidity_label_, LV_ALIGN_TOP_RIGHT, -37, 112);

        comfort_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(comfort_label_, text_font, 0);
        lv_obj_set_style_text_color(comfort_label_, lv_color_hex(0xffffff), 0);
        lv_obj_set_style_bg_color(comfort_label_, lv_color_hex(0x2f8f67), 0);
        lv_obj_set_style_bg_opa(comfort_label_, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(comfort_label_, 4, 0);
        lv_obj_set_style_pad_left(comfort_label_, 14, 0);
        lv_obj_set_style_pad_right(comfort_label_, 14, 0);
        lv_obj_set_style_pad_top(comfort_label_, 5, 0);
        lv_obj_set_style_pad_bottom(comfort_label_, 5, 0);
        lv_label_set_text(comfort_label_, "舒适");
        lv_obj_align(comfort_label_, LV_ALIGN_BOTTOM_MID, 0, -13);
    }

    void CreateWeatherPanel() {
        auto* theme = static_cast<LvglTheme*>(current_theme_);
        auto text_font = theme->text_font()->font();
        const lv_color_t ink = lv_color_hex(0x182326);
        const lv_color_t muted = lv_color_hex(0x657377);
        const lv_color_t rule = lv_color_hex(0xd8dfe1);
        const lv_color_t accent = lv_color_hex(0x19758a);

        weather_panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(weather_panel_, 320, 205);
        lv_obj_align(weather_panel_, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_radius(weather_panel_, 0, 0);
        lv_obj_set_style_border_width(weather_panel_, 0, 0);
        lv_obj_set_style_pad_all(weather_panel_, 0, 0);
        lv_obj_set_style_bg_color(weather_panel_, lv_color_hex(0xf8faf9), 0);
        lv_obj_set_scrollbar_mode(weather_panel_, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* city = lv_label_create(weather_panel_);
        lv_label_set_text(city, "慈溪");
        lv_obj_set_style_text_font(city, text_font, 0);
        lv_obj_set_style_text_color(city, ink, 0);
        lv_obj_align(city, LV_ALIGN_TOP_LEFT, 18, 12);

        weather_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(weather_label_, "天气更新中");
        lv_obj_set_style_text_font(weather_label_, text_font, 0);
        lv_obj_set_style_text_color(weather_label_, accent, 0);
        lv_obj_align(weather_label_, LV_ALIGN_TOP_RIGHT, -18, 12);

        lv_obj_t* outside_caption = lv_label_create(weather_panel_);
        lv_label_set_text(outside_caption, "室外温度");
        lv_obj_set_style_text_font(outside_caption, text_font, 0);
        lv_obj_set_style_text_color(outside_caption, muted, 0);
        lv_obj_align(outside_caption, LV_ALIGN_TOP_LEFT, 19, 46);

        outdoor_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(outdoor_label_, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(outdoor_label_, ink, 0);
        lv_label_set_text(outdoor_label_, "--.-\xC2\xB0");
        lv_obj_align(outdoor_label_, LV_ALIGN_TOP_LEFT, 16, 68);

        lv_obj_t* mark = lv_obj_create(weather_panel_);
        lv_obj_set_size(mark, 58, 58);
        lv_obj_align(mark, LV_ALIGN_TOP_RIGHT, -28, 59);
        lv_obj_set_style_radius(mark, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(mark, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(mark, 3, 0);
        lv_obj_set_style_border_color(mark, accent, 0);

        lv_obj_t* horizon = lv_obj_create(weather_panel_);
        lv_obj_set_size(horizon, 284, 1);
        lv_obj_align(horizon, LV_ALIGN_TOP_MID, 0, 137);
        lv_obj_set_style_radius(horizon, 0, 0);
        lv_obj_set_style_border_width(horizon, 0, 0);
        lv_obj_set_style_bg_color(horizon, rule, 0);

        const char* captions[] = {"室内", "湿度", "环境"};
        for (int i = 0; i < 3; ++i) {
            lv_obj_t* caption = lv_label_create(weather_panel_);
            lv_label_set_text(caption, captions[i]);
            lv_obj_set_style_text_font(caption, text_font, 0);
            lv_obj_set_style_text_color(caption, muted, 0);
            lv_obj_align(caption, LV_ALIGN_TOP_LEFT, 18 + i * 106, 146);
            if (i > 0) {
                lv_obj_t* separator = lv_obj_create(weather_panel_);
                lv_obj_set_size(separator, 1, 48);
                lv_obj_align(separator, LV_ALIGN_BOTTOM_LEFT, i * 106, -8);
                lv_obj_set_style_radius(separator, 0, 0);
                lv_obj_set_style_border_width(separator, 0, 0);
                lv_obj_set_style_bg_color(separator, rule, 0);
            }
        }

        temperature_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(temperature_label_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(temperature_label_, ink, 0);
        lv_label_set_text(temperature_label_, "--.-\xC2\xB0");
        lv_obj_align(temperature_label_, LV_ALIGN_BOTTOM_LEFT, 18, -10);

        humidity_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(humidity_label_, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(humidity_label_, ink, 0);
        lv_label_set_text(humidity_label_, "--%");
        lv_obj_align(humidity_label_, LV_ALIGN_BOTTOM_LEFT, 124, -10);

        comfort_label_ = lv_label_create(weather_panel_);
        lv_obj_set_style_text_font(comfort_label_, text_font, 0);
        lv_obj_set_style_text_color(comfort_label_, ink, 0);
        lv_label_set_text(comfort_label_, "--");
        lv_obj_align(comfort_label_, LV_ALIGN_BOTTOM_LEFT, 230, -12);
    }
#endif

    void CreateWeatherPanelLite() {
        auto title_font = &font_atian_ui_20_4;
        auto detail_font = &font_atian_ui_14_4;
        const lv_color_t primary = lv_color_hex(0xf6f9ff);
        const lv_color_t ink = lv_color_hex(0x17335f);
        const lv_color_t secondary = lv_color_hex(0x4e78ad);

        weather_panel_ = lv_obj_create(lv_screen_active());
        lv_obj_set_size(weather_panel_, 320, 240);
        lv_obj_align(weather_panel_, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(weather_panel_, 0, 0);
        lv_obj_set_style_border_width(weather_panel_, 0, 0);
        lv_obj_set_style_pad_all(weather_panel_, 0, 0);
        lv_obj_set_style_bg_color(weather_panel_, lv_color_hex(0x0b2048), 0);
        lv_obj_set_style_bg_grad_color(weather_panel_, lv_color_hex(0x173f83), 0);
        lv_obj_set_style_bg_grad_dir(weather_panel_, LV_GRAD_DIR_VER, 0);
        lv_obj_set_scrollbar_mode(weather_panel_, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* back = lv_label_create(weather_panel_);
        lv_label_set_text(back, FONT_AWESOME_ARROW_LEFT);
        lv_obj_set_style_text_font(back, &font_awesome_16_4, 0);
        lv_obj_set_style_text_color(back, primary, 0);
        lv_obj_set_pos(back, 9, 8);

        comfort_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(comfort_label_, "天气");
        lv_obj_set_style_text_font(comfort_label_, title_font, 0);
        lv_obj_set_style_text_color(comfort_label_, primary, 0);
        lv_obj_set_pos(comfort_label_, 36, 5);

        lv_obj_t* wifi = lv_label_create(weather_panel_);
        lv_label_set_text(wifi, FONT_AWESOME_WIFI);
        lv_obj_set_style_text_font(wifi, &font_awesome_16_4, 0);
        lv_obj_set_style_text_color(wifi, lv_color_hex(0x50e6dd), 0);
        lv_obj_align(wifi, LV_ALIGN_TOP_RIGHT, -79, 8);

        clock_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(clock_label_, "--:--");
        lv_obj_set_width(clock_label_, 65);
        lv_obj_set_style_text_font(clock_label_, title_font, 0);
        lv_obj_set_style_text_color(clock_label_, primary, 0);
        lv_obj_set_style_text_align(clock_label_, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(clock_label_, LV_ALIGN_TOP_RIGHT, -7, 5);

        lv_obj_t* hero = lv_obj_create(weather_panel_);
        lv_obj_set_size(hero, 308, 96);
        lv_obj_set_pos(hero, 6, 36);
        lv_obj_set_style_radius(hero, 8, 0);
        lv_obj_set_style_border_width(hero, 1, 0);
        lv_obj_set_style_border_color(hero, lv_color_hex(0xc9e8ee), 0);
        lv_obj_set_style_bg_color(hero, lv_color_hex(0xe9f7ff), 0);
        lv_obj_set_style_bg_opa(hero, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(hero, 0, 0);
        lv_obj_set_scrollbar_mode(hero, LV_SCROLLBAR_MODE_OFF);

        humidity_label_ = lv_image_create(hero);
        lv_image_set_src(humidity_label_, &detail_weather);
        lv_obj_set_size(humidity_label_, 96, 96);
        lv_obj_set_pos(humidity_label_, 8, 0);

        date_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(date_label_, "--月--日");
        lv_obj_set_width(date_label_, 188);
        lv_obj_set_style_text_font(date_label_, detail_font, 0);
        lv_obj_set_style_text_color(date_label_, secondary, 0);
        lv_obj_set_pos(date_label_, 120, 108);

        weather_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(weather_label_, "更新中");
        lv_obj_set_width(weather_label_, 188);
        lv_obj_set_style_text_font(weather_label_, title_font, 0);
        lv_obj_set_style_text_color(weather_label_, secondary, 0);
        lv_obj_set_pos(weather_label_, 120, 80);

        outdoor_label_ = lv_label_create(weather_panel_);
        lv_label_set_text(outdoor_label_, "--.-\xC2\xB0");
        lv_obj_set_style_text_font(outdoor_label_, &lv_font_montserrat_40, 0);
        lv_obj_set_style_text_color(outdoor_label_, ink, 0);
        lv_obj_set_style_text_letter_space(outdoor_label_, 0, 0);
        lv_obj_set_pos(outdoor_label_, 118, 39);

        lv_obj_t** metric_labels[] = {
            &temperature_label_, &indoor_humidity_label_, &aqi_label_
        };
        static const char* metric_text[] = {"室内\n--.-\xC2\xB0", "湿度\n--%", "空气\n--"};
        static const uint32_t metric_colors[] = {0xeaf6ff, 0xe9fbfa, 0xf0f9e9};
        for (int i = 0; i < 3; ++i) {
            lv_obj_t* metric = lv_obj_create(weather_panel_);
            lv_obj_set_size(metric, 97, 42);
            lv_obj_set_pos(metric, 6 + i * 103, 136);
            lv_obj_set_style_radius(metric, 8, 0);
            lv_obj_set_style_border_width(metric, 1, 0);
            lv_obj_set_style_border_color(metric, lv_color_hex(0xd3e3eb), 0);
            lv_obj_set_style_bg_color(metric, lv_color_hex(metric_colors[i]), 0);
            lv_obj_set_style_bg_opa(metric, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_all(metric, 0, 0);
            lv_obj_set_scrollbar_mode(metric, LV_SCROLLBAR_MODE_OFF);

            *metric_labels[i] = lv_label_create(metric);
            lv_label_set_text(*metric_labels[i], metric_text[i]);
            lv_obj_set_width(*metric_labels[i], 93);
            lv_obj_set_style_text_font(*metric_labels[i], detail_font, 0);
            lv_obj_set_style_text_color(*metric_labels[i], i == 2 ? lv_color_hex(0x55985d) : secondary, 0);
            lv_obj_set_style_text_align(*metric_labels[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(*metric_labels[i], LV_ALIGN_CENTER, 0, 0);
        }

        for (int i = 0; i < 3; ++i) {
            lv_obj_t* forecast_card = lv_obj_create(weather_panel_);
            lv_obj_set_size(forecast_card, 97, 53);
            lv_obj_set_pos(forecast_card, 6 + i * 103, 182);
            lv_obj_set_style_radius(forecast_card, 8, 0);
            lv_obj_set_style_border_width(forecast_card, i == 0 ? 2 : 1, 0);
            lv_obj_set_style_border_color(forecast_card, lv_color_hex(i == 0 ? 0x55d5d5 : 0xd4e0ee), 0);
            lv_obj_set_style_bg_color(forecast_card, lv_color_hex(0xf5f8ff), 0);
            lv_obj_set_style_bg_opa(forecast_card, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_all(forecast_card, 0, 0);
            lv_obj_set_scrollbar_mode(forecast_card, LV_SCROLLBAR_MODE_OFF);

            forecast_icons_[i] = lv_image_create(forecast_card);
            lv_image_set_src(forecast_icons_[i], &forecast_cloud);
            lv_obj_set_size(forecast_icons_[i], 32, 32);
            lv_obj_set_pos(forecast_icons_[i], 4, 10);

            forecast_labels_[i] = lv_label_create(forecast_card);
            lv_label_set_text(forecast_labels_[i], i == 0 ? "今天\n--/--\xC2\xB0" :
                                                   i == 1 ? "明天\n--/--\xC2\xB0" : "后天\n--/--\xC2\xB0");
            lv_obj_set_width(forecast_labels_[i], 58);
            lv_obj_set_style_text_font(forecast_labels_[i], detail_font, 0);
            lv_obj_set_style_text_color(forecast_labels_[i], ink, 0);
            lv_obj_set_style_text_align(forecast_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(forecast_labels_[i], 36, 7);
        }
    }

public:
    AtianWeatherDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                        int width, int height, int offset_x, int offset_y,
                        bool mirror_x, bool mirror_y, bool swap_xy)
        : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y,
                        mirror_x, mirror_y, swap_xy) {}

    void SetIndoorClimate(float temperature, float humidity) {
        if (!IsSetupUICalled())
            return;
        DisplayLockGuard lock(this);
        if (!weather_panel_)
            CreateWeatherPanelLite();
        indoor_temperature_ = temperature;
        indoor_humidity_ = humidity;
        bottom_module_dirty_ = true;
        RefreshBottomModule(time(nullptr));
    }

    void SetClock(time_t now) {
        if (!IsSetupUICalled())
            return;
        struct tm local_time = {};
        localtime_r(&now, &local_time);
        if (local_time.tm_year < 125)
            return;

        DisplayLockGuard lock(this);
        if (!weather_panel_)
            CreateWeatherPanelLite();
        static const char* weekdays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
        char clock_text[8];
        char date_text[32];
        snprintf(clock_text, sizeof(clock_text), "%02d:%02d", local_time.tm_hour, local_time.tm_min);
        snprintf(date_text, sizeof(date_text), "%02d月%02d日 %s",
                 local_time.tm_mon + 1, local_time.tm_mday, weekdays[local_time.tm_wday]);
        lv_label_set_text(clock_label_, clock_text);
        lv_label_set_text(date_label_, date_text);
        RefreshBottomModule(now);
    }

    void SetOutdoorWeather(float temperature, const char* condition, int aqi) {
        if (!IsSetupUICalled())
            return;
        DisplayLockGuard lock(this);
        if (!weather_panel_)
            CreateWeatherPanelLite();
        char temperature_text[20];
        snprintf(temperature_text, sizeof(temperature_text), "%.1f\xC2\xB0", temperature);
        lv_label_set_text(outdoor_label_, temperature_text);
        lv_label_set_text(weather_label_, condition);
        aqi_ = aqi;
        bottom_module_dirty_ = true;
        RefreshBottomModule(time(nullptr));

        if (menu_detail_labels_[0]) {
            char summary[48];
            snprintf(summary, sizeof(summary), "%.1f\xC2\xB0 · %s", temperature, condition);
            lv_label_set_text(menu_detail_labels_[0], summary);
        }
    }

    void SetForecast(const char* const conditions[3], const int lows[3], const int highs[3]) {
        if (!IsSetupUICalled())
            return;
        DisplayLockGuard lock(this);
        if (!weather_panel_)
            CreateWeatherPanelLite();
        for (int i = 0; i < 3; ++i) {
            snprintf(forecast_[i].condition, sizeof(forecast_[i].condition), "%s", conditions[i]);
            forecast_[i].low = lows[i];
            forecast_[i].high = highs[i];
            if (forecast_icons_[i])
                lv_image_set_src(forecast_icons_[i], ForecastIconFor(conditions[i]));
        }
        has_forecast_ = true;
        bottom_module_dirty_ = true;
        RefreshBottomModule(time(nullptr));
    }

    void HideIndoorClimate() {
        if (!weather_panel_)
            return;
        DisplayLockGuard lock(this);
        lv_obj_add_flag(weather_panel_, LV_OBJ_FLAG_HIDDEN);
    }

    void ShowRadio(const char* station, const char* status) {
        if (!IsSetupUICalled())
            return;
        DisplayLockGuard lock(this);
        if (!radio_panel_)
            CreateRadioPanel();
        if (top_bar_)
            lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        if (status_bar_)
            lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(radio_station_label_, station);
        lv_label_set_text(radio_status_label_, status);
        if (radio_clock_label_) {
            time_t now = time(nullptr);
            struct tm local_time = {};
            localtime_r(&now, &local_time);
            char clock_text[8];
            snprintf(clock_text, sizeof(clock_text), "%02d:%02d",
                     local_time.tm_hour, local_time.tm_min);
            lv_label_set_text(radio_clock_label_, clock_text);
        }
        if (menu_detail_labels_[1])
            lv_label_set_text(menu_detail_labels_[1], "正在播放");
        lv_obj_remove_flag(radio_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(radio_panel_);
    }

    void SetRadioStatus(const char* status) {
        if (!radio_panel_)
            return;
        DisplayLockGuard lock(this);
        lv_label_set_text(radio_status_label_, status);
    }

    void HideRadio() {
        if (!radio_panel_)
            return;
        DisplayLockGuard lock(this);
        lv_obj_add_flag(radio_panel_, LV_OBJ_FLAG_HIDDEN);
        if (menu_detail_labels_[1])
            lv_label_set_text(menu_detail_labels_[1], "未播放");
        if (weather_panel_) {
            lv_obj_remove_flag(weather_panel_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(weather_panel_);
        }
    }

    void ShowMainMenu() {
        if (!IsSetupUICalled())
            return;
        DisplayLockGuard lock(this);
        if (!menu_panel_)
            CreateMainMenu();
        if (top_bar_)
            lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        if (status_bar_)
            lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        struct tm local_time = {};
        const time_t now = time(nullptr);
        localtime_r(&now, &local_time);
        if (local_time.tm_year >= 125) {
            char clock[8];
            snprintf(clock, sizeof(clock), "%02d:%02d", local_time.tm_hour, local_time.tm_min);
            lv_label_set_text(menu_clock_label_, clock);
        }
        RefreshMainMenu();
        lv_obj_remove_flag(menu_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(menu_panel_);
        menu_visible_.store(true);
    }

    void HideMainMenu() {
        DisplayLockGuard lock(this);
        if (menu_panel_)
            lv_obj_add_flag(menu_panel_, LV_OBJ_FLAG_HIDDEN);
        menu_visible_.store(false);
    }

    bool IsMainMenuVisible() const {
        return menu_visible_.load();
    }

    void MoveMainMenu(int delta) {
        if (!menu_visible_.load())
            return;
        DisplayLockGuard lock(this);
        menu_selection_ = (menu_selection_ + delta + 4) % 4;
        RefreshMainMenu();
    }

    int MainMenuSelection() const {
        return menu_selection_;
    }

    void SetMainMenuClock(time_t now) {
        if (!menu_visible_.load() || !menu_clock_label_)
            return;
        struct tm local_time = {};
        localtime_r(&now, &local_time);
        if (local_time.tm_year < 125)
            return;
        char clock[8];
        snprintf(clock, sizeof(clock), "%02d:%02d", local_time.tm_hour, local_time.tm_min);
        DisplayLockGuard lock(this);
        lv_label_set_text(menu_clock_label_, clock);
    }

    void ShowWeather() {
        if (!IsSetupUICalled())
            return;
        DisplayLockGuard lock(this);
        if (!weather_panel_)
            CreateWeatherPanelLite();
        if (top_bar_)
            lv_obj_add_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        if (status_bar_)
            lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(weather_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(weather_panel_);
    }

    void ShowXiaozhiChrome() {
        DisplayLockGuard lock(this);
        if (top_bar_)
            lv_obj_remove_flag(top_bar_, LV_OBJ_FLAG_HIDDEN);
        if (status_bar_)
            lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_HIDDEN);
        if (container_)
            lv_obj_move_foreground(container_);
        if (top_bar_)
            lv_obj_move_foreground(top_bar_);
        if (status_bar_)
            lv_obj_move_foreground(status_bar_);
    }
};

class AtianS3NoAudioCodec : public NoAudioCodec {
private:
    std::mutex input_buffer_mutex_;
    PsramVector<int32_t> input_buffer_;
    PsramVector<int32_t> output_buffer_;
    bool input_buffer_logged_ = false;
    bool output_buffer_logged_ = false;

public:
    AtianS3NoAudioCodec(int input_sample_rate, int output_sample_rate,
                        gpio_num_t bclk, gpio_num_t ws,
                        gpio_num_t dout, gpio_num_t din) {
        duplex_ = true;
        input_sample_rate_ = input_sample_rate;
        output_sample_rate_ = output_sample_rate;

        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM_0,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,
            .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM,
            .auto_clear_after_cb = true,
            .auto_clear_before_cb = false,
            .intr_priority = 0,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = (uint32_t)output_sample_rate_,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = {
                // INMP441 uses a standard I2S stereo frame: 32 clock cycles
                // per channel. The MAX98357A accepts the same frame format.
                .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = I2S_STD_SLOT_BOTH,
                .ws_width = I2S_DATA_BIT_WIDTH_32BIT,
                .ws_pol = false,
                .bit_shift = true,
                #ifdef I2S_HW_VERSION_2
                    .left_align = false,
                    .big_endian = false,
                    .bit_order_lsb = false,
                #endif
            },
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = bclk,
                .ws = ws,
                .dout = dout,
                .din = din,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false
                }
            }
        };
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
        ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
        ESP_LOGI(TAG, "Duplex I2S channels created (MAX98357A + INMP441)");
    }

    void Start() override {
        NoAudioCodec::Start();
        // MAX98357A has no hardware volume control; keep the digital gain at a
        // conservative level and persist it for subsequent boots.
        SetOutputVolume(30);

    }

    void OutputRadioSamples(const int16_t* data, int samples) {
        if (!output_enabled_)
            EnableOutput(true);
        Write(data, samples);
    }

protected:
    int Write(const int16_t* data, int samples) override {
        std::lock_guard<std::mutex> lock(data_if_mutex_);
        output_buffer_.resize(samples * 2);
        if (!output_buffer_logged_) {
            ESP_LOGI(TAG, "Audio output buffer: %u bytes, PSRAM=%s",
                     output_buffer_.capacity() * sizeof(int32_t),
                     esp_ptr_external_ram(output_buffer_.data()) ? "yes" : "no");
            output_buffer_logged_ = true;
        }
        const int32_t volume_factor = static_cast<int32_t>(output_volume_ * 65536LL / 100);

        for (int i = 0; i < samples; ++i) {
            const int64_t scaled = static_cast<int64_t>(data[i]) * volume_factor;
            const int32_t sample = scaled > INT32_MAX ? INT32_MAX :
                                   scaled < INT32_MIN ? INT32_MIN : static_cast<int32_t>(scaled);
            // Duplicate the sample so MAX98357A works regardless of its L/R
            // selection pin, while preserving INMP441's required frame clock.
            output_buffer_[i * 2] = sample;
            output_buffer_[i * 2 + 1] = sample;
        }

        size_t bytes_written = 0;
        ESP_ERROR_CHECK(i2s_channel_write(tx_handle_, output_buffer_.data(), output_buffer_.size() * sizeof(int32_t),
                                          &bytes_written, portMAX_DELAY));
        return bytes_written / (2 * sizeof(int32_t));
    }

    int Read(int16_t* dest, int samples) override {
        std::lock_guard<std::mutex> lock(input_buffer_mutex_);
        input_buffer_.resize(samples * 2);
        if (!input_buffer_logged_) {
            ESP_LOGI(TAG, "Audio input buffer: %u bytes, PSRAM=%s",
                     input_buffer_.capacity() * sizeof(int32_t),
                     esp_ptr_external_ram(input_buffer_.data()) ? "yes" : "no");
            input_buffer_logged_ = true;
        }
        size_t bytes_read = 0;
        constexpr uint32_t kReadTimeoutMs = 200;
        if (i2s_channel_read(rx_handle_, input_buffer_.data(), input_buffer_.size() * sizeof(int32_t),
                             &bytes_read, kReadTimeoutMs) != ESP_OK) {
            return 0;
        }

        const int frames = bytes_read / (2 * sizeof(int32_t));
        int64_t left_energy = 0;
        int64_t right_energy = 0;
        for (int i = 0; i < frames; ++i) {
            left_energy += std::llabs(static_cast<int64_t>(input_buffer_[i * 2]));
            right_energy += std::llabs(static_cast<int64_t>(input_buffer_[i * 2 + 1]));
        }

        const int channel = right_energy > left_energy ? 1 : 0;
        for (int i = 0; i < frames; ++i) {
            const int32_t value = input_buffer_[i * 2 + channel] >> 12;
            dest[i] = value > INT16_MAX ? INT16_MAX : value < -INT16_MAX ? -INT16_MAX : static_cast<int16_t>(value);
        }
        return frames;
    }
};

class AtianS3Board : public WifiBoard {
private:
    Button boot_button_;
    Button radio_button_;
    Button menu_button_;
    Button joystick_button_;
    AtianWeatherDisplay* display_;
    std::unique_ptr<screen_stream::ScreenStreamApp> screen_stream_;
    adc_oneshot_unit_handle_t joystick_adc_ = nullptr;
    i2c_master_bus_handle_t sensor_i2c_bus_ = nullptr;
    i2c_master_dev_handle_t sht30_ = nullptr;
    TaskHandle_t radio_task_handle_ = nullptr;
    TaskHandle_t radio_playback_task_handle_ = nullptr;
    RingbufHandle_t radio_pcm_buffer_ = nullptr;
    std::atomic<bool> radio_enabled_ = false;
    std::atomic<bool> radio_playback_active_ = false;
    std::atomic<bool> radio_playback_drained_ = true;
    std::atomic<int> radio_station_ = 0;
    std::atomic<bool> radio_button_armed_ = false;
    std::atomic<bool> menu_shown_on_boot_ = false;

    static constexpr size_t kRadioPcmBufferBytes = 32768;
    static constexpr size_t kRadioPrebufferBytes = 16000;

    struct RadioStation {
        const char* name;
        const char* url;
    };

    static constexpr RadioStation kRadioStations[] = {
        {"宁波经济广播", "http://lhttp.qtfm.cn/live/1152/64k.mp3"},
        {"中国之声", "http://lhttp.qtfm.cn/live/15318317/64k.mp3"},
        {"清晨音乐台", "http://lhttp.qingting.fm/live/4915/64k.mp3"},
        {"上海动感101", "http://lhttp.qingting.fm/live/274/64k.mp3"},
        {"华语经典", "http://lhttp.qtfm.cn/live/5022308/64k.mp3"},
        {"动听音乐台", "http://lhttp-hw.qtfm.cn/live/5022107/64k.mp3"},
        {"上海经典金曲", "http://lhttp-hw.qtfm.cn/live/273/64k.mp3"},
        {"上海经典音乐", "http://lhttp-hw.qtfm.cn/live/267/64k.mp3"},
        {"北京新闻广播", "http://lhttp.qtfm.cn/live/339/64k.mp3"},
        {"小说评书广播", "http://lhttp.qtfm.cn/live/1951/64k.mp3"},
    };

    static uint8_t Sht30Crc(const uint8_t* data) {
        uint8_t crc = 0xff;
        for (int i = 0; i < 2; ++i) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit)
                crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
        }
        return crc;
    }

    static int ParseWeatherTemperature(cJSON* item) {
        if (!cJSON_IsString(item))
            return INT_MIN;
        const char* cursor = item->valuestring;
        while (*cursor && *cursor != '-' && (*cursor < '0' || *cursor > '9'))
            ++cursor;
        return *cursor ? static_cast<int>(strtol(cursor, nullptr, 10)) : INT_MIN;
    }

    bool ReadSht30(float& temperature, float& humidity) {
        const uint8_t command[] = {0x2c, 0x06};
        uint8_t data[6];
        if (i2c_master_transmit(sht30_, command, sizeof(command), 100) != ESP_OK)
            return false;
        vTaskDelay(pdMS_TO_TICKS(20));
        if (i2c_master_receive(sht30_, data, sizeof(data), 100) != ESP_OK)
            return false;
        if (Sht30Crc(data) != data[2] || Sht30Crc(data + 3) != data[5])
            return false;

        const uint16_t raw_temperature = (data[0] << 8) | data[1];
        const uint16_t raw_humidity = (data[3] << 8) | data[4];
        temperature = -45.0f + 175.0f * raw_temperature / 65535.0f;
        humidity = 100.0f * raw_humidity / 65535.0f;
        return true;
    }

    void DrainRadioPcmBuffer() {
        size_t bytes = 0;
        while (void* item = xRingbufferReceiveUpTo(radio_pcm_buffer_, &bytes, 0, 2048))
            vRingbufferReturnItem(radio_pcm_buffer_, item);
    }

    void RadioPlaybackTask() {
        auto* codec = static_cast<AtianS3NoAudioCodec*>(GetAudioCodec());
        while (true) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            if (!radio_playback_active_.load()) {
                DrainRadioPcmBuffer();
                radio_playback_drained_.store(true);
                continue;
            }

            // Build a jitter reserve before touching I2S. At 16 kHz mono,
            // 16,000 bytes is half a second of PCM.
            while (radio_playback_active_.load() &&
                   xRingbufferGetCurFreeSize(radio_pcm_buffer_) >
                       kRadioPcmBufferBytes - kRadioPrebufferBytes) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            ESP_LOGI(TAG, "Radio playback prebuffer ready");

            uint32_t underruns = 0;
            while (radio_playback_active_.load()) {
                size_t bytes = 0;
                void* item = xRingbufferReceiveUpTo(
                    radio_pcm_buffer_, &bytes, pdMS_TO_TICKS(80), 2048);
                if (!item) {
                    ++underruns;
                    ESP_LOGW(TAG, "Radio PCM underrun #%u, buffering", underruns);
                    while (radio_playback_active_.load() &&
                           xRingbufferGetCurFreeSize(radio_pcm_buffer_) >
                               kRadioPcmBufferBytes - kRadioPrebufferBytes) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    continue;
                }
                const size_t samples = (bytes & ~size_t{1}) / sizeof(int16_t);
                if (samples > 0)
                    codec->OutputRadioSamples(static_cast<const int16_t*>(item), samples);
                vRingbufferReturnItem(radio_pcm_buffer_, item);
            }
            DrainRadioPcmBuffer();
            radio_playback_drained_.store(true);
            ESP_LOGI(TAG, "Radio playback stopped, underruns=%u", underruns);
        }
    }

    void PrepareRadioPlayback() {
        radio_playback_active_.store(false);
        xTaskNotifyGive(radio_playback_task_handle_);
        for (int i = 0; i < 20 && !radio_playback_drained_.load(); ++i)
            vTaskDelay(pdMS_TO_TICKS(5));
        radio_playback_drained_.store(false);
        radio_playback_active_.store(true);
        xTaskNotifyGive(radio_playback_task_handle_);
    }

    void QueueRadioSamples(const int16_t* samples, size_t count) {
        if (!radio_playback_active_.load() || count == 0)
            return;
        if (xRingbufferSend(radio_pcm_buffer_, samples, count * sizeof(int16_t),
                            pdMS_TO_TICKS(100)) != pdTRUE) {
            ESP_LOGW(TAG, "Radio PCM buffer full, dropping %u samples", count);
        }
    }

    bool PlayRadioStream(int station_index) {
        const auto& station = kRadioStations[station_index];
        display_->ShowRadio(station.name, "正在连接  ·  按 0 停止");
        LogHeapState("before radio");

        esp_http_client_config_t http_config = {};
        http_config.url = station.url;
        http_config.timeout_ms = 5000;
        http_config.buffer_size = 4096;
        http_config.crt_bundle_attach = esp_crt_bundle_attach;
        http_config.keep_alive_enable = true;
        esp_http_client_handle_t client = esp_http_client_init(&http_config);
        if (!client) {
            display_->SetRadioStatus("连接失败  ·  按 0 返回");
            return false;
        }
        esp_http_client_set_header(client, "User-Agent", "ESP32-S3 Internet Radio/1.0");
        esp_http_client_set_header(client, "Icy-MetaData", "0");
        esp_err_t err = esp_http_client_open(client, 0);
        if (err == ESP_OK)
            esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (err != ESP_OK || status != 200) {
            ESP_LOGW(TAG, "Radio connection failed: %s, HTTP %d", esp_err_to_name(err), status);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            display_->SetRadioStatus("连接失败  ·  按 0 返回");
            return false;
        }

        esp_audio_simple_dec_cfg_t decoder_config = {};
        decoder_config.dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
        decoder_config.use_frame_dec = false;
        esp_audio_simple_dec_handle_t decoder = nullptr;
        const auto decoder_result = esp_audio_simple_dec_open(&decoder_config, &decoder);
        if (decoder_result != ESP_AUDIO_ERR_OK || decoder == nullptr) {
            ESP_LOGE(TAG, "Failed to open MP3 decoder: %d", decoder_result);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            display_->SetRadioStatus("解码器内存不足  ·  按 0 返回");
            return false;
        }

        constexpr size_t kInputSize = 4096;
        size_t output_size = 6144;
        auto* input = static_cast<uint8_t*>(heap_caps_malloc(kInputSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        auto* output = static_cast<uint8_t*>(heap_caps_malloc(output_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (!input || !output) {
            heap_caps_free(input);
            heap_caps_free(output);
            esp_audio_simple_dec_close(decoder);
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            display_->SetRadioStatus("播放缓冲区不足  ·  按 0 返回");
            return false;
        }

        PsramVector<int16_t> mono;
        PsramVector<int16_t> resampled;
        uint32_t resample_source_rate = 0;
        uint32_t resample_phase = 0;
        int64_t resample_sum = 0;
        uint32_t resample_count = 0;
        bool started = false;

        while (radio_enabled_.load() && radio_station_.load() == station_index) {
            if (started && Application::GetInstance().GetDeviceState() != kDeviceStateIdle) {
                radio_enabled_.store(false);
                break;
            }
            const int bytes_read = esp_http_client_read(client, reinterpret_cast<char*>(input), kInputSize);
            if (bytes_read <= 0) {
                ESP_LOGW(TAG, "Radio stream ended or timed out: %d", bytes_read);
                break;
            }

            esp_audio_simple_dec_raw_t raw = {};
            raw.buffer = input;
            raw.len = bytes_read;
            while (raw.len > 0 && radio_enabled_.load() && radio_station_.load() == station_index) {
                esp_audio_simple_dec_out_t frame = {};
                frame.buffer = output;
                frame.len = output_size;
                raw.consumed = 0;
                auto result = esp_audio_simple_dec_process(decoder, &raw, &frame);
                if (result == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                    auto* larger = static_cast<uint8_t*>(heap_caps_realloc(
                        output, frame.needed_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
                    if (!larger) {
                        ESP_LOGE(TAG, "Failed to grow radio PCM buffer to %u", frame.needed_size);
                        radio_enabled_.store(false);
                        break;
                    }
                    output = larger;
                    output_size = frame.needed_size;
                    continue;
                }
                if (result != ESP_AUDIO_ERR_OK) {
                    ESP_LOGW(TAG, "MP3 decode error: %d", result);
                    break;
                }
                if (raw.consumed > raw.len) {
                    ESP_LOGE(TAG, "MP3 decoder consumed invalid length: %u > %u",
                             raw.consumed, raw.len);
                    radio_enabled_.store(false);
                    break;
                }
                raw.buffer += raw.consumed;
                raw.len -= raw.consumed;
                if (raw.consumed == 0 && frame.decoded_size == 0) {
                    ESP_LOGW(TAG, "MP3 decoder made no progress");
                    break;
                }
                if (frame.decoded_size == 0)
                    continue;

                esp_audio_simple_dec_info_t info = {};
                if (esp_audio_simple_dec_get_info(decoder, &info) != ESP_AUDIO_ERR_OK ||
                    info.bits_per_sample != 16 || info.channel == 0) {
                    ESP_LOGW(TAG, "Unsupported radio PCM format");
                    radio_enabled_.store(false);
                    break;
                }
                // A voice command starts the radio while Xiaozhi is still
                // speaking its acknowledgement. Buffer and discard those
                // first frames, then begin playback as soon as it returns idle.
                if (!started && Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
                    continue;
                if (!started) {
                    int16_t minimum = INT16_MAX;
                    int16_t maximum = INT16_MIN;
                    int64_t absolute_sum = 0;
                    const auto* decoded = reinterpret_cast<const int16_t*>(output);
                    const size_t sample_count = frame.decoded_size / sizeof(int16_t);
                    for (size_t i = 0; i < sample_count; ++i) {
                        minimum = std::min(minimum, decoded[i]);
                        maximum = std::max(maximum, decoded[i]);
                        absolute_sum += std::abs(static_cast<int>(decoded[i]));
                    }
                    ESP_LOGI(TAG, "Radio started: %s, %u Hz, %u channels, %u bps",
                             station.name, info.sample_rate, info.channel, info.bitrate);
                    ESP_LOGI(TAG, "Radio PCM: %u bytes, min=%d, max=%d, avg_abs=%lld",
                             frame.decoded_size, minimum, maximum,
                             absolute_sum / std::max<size_t>(sample_count, 1));
                    display_->SetRadioStatus("正在播放  ·  按 0 停止");
                    LogHeapState("radio playing");
                    started = true;
                }

                const auto* pcm = reinterpret_cast<const int16_t*>(output);
                const size_t decoded_samples = frame.decoded_size / sizeof(int16_t);
                const size_t frames = decoded_samples / info.channel;
                mono.resize(frames);
                if (info.channel == 1) {
                    memcpy(mono.data(), pcm, frames * sizeof(int16_t));
                } else {
                    for (size_t i = 0; i < frames; ++i) {
                        int32_t mixed = 0;
                        for (int channel = 0; channel < info.channel; ++channel)
                            mixed += pcm[i * info.channel + channel];
                        mono[i] = static_cast<int16_t>(mixed / info.channel);
                    }
                }

                if (info.sample_rate == AUDIO_OUTPUT_SAMPLE_RATE) {
                    QueueRadioSamples(mono.data(), mono.size());
                    continue;
                }
                if (resample_source_rate != info.sample_rate) {
                    resample_source_rate = info.sample_rate;
                    resample_phase = 0;
                    resample_sum = 0;
                    resample_count = 0;
                }
                resampled.clear();
                resampled.reserve(mono.size() * AUDIO_OUTPUT_SAMPLE_RATE / info.sample_rate + 4);
                for (const int16_t sample : mono) {
                    resample_sum += sample;
                    ++resample_count;
                    resample_phase += AUDIO_OUTPUT_SAMPLE_RATE;
                    if (resample_phase >= info.sample_rate) {
                        resampled.push_back(static_cast<int16_t>(resample_sum / resample_count));
                        resample_phase -= info.sample_rate;
                        resample_sum = 0;
                        resample_count = 0;
                    }
                }
                if (!resampled.empty())
                    QueueRadioSamples(resampled.data(), resampled.size());
            }
        }

        heap_caps_free(input);
        heap_caps_free(output);
        esp_audio_simple_dec_close(decoder);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return started;
    }

    void RadioTask() {
        esp_mp3_dec_register();
        while (true) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            while (radio_enabled_.load()) {
                const int station = radio_station_.load();
                ESP_LOGI(TAG, "Radio request accepted: %s, device state %d",
                         kRadioStations[station].name,
                         static_cast<int>(Application::GetInstance().GetDeviceState()));
                PlayRadioStream(station);
                if (radio_enabled_.load() && radio_station_.load() == station)
                    vTaskDelay(pdMS_TO_TICKS(1500));
            }
            display_->HideRadio();
            Application::GetInstance().SetExternalAudioActive(false);
            LogHeapState("radio stopped");
        }
    }

    void InitializeRadio() {
        radio_pcm_buffer_ = xRingbufferCreateWithCaps(
            kRadioPcmBufferBytes, RINGBUF_TYPE_BYTEBUF,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        ESP_ERROR_CHECK(radio_pcm_buffer_ ? ESP_OK : ESP_ERR_NO_MEM);
        xTaskCreateWithCaps([](void* arg) {
            static_cast<AtianS3Board*>(arg)->RadioPlaybackTask();
            vTaskDeleteWithCaps(nullptr);
        }, "radio_playback", 4096, this, 5, &radio_playback_task_handle_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        xTaskCreateWithCaps([](void* arg) {
            static_cast<AtianS3Board*>(arg)->RadioTask();
            vTaskDeleteWithCaps(nullptr);
        }, "internet_radio", 16384, this, 3, &radio_task_handle_,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    void StartRadio(int station) {
        station %= static_cast<int>(sizeof(kRadioStations) / sizeof(kRadioStations[0]));
        Application::GetInstance().SetExternalAudioActive(true);
        PrepareRadioPlayback();
        radio_station_.store(station);
        radio_enabled_.store(true);
        display_->ShowRadio(kRadioStations[station].name, "正在连接  ·  按 0 停止");
        xTaskNotifyGive(radio_task_handle_);
    }

    void StopRadio() {
        radio_enabled_.store(false);
        radio_playback_active_.store(false);
        xTaskNotifyGive(radio_playback_task_handle_);
        Application::GetInstance().SetExternalAudioActive(false);
        display_->SetRadioStatus("正在停止…");
        xTaskNotifyGive(radio_task_handle_);
    }

#if 1
    bool FetchCixiOutdoorTemperature(float& value) {
        constexpr const char* url =
            "http://api.open-meteo.com/v1/forecast?latitude=30.17&longitude=121.27"
            "&current=temperature_2m&timezone=Asia%2FShanghai";
        PsramString body;
        esp_http_client_config_t config = {};
        config.url = url;
        config.timeout_ms = 15000;
        config.user_data = &body;
        config.event_handler = [](esp_http_client_event_t* event) -> esp_err_t {
            if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
                static_cast<PsramString*>(event->user_data)->append(
                    static_cast<const char*>(event->data), event->data_len);
            }
            return ESP_OK;
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client)
            return false;
        const esp_err_t result = esp_http_client_perform(client);
        const int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (result != ESP_OK || status != 200)
            return false;

        cJSON* root = cJSON_Parse(body.c_str());
        cJSON* current = root ? cJSON_GetObjectItem(root, "current") : nullptr;
        cJSON* temperature = current ? cJSON_GetObjectItem(current, "temperature_2m") : nullptr;
        const bool valid = cJSON_IsNumber(temperature) &&
                           temperature->valuedouble > -30 && temperature->valuedouble < 60;
        if (valid)
            value = static_cast<float>(temperature->valuedouble);
        cJSON_Delete(root);
        return valid;
    }

    bool UpdateCixiWeather() {
        constexpr const char* url = "http://t.weather.itboy.net/api/weather/city/101210403";
        LogHeapState("before weather");
        PsramString body;
        esp_http_client_config_t config = {};
        config.url = url;
        config.timeout_ms = 30000;
        config.user_data = &body;
        config.event_handler = [](esp_http_client_event_t* event) -> esp_err_t {
            if (event->event_id == HTTP_EVENT_ON_DATA && event->data_len > 0) {
                auto* response = static_cast<PsramString*>(event->user_data);
                response->append(static_cast<const char*>(event->data), event->data_len);
            }
            return ESP_OK;
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (!client) {
            ESP_LOGW(TAG, "Failed to create Cixi weather client");
            return false;
        }
        esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 ESP32 WeatherDisplay/1.0");
        const esp_err_t result = esp_http_client_perform(client);
        const int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        if (result != ESP_OK || status != 200) {
            ESP_LOGW(TAG, "Cixi weather request failed: %s, HTTP %d", esp_err_to_name(result), status);
            return false;
        }
        cJSON* root = cJSON_Parse(body.c_str());
        cJSON* data = root ? cJSON_GetObjectItem(root, "data") : nullptr;
        cJSON* temperature = data ? cJSON_GetObjectItem(data, "wendu") : nullptr;
        cJSON* forecast = data ? cJSON_GetObjectItem(data, "forecast") : nullptr;
        cJSON* today = cJSON_IsArray(forecast) ? cJSON_GetArrayItem(forecast, 0) : nullptr;
        cJSON* condition = today ? cJSON_GetObjectItem(today, "type") : nullptr;
        cJSON* aqi = today ? cJSON_GetObjectItem(today, "aqi") : nullptr;
        if (!cJSON_IsString(temperature) || !cJSON_IsString(condition)) {
            cJSON_Delete(root);
            ESP_LOGW(TAG, "Invalid Cixi weather response");
            return false;
        }
        float value = strtof(temperature->valuestring, nullptr);
        const float legacy_value = value;
        if (FetchCixiOutdoorTemperature(value)) {
            ESP_LOGI(TAG, "Outdoor temperature: Open-Meteo %.1f C (legacy source %.1f C)",
                     value, legacy_value);
        } else {
            ESP_LOGW(TAG, "Open-Meteo unavailable, using legacy temperature %.1f C", value);
        }
        const int aqi_value = cJSON_IsNumber(aqi) ? aqi->valueint : -1;
        const std::string weather_condition = condition->valuestring;
        const char* forecast_conditions[3] = {};
        int forecast_lows[3] = {};
        int forecast_highs[3] = {};
        bool forecast_valid = true;
        for (int i = 0; i < 3; ++i) {
            cJSON* day = cJSON_IsArray(forecast) ? cJSON_GetArrayItem(forecast, i) : nullptr;
            cJSON* day_condition = day ? cJSON_GetObjectItem(day, "type") : nullptr;
            const int low = ParseWeatherTemperature(day ? cJSON_GetObjectItem(day, "low") : nullptr);
            const int high = ParseWeatherTemperature(day ? cJSON_GetObjectItem(day, "high") : nullptr);
            if (!cJSON_IsString(day_condition) || low == INT_MIN || high == INT_MIN) {
                forecast_valid = false;
                break;
            }
            forecast_conditions[i] = day_condition->valuestring;
            forecast_lows[i] = low;
            forecast_highs[i] = high;
        }
        if (forecast_valid)
            display_->SetForecast(forecast_conditions, forecast_lows, forecast_highs);
        cJSON_Delete(root);
        display_->SetOutdoorWeather(value, weather_condition.c_str(), aqi_value);
        ESP_LOGI(TAG, "Cixi weather: %.1f C, %s, AQI %d", value, weather_condition.c_str(), aqi_value);
        LogHeapState("after weather");
        return true;
    }
#endif

    void InitializeSht30() {
        i2c_master_bus_config_t bus_config = {};
        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = SHT30_SDA_PIN;
        bus_config.scl_io_num = SHT30_SCL_PIN;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &sensor_i2c_bus_));

        i2c_device_config_t device_config = {};
        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = SHT30_I2C_ADDRESS;
        device_config.scl_speed_hz = 100000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(sensor_i2c_bus_, &device_config, &sht30_));

        xTaskCreateWithCaps([](void* arg) {
            auto* board = static_cast<AtianS3Board*>(arg);
            while (true) {
                float temperature = 0;
                float humidity = 0;
                if (board->ReadSht30(temperature, humidity)) {
                    ESP_LOGI(TAG, "SHT30: %.1f C, %.1f %%", temperature, humidity);
                    if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle &&
                        !board->radio_enabled_.load() &&
                        !board->display_->IsMainMenuVisible()) {
                        board->display_->SetIndoorClimate(temperature, humidity);
                        if (Application::GetInstance().HasServerTime())
                            board->display_->SetClock(time(nullptr));
                        if (!board->menu_shown_on_boot_.exchange(true))
                            board->display_->ShowMainMenu();
                    } else if (board->display_->IsMainMenuVisible() &&
                               Application::GetInstance().HasServerTime()) {
                        board->display_->SetMainMenuClock(time(nullptr));
                    }
                } else {
                    ESP_LOGW(TAG, "Failed to read SHT30 at 0x%02x", SHT30_I2C_ADDRESS);
                }
                for (int i = 0; i < 5; ++i) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle)
                        board->display_->HideIndoorClimate();
                }
            }
        }, "sht30", 4096, this, 2, nullptr, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        xTaskCreateWithCaps([](void* arg) {
            auto* board = static_cast<AtianS3Board*>(arg);
            vTaskDelay(pdMS_TO_TICKS(20000));
            while (true) {
                if (Application::GetInstance().GetDeviceState() == kDeviceStateIdle &&
                    !board->radio_enabled_.load())
                    board->UpdateCixiWeather();
                vTaskDelay(pdMS_TO_TICKS(30 * 60 * 1000));
            }
        }, "cixi_weather", 6144, this, 1, nullptr, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    }

    void SwitchToGameTarget(const char* target_label, const char* rom_path = nullptr) {
        const esp_partition_t* current = esp_ota_get_running_partition();
        const esp_partition_t* target = esp_partition_find_first(
            ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, target_label);
        if (!current || !target) {
            ESP_LOGE(TAG, "Game target partition %s not found", target_label);
            return;
        }

        nvs_handle_t handle;
        if (nvs_open("coexist", NVS_READWRITE, &handle) == ESP_OK) {
            nvs_set_str(handle, "xiaozhi_slot", current->label);
            if (rom_path)
                nvs_set_str(handle, "game_rom", rom_path);
            else
                nvs_erase_key(handle, "game_rom");
            nvs_commit(handle);
            nvs_close(handle);
        }
        ESP_LOGI(TAG, "Switching from %s to %s", current->label, target_label);
        ESP_ERROR_CHECK(esp_ota_set_boot_partition(target));
        esp_restart();
    }

    void SwitchToGameLauncher() {
        SwitchToGameTarget("launcher");
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_MOSI_PIN;
        buscfg.miso_io_num = DISPLAY_MISO_PIN;
        buscfg.sclk_io_num = DISPLAY_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_SPI_HOST, &io_config, &panel_io));

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new AtianWeatherDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        ESP_LOGI(TAG, "ILI9341 display initialized");
    }

    void RestoreCurrentView() {
        if (radio_enabled_.load()) {
            display_->ShowRadio(kRadioStations[radio_station_.load()].name,
                                "正在播放  ·  按 0 停止");
        } else {
            display_->ShowWeather();
        }
    }

    void ToggleMainMenu() {
        if (display_->IsMainMenuVisible()) {
            display_->HideMainMenu();
            RestoreCurrentView();
        } else {
            display_->ShowMainMenu();
        }
    }

    void ActivateMainMenuItem() {
        if (!display_->IsMainMenuVisible())
            return;
        const int selection = display_->MainMenuSelection();
        display_->HideMainMenu();
        switch (selection) {
            case 0:
                if (radio_enabled_.load())
                    StopRadio();
                display_->ShowWeather();
                break;
            case 1:
                StartRadio(radio_station_.load());
                break;
            case 2:
                if (radio_enabled_.load())
                    StopRadio();
                display_->ShowXiaozhiChrome();
                Application::GetInstance().Schedule([]() {
                    Application::GetInstance().ToggleChatState();
                });
                break;
            case 3:
                if (radio_enabled_.load())
                    StopRadio();
                screen_stream_->ShowGameMenu();
                break;
        }
    }

    void InitializeJoystickMenu() {
        adc_oneshot_unit_init_cfg_t unit_config = {};
        unit_config.unit_id = ADC_UNIT_1;
        ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &joystick_adc_));
        adc_oneshot_chan_cfg_t channel_config = {};
        channel_config.atten = ADC_ATTEN_DB_12;
        channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            joystick_adc_, JOYSTICK_X_CHANNEL, &channel_config));
        ESP_ERROR_CHECK(adc_oneshot_config_channel(
            joystick_adc_, JOYSTICK_Y_CHANNEL, &channel_config));

        xTaskCreateWithCaps([](void* arg) {
            auto* board = static_cast<AtianS3Board*>(arg);
            int last_direction = 0;
            while (true) {
                int x = 2048;
                int y = 2048;
                if ((board->display_->IsMainMenuVisible() ||
                     board->screen_stream_->IsGameMenuVisible()) &&
                    adc_oneshot_read(board->joystick_adc_, JOYSTICK_X_CHANNEL, &x) == ESP_OK &&
                    adc_oneshot_read(board->joystick_adc_, JOYSTICK_Y_CHANNEL, &y) == ESP_OK) {
                    int direction = 0;
                    if (x < 1100)
                        direction = -1;
                    else if (x > 2900)
                        direction = 1;
                    else if (y < 1100)
                        direction = -2;
                    else if (y > 2900)
                        direction = 2;
                    if (direction != 0 && last_direction == 0) {
                        if (board->screen_stream_->IsGameMenuVisible())
                            board->screen_stream_->MoveGameMenu(direction);
                        else
                            board->display_->MoveMainMenu(direction);
                    }
                    last_direction = direction;
                } else {
                    last_direction = 0;
                }
                vTaskDelay(pdMS_TO_TICKS(40));
            }
        }, "menu_joystick", 3072, this, 2, nullptr,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            if (screen_stream_->IsActive())
                return;
            if (screen_stream_->IsGameMenuVisible()) {
                screen_stream_->HideGameMenu();
                display_->ShowMainMenu();
                return;
            }
            if (display_->IsMainMenuVisible()) {
                display_->HideMainMenu();
                RestoreCurrentView();
                return;
            }
            if (radio_enabled_.load()) {
                StopRadio();
                return;
            }
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
        boot_button_.OnLongPress([this]() {
            if (screen_stream_->IsActive()) {
                screen_stream_->RequestExit();
                return;
            }
            if (screen_stream_->IsGameMenuVisible()) {
                screen_stream_->HideGameMenu();
                display_->ShowMainMenu();
                return;
            }
            SwitchToGameLauncher();
        });
        radio_button_.OnClick([this]() {
            if (screen_stream_->IsActive())
                return;
            if (!radio_button_armed_.load()) {
                ESP_LOGI(TAG, "Ignored radio click while startup input is guarded");
                return;
            }
            if (radio_enabled_.load())
                StopRadio();
            else
                StartRadio(radio_station_.load());
        });
        radio_button_.OnLongPress([this]() {
            if (screen_stream_->IsActive())
                return;
            if (!radio_button_armed_.load()) {
                ESP_LOGI(TAG, "Ignored radio long press while startup input is guarded");
                return;
            }
            StartRadio(radio_station_.load() + 1);
        });
        menu_button_.OnClick([this]() {
            if (screen_stream_->IsActive())
                return;
            if (screen_stream_->IsGameMenuVisible()) {
                screen_stream_->HideGameMenu();
                display_->ShowMainMenu();
                return;
            }
            ToggleMainMenu();
        });
        joystick_button_.OnClick([this]() {
            if (screen_stream_->IsActive())
                return;
            if (screen_stream_->IsGameMenuVisible()) {
                if (screen_stream_->GameMenuSelection() == 0)
                    SwitchToGameLauncher();
                else
                    screen_stream_->RequestEnter();
            } else if (display_->IsMainMenuVisible())
                ActivateMainMenuItem();
            else
                display_->ShowMainMenu();
        });

        xTaskCreate([](void* arg) {
            auto* board = static_cast<AtianS3Board*>(arg);
            int released_samples = 0;
            while (released_samples < 15) {
                if (gpio_get_level(RADIO_BUTTON_GPIO) != 0)
                    ++released_samples;
                else
                    released_samples = 0;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            board->radio_button_armed_.store(true);
            ESP_LOGI(TAG, "Radio button armed after stable release");
            vTaskDelete(nullptr);
        }, "radio_btn_guard", 2048, this, 3, nullptr);
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool(
            "self.radio.play",
            "播放网络收音机。当用户说播放收音机、打开收音机、听音乐时调用。",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    StartRadio(radio_station_.load());
                });
                return std::string("正在打开网络收音机");
            });
        mcp_server.AddTool(
            "self.radio.next",
            "切换到下一个网络电台。当用户说换台、下一个电台时调用。",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    StartRadio(radio_station_.load() + 1);
                });
                return std::string("正在切换电台");
            });
        mcp_server.AddTool(
            "self.radio.play_ningbo",
            "播放宁波经济广播。当用户说播放宁波电台、宁波广播、宁波经济广播时调用。",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    StartRadio(0);
                });
                return std::string("正在播放宁波经济广播");
            });
        mcp_server.AddTool(
            "self.radio.play_china_voice",
            "播放中国之声。当用户说播放中国之声、听中国之声时调用。",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    StartRadio(1);
                });
                return std::string("正在播放中国之声");
            });
        mcp_server.AddTool(
            "self.radio.stop",
            "停止网络收音机。当用户说停止收音机、关闭收音机、停止播放时调用。",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    StopRadio();
                });
                return std::string("已停止收音机");
            });
        mcp_server.AddTool(
            "self.game_launcher.open",
            "打开本机游戏菜单或模拟器。当用户说打开游戏、进入模拟器、玩游戏、玩魂斗罗、玩赤色要塞等意图时调用。",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    SwitchToGameLauncher();
                });
                return std::string("正在打开游戏菜单");
            });
        mcp_server.AddTool(
            "self.game_launcher.play_contra",
            "Start Contra 1. Call only when the user says 魂斗罗 or 魂斗罗1代. Do not call for 超级魂斗罗.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    SwitchToGameTarget("retro-core", "/sd/roms/nes/04_Contra_Spread.nes");
                });
                return std::string("正在启动魂斗罗");
            });
        mcp_server.AddTool(
            "self.game_launcher.play_super_contra",
            "Start Super Contra. Call only when the user explicitly says 超级魂斗罗.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    SwitchToGameTarget("retro-core", "/sd/roms/nes/01_Super_Contra_8.nes");
                });
                return std::string("正在启动超级魂斗罗");
            });
        mcp_server.AddTool(
            "self.game_launcher.play_jackal",
            "Start Jackal. Call when the user says 赤色要塞.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                Application::GetInstance().Schedule([this]() {
                    SwitchToGameTarget("retro-core", "/sd/roms/nes/02_Jackal_Unlimited.nes");
                });
                return std::string("正在启动赤色要塞");
            });
    }

public:
    AtianS3Board() : boot_button_(BOOT_BUTTON_GPIO), radio_button_(RADIO_BUTTON_GPIO),
                     menu_button_(MENU_BUTTON_GPIO), joystick_button_(JOYSTICK_SW_GPIO) {
        setenv("TZ", "CST-8", 1);
        tzset();
        InitializeSpi();
        InitializeLcdDisplay();
        screen_stream_ = std::make_unique<screen_stream::ScreenStreamApp>(display_);
        InitializeRadio();
        InitializeSht30();
        InitializeJoystickMenu();
        InitializeButtons();
        InitializeTools();
        GetBacklight()->SetBrightness(100);
        LogHeapState("board ready");
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static AtianS3NoAudioCodec audio_codec(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override { return display_; }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        // This board's ADC battery-divider wiring is not calibrated. Do not
        // report an unreliable level or show low-battery warnings.
        level = 100;
        charging = false;
        discharging = false;
        return false;
    }
};

DECLARE_BOARD(AtianS3Board);
