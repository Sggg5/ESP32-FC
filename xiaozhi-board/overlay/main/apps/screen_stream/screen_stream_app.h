#pragma once

#include <atomic>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "screen_stream_receiver.h"
#include "screen_stream_renderer.h"
#include "screen_stream_ui.h"

class LcdDisplay;

namespace screen_stream {

class ScreenStreamApp {
public:
    explicit ScreenStreamApp(LcdDisplay* display);
    ~ScreenStreamApp();

    void ShowGameMenu();
    void HideGameMenu();
    bool IsGameMenuVisible() const;
    void MoveGameMenu(int delta);
    int GameMenuSelection() const;
    bool RequestEnter(uint16_t port = 8888);
    bool RequestExit();
    bool Enter(uint16_t port = 8888);
    void Exit();
    bool IsActive() const { return active_.load(); }

private:
    static void StatusTaskEntry(void* context);
    void StatusTask();
    bool OnFirstFrame();

    LcdDisplay* display_;
    ScreenStreamRenderer renderer_;
    ScreenStreamReceiver receiver_;
    ScreenStreamUi ui_;
    std::atomic<bool> active_{false};
    std::atomic<bool> enter_pending_{false};
    std::atomic<bool> exit_pending_{false};
    std::atomic<bool> direct_display_{false};
    TaskHandle_t status_task_ = nullptr;
    SemaphoreHandle_t status_stopped_ = nullptr;
};

}  // namespace screen_stream
