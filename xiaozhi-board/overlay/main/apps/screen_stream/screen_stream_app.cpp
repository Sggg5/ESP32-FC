#include "screen_stream_app.h"

#include "display/lcd_display.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/idf_additions.h>
#include <new>
#include <wifi_manager.h>

namespace screen_stream {
namespace {
constexpr char kTag[] = "SCREEN_STREAM";
}

ScreenStreamApp::ScreenStreamApp(LcdDisplay* display)
    : display_(display), renderer_(display), receiver_(&renderer_), ui_(display) {}

ScreenStreamApp::~ScreenStreamApp() {
    Exit();
}

void ScreenStreamApp::ShowGameMenu() {
    ui_.ShowGameMenu();
}

void ScreenStreamApp::HideGameMenu() {
    ui_.HideGameMenu();
}

bool ScreenStreamApp::IsGameMenuVisible() const {
    return ui_.IsGameMenuVisible();
}

void ScreenStreamApp::MoveGameMenu(int delta) {
    ui_.MoveGameMenu(delta);
}

int ScreenStreamApp::GameMenuSelection() const {
    return ui_.GameMenuSelection();
}

bool ScreenStreamApp::RequestEnter(uint16_t port) {
    if (active_.load()) return false;
    bool expected = false;
    if (!enter_pending_.compare_exchange_strong(expected, true)) return false;
    struct EnterRequest {
        ScreenStreamApp* app;
        uint16_t port;
    };
    auto* request = new (std::nothrow) EnterRequest{this, port};
    if (!request || xTaskCreateWithCaps([](void* context) {
            auto* request = static_cast<EnterRequest*>(context);
            request->app->Enter(request->port);
            request->app->enter_pending_.store(false);
            delete request;
            vTaskDelete(nullptr);
        }, "stream_enter", 6144, request, 3, nullptr,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        delete request;
        enter_pending_.store(false);
        return false;
    }
    return true;
}

bool ScreenStreamApp::RequestExit() {
    bool expected = false;
    if (!active_.load() || !exit_pending_.compare_exchange_strong(expected, true)) return false;
    if (xTaskCreateWithCaps([](void* context) {
            auto* app = static_cast<ScreenStreamApp*>(context);
            app->Exit();
            app->exit_pending_.store(false);
            vTaskDelete(nullptr);
        }, "stream_exit", 6144, this, 3, nullptr,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        exit_pending_.store(false);
        return false;
    }
    return true;
}

bool ScreenStreamApp::Enter(uint16_t port) {
    if (active_.exchange(true)) return true;
    WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::PERFORMANCE);
    direct_display_.store(false);
    ui_.ShowConnection(WifiManager::GetInstance().GetIpAddress(), port);
    if (!renderer_.Start([this]() { return OnFirstFrame(); })) {
        active_.store(false);
        WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::LOW_POWER);
        ui_.ShowError("显示任务或DMA内存创建失败");
        return false;
    }
    if (!receiver_.Start(port)) {
        renderer_.Stop();
        active_.store(false);
        WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::LOW_POWER);
        ui_.ShowError("PSRAM不足或UDP任务创建失败");
        return false;
    }
    status_stopped_ = xSemaphoreCreateBinary();
    if (!status_stopped_ ||
        xTaskCreateWithCaps(StatusTaskEntry, "stream_status", 3072, this, 2, &status_task_,
                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        receiver_.Stop();
        renderer_.Stop();
        active_.store(false);
        WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::LOW_POWER);
        if (status_stopped_) vSemaphoreDelete(status_stopped_);
        status_stopped_ = nullptr;
        ui_.ShowError("状态任务创建失败");
        return false;
    }
    ESP_LOGI(kTag, "Stream mode entered: internal=%u psram=%u",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return true;
}

bool ScreenStreamApp::OnFirstFrame() {
    if (!active_.load()) return false;
    if (direct_display_.load()) return true;
    if (display_->PauseLvglRefresh() == ESP_OK) {
        direct_display_.store(true);
        ESP_LOGI(kTag, "LCD ownership transferred from LVGL to stream renderer");
        return true;
    } else {
        ESP_LOGE(kTag, "Failed to pause LVGL");
        return false;
    }
}

void ScreenStreamApp::Exit() {
    if (!active_.exchange(false)) return;
    receiver_.Stop();
    renderer_.Stop();
    if (status_task_) {
        if (status_stopped_) xSemaphoreTake(status_stopped_, pdMS_TO_TICKS(2500));
        status_task_ = nullptr;
    }
    if (status_stopped_) vSemaphoreDelete(status_stopped_);
    status_stopped_ = nullptr;

    ui_.ShowGameMenu();
    if (direct_display_.exchange(false)) {
        const esp_err_t result = display_->ResumeLvglRefresh();
        ESP_LOGI(kTag, "LVGL resumed: %s", esp_err_to_name(result));
    }
    WifiManager::GetInstance().SetPowerSaveLevel(WifiPowerSaveLevel::LOW_POWER);
    ESP_LOGI(kTag, "Stream mode exited: internal=%u psram=%u",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

void ScreenStreamApp::StatusTaskEntry(void* context) {
    static_cast<ScreenStreamApp*>(context)->StatusTask();
}

void ScreenStreamApp::StatusTask() {
    while (active_.load()) {
        const StreamStats stats = receiver_.Stats();
        const int64_t now = esp_timer_get_time();
        const bool disconnected = stats.last_receive_us > 0 &&
                                  now - stats.last_receive_us > 3000000;
        if (!direct_display_.load()) ui_.UpdateConnection(stats, disconnected);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (status_stopped_) xSemaphoreGive(status_stopped_);
    vTaskDelete(nullptr);
}

}  // namespace screen_stream
