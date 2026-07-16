#pragma once

#include <atomic>
#include <cstdint>
#include <functional>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class LcdDisplay;

namespace screen_stream {

class ScreenStreamRenderer {
public:
    explicit ScreenStreamRenderer(LcdDisplay* display);
    ~ScreenStreamRenderer();

    bool Start(std::function<bool()> first_frame_callback);
    void Stop();
    bool TrySubmit(uint16_t* frame);
    bool Busy() const { return busy_.load(); }
    uint32_t AverageDrawUs() const;
    uint32_t MaxDrawUs() const { return max_draw_us_.load(); }

private:
    static void TaskEntry(void* context);
    void TaskLoop();

    LcdDisplay* display_;
    uint16_t* dma_strip_ = nullptr;
    int strip_lines_ = 0;
    bool owns_dma_strip_ = false;
    std::atomic<uint16_t*> pending_frame_{nullptr};
    std::atomic<bool> running_{false};
    std::atomic<bool> busy_{false};
    std::atomic<uint32_t> draw_count_{0};
    std::atomic<uint64_t> total_draw_us_{0};
    std::atomic<uint32_t> max_draw_us_{0};
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t stopped_ = nullptr;
    std::function<bool()> first_frame_callback_;
    bool first_frame_ = true;
};

}  // namespace screen_stream
