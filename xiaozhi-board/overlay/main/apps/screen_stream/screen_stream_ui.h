#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <lvgl.h>

#include "screen_stream_receiver.h"

class Display;

namespace screen_stream {

class ScreenStreamUi {
public:
    explicit ScreenStreamUi(Display* display);

    void ShowGameMenu();
    void HideGameMenu();
    bool IsGameMenuVisible() const { return game_menu_visible_.load(); }
    void MoveGameMenu(int delta);
    int GameMenuSelection() const { return game_menu_selection_; }

    void ShowConnection(const std::string& ip, uint16_t port);
    void UpdateConnection(const StreamStats& stats, bool disconnected);
    void ShowError(const char* message);

private:
    void CreateGameMenu();
    void RefreshGameMenu();
    void CreateConnectionPage();

    Display* display_;
    lv_obj_t* game_panel_ = nullptr;
    lv_obj_t* game_cards_[2] = {};
    lv_obj_t* connection_panel_ = nullptr;
    lv_obj_t* connection_status_ = nullptr;
    lv_obj_t* connection_ip_ = nullptr;
    lv_obj_t* connection_stats_ = nullptr;
    std::atomic<bool> game_menu_visible_{false};
    int game_menu_selection_ = 0;
    std::string ip_;
    uint16_t port_ = 8888;
};

}  // namespace screen_stream
