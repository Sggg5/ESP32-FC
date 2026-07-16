#include "screen_stream_renderer.h"

#include "screen_stream_protocol.h"
#include "display/lcd_display.h"

#include <algorithm>
#include <cstring>

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/idf_additions.h>

namespace screen_stream {
namespace {
constexpr char kTag[] = "SCREEN_STREAM";
constexpr int kFallbackStripLines = 8;
}

ScreenStreamRenderer::ScreenStreamRenderer(LcdDisplay* display) : display_(display) {}

ScreenStreamRenderer::~ScreenStreamRenderer() {
    Stop();
}

bool ScreenStreamRenderer::Start(std::function<bool()> first_frame_callback) {
    if (running_.exchange(true)) {
        return true;
    }
    first_frame_callback_ = std::move(first_frame_callback);
    first_frame_ = true;
    stopped_ = xSemaphoreCreateBinary();
    if (!stopped_ || xTaskCreateWithCaps(TaskEntry, "stream_draw", 4096, this, 4,
                                                        &task_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(kTag, "Renderer allocation failed");
        running_.store(false);
        if (stopped_) vSemaphoreDelete(stopped_);
        stopped_ = nullptr;
        return false;
    }
    return true;
}

void ScreenStreamRenderer::Stop() {
    if (!running_.exchange(false)) return;
    if (task_) xTaskNotifyGive(task_);
    if (stopped_) xSemaphoreTake(stopped_, pdMS_TO_TICKS(3000));
    task_ = nullptr;
    if (stopped_) vSemaphoreDelete(stopped_);
    stopped_ = nullptr;
    if (dma_strip_ && owns_dma_strip_) heap_caps_free(dma_strip_);
    dma_strip_ = nullptr;
    strip_lines_ = 0;
    owns_dma_strip_ = false;
    pending_frame_.store(nullptr);
    busy_.store(false);
}

bool ScreenStreamRenderer::TrySubmit(uint16_t* frame) {
    bool expected = false;
    if (!running_.load() || !busy_.compare_exchange_strong(expected, true)) return false;
    pending_frame_.store(frame);
    xTaskNotifyGive(task_);
    return true;
}

uint32_t ScreenStreamRenderer::AverageDrawUs() const {
    const uint32_t count = draw_count_.load();
    return count ? static_cast<uint32_t>(total_draw_us_.load() / count) : 0;
}

void ScreenStreamRenderer::TaskEntry(void* context) {
    static_cast<ScreenStreamRenderer*>(context)->TaskLoop();
}

void ScreenStreamRenderer::TaskLoop() {
    while (running_.load()) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (!running_.load()) break;
        uint16_t* frame = pending_frame_.exchange(nullptr);
        if (!frame) {
            busy_.store(false);
            continue;
        }
        if (first_frame_) {
            if (first_frame_callback_ && !first_frame_callback_()) {
                busy_.store(false);
                continue;
            }
            size_t draw_buffer_bytes = 0;
            dma_strip_ = static_cast<uint16_t*>(display_->GetLvglDrawBuffer(&draw_buffer_bytes));
            strip_lines_ = static_cast<int>(draw_buffer_bytes / (kWidth * sizeof(uint16_t)));
            if (!dma_strip_ || strip_lines_ < 1) {
                strip_lines_ = kFallbackStripLines;
                dma_strip_ = static_cast<uint16_t*>(heap_caps_malloc(
                    kWidth * strip_lines_ * sizeof(uint16_t),
                    MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
                owns_dma_strip_ = true;
            }
            if (!dma_strip_) {
                ESP_LOGE(kTag, "No DMA strip available");
                busy_.store(false);
                continue;
            }
            ESP_LOGI(kTag, "Using %d-line %s DMA strip", strip_lines_,
                     owns_dma_strip_ ? "private" : "LVGL");
            first_frame_ = false;
        }
        const int64_t started = esp_timer_get_time();
        bool ok = true;
        for (int y = 0; y < kHeight && running_.load(); y += strip_lines_) {
            const int lines = std::min(strip_lines_, kHeight - y);
            memcpy(dma_strip_, frame + y * kWidth, kWidth * lines * sizeof(uint16_t));
            if (display_->DrawBitmapBlocking(0, y, kWidth, y + lines, dma_strip_) != ESP_OK) {
                ok = false;
                break;
            }
        }
        const uint32_t elapsed = static_cast<uint32_t>(esp_timer_get_time() - started);
        if (ok) {
            total_draw_us_.fetch_add(elapsed);
            draw_count_.fetch_add(1);
            uint32_t maximum = max_draw_us_.load();
            while (elapsed > maximum && !max_draw_us_.compare_exchange_weak(maximum, elapsed)) {}
        }
        busy_.store(false);
    }
    if (stopped_) xSemaphoreGive(stopped_);
    vTaskDelete(nullptr);
}

}  // namespace screen_stream
