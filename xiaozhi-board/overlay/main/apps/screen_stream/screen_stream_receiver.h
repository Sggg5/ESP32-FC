#pragma once

#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace screen_stream {

class ScreenStreamRenderer;

struct StreamStats {
    uint32_t packets = 0;
    uint32_t complete_frames = 0;
    uint32_t dropped_frames = 0;
    uint32_t invalid_packets = 0;
    uint32_t fps = 0;
    int64_t last_receive_us = 0;
};

class ScreenStreamReceiver {
public:
    explicit ScreenStreamReceiver(ScreenStreamRenderer* renderer);
    ~ScreenStreamReceiver();
    bool Start(uint16_t port);
    void Stop();
    StreamStats Stats() const;

private:
    static void TaskEntry(void* context);
    void TaskLoop();
    bool AllocateFrames();
    void ReleaseFrames();
    void ResetAssembly(uint16_t frame_id, int64_t now);

    ScreenStreamRenderer* renderer_;
    uint16_t* frames_[2] = {};
    int receive_index_ = 0;
    uint16_t active_frame_id_ = 0;
    uint64_t chunk_map_ = 0;
    int64_t frame_started_us_ = 0;
    bool assembling_ = false;
    uint16_t last_completed_frame_id_ = 0;
    bool has_completed_frame_ = false;
    uint16_t port_ = 8888;
    std::atomic<int> socket_{-1};
    std::atomic<bool> running_{false};
    TaskHandle_t task_ = nullptr;
    SemaphoreHandle_t stopped_ = nullptr;
    std::atomic<uint32_t> packets_{0};
    std::atomic<uint32_t> complete_frames_{0};
    std::atomic<uint32_t> dropped_frames_{0};
    std::atomic<uint32_t> invalid_packets_{0};
    std::atomic<uint32_t> fps_{0};
    std::atomic<int64_t> last_receive_us_{0};
};

}  // namespace screen_stream
