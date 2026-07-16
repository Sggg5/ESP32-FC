#include "screen_stream_ui.h"

#include "display/display.h"

#include <cstdio>

LV_FONT_DECLARE(font_atian_ui_20_4);
LV_FONT_DECLARE(font_atian_ui_14_4);

namespace screen_stream {
namespace {
void StylePage(lv_obj_t* page) {
    lv_obj_set_size(page, 320, 240);
    lv_obj_align(page, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_bg_color(page, lv_color_hex(0x08182f), 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_OFF);
}
}

ScreenStreamUi::ScreenStreamUi(Display* display) : display_(display) {}

void ScreenStreamUi::CreateGameMenu() {
    game_panel_ = lv_obj_create(lv_screen_active());
    StylePage(game_panel_);

    lv_obj_t* title = lv_label_create(game_panel_);
    lv_label_set_text(title, "游戏机");
    lv_obj_set_style_text_font(title, &font_atian_ui_20_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf5f8ff), 0);
    lv_obj_set_pos(title, 14, 12);

    const char* names[] = {"游戏模拟器", "电脑串流"};
    const char* details[] = {"选择并运行本机游戏", "连接电脑并显示游戏画面"};
    const char* icons[] = {LV_SYMBOL_PLAY, LV_SYMBOL_WIFI};
    for (int i = 0; i < 2; ++i) {
        game_cards_[i] = lv_obj_create(game_panel_);
        lv_obj_set_size(game_cards_[i], 292, 73);
        lv_obj_set_pos(game_cards_[i], 14, 52 + i * 82);
        lv_obj_set_style_radius(game_cards_[i], 7, 0);
        lv_obj_set_style_bg_color(game_cards_[i], lv_color_hex(0x132b49), 0);
        lv_obj_set_style_bg_opa(game_cards_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(game_cards_[i], 0, 0);
        lv_obj_set_scrollbar_mode(game_cards_[i], LV_SCROLLBAR_MODE_OFF);

        lv_obj_t* icon = lv_label_create(game_cards_[i]);
        lv_label_set_text(icon, icons[i]);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(0x2fd5cf), 0);
        lv_obj_set_pos(icon, 15, 20);

        lv_obj_t* name = lv_label_create(game_cards_[i]);
        lv_label_set_text(name, names[i]);
        lv_obj_set_style_text_font(name, &font_atian_ui_20_4, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0xf4f7fc), 0);
        lv_obj_set_pos(name, 55, 9);

        lv_obj_t* detail = lv_label_create(game_cards_[i]);
        lv_label_set_text(detail, details[i]);
        lv_obj_set_style_text_font(detail, &font_atian_ui_14_4, 0);
        lv_obj_set_style_text_color(detail, lv_color_hex(0x94a9bf), 0);
        lv_obj_set_pos(detail, 55, 39);
    }
}

void ScreenStreamUi::RefreshGameMenu() {
    for (int i = 0; i < 2; ++i) {
        const bool selected = i == game_menu_selection_;
        lv_obj_set_style_border_width(game_cards_[i], selected ? 2 : 1, 0);
        lv_obj_set_style_border_color(game_cards_[i],
                                      lv_color_hex(selected ? 0x2fd5cf : 0x29445f), 0);
        lv_obj_set_style_bg_color(game_cards_[i],
                                  lv_color_hex(selected ? 0x183b55 : 0x132b49), 0);
    }
}

void ScreenStreamUi::ShowGameMenu() {
    DisplayLockGuard lock(display_);
    if (!game_panel_) CreateGameMenu();
    RefreshGameMenu();
    lv_obj_remove_flag(game_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(game_panel_);
    if (connection_panel_) lv_obj_add_flag(connection_panel_, LV_OBJ_FLAG_HIDDEN);
    game_menu_visible_.store(true);
}

void ScreenStreamUi::HideGameMenu() {
    DisplayLockGuard lock(display_);
    if (game_panel_) lv_obj_add_flag(game_panel_, LV_OBJ_FLAG_HIDDEN);
    game_menu_visible_.store(false);
}

void ScreenStreamUi::MoveGameMenu(int delta) {
    if (!game_menu_visible_.load()) return;
    DisplayLockGuard lock(display_);
    game_menu_selection_ = (game_menu_selection_ + (delta > 0 ? 1 : -1) + 2) % 2;
    RefreshGameMenu();
}

void ScreenStreamUi::CreateConnectionPage() {
    connection_panel_ = lv_obj_create(lv_screen_active());
    StylePage(connection_panel_);

    lv_obj_t* back = lv_label_create(connection_panel_);
    lv_label_set_text(back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back, lv_color_hex(0xf5f8ff), 0);
    lv_obj_set_pos(back, 14, 15);

    lv_obj_t* title = lv_label_create(connection_panel_);
    lv_label_set_text(title, "电脑串流");
    lv_obj_set_style_text_font(title, &font_atian_ui_20_4, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xf5f8ff), 0);
    lv_obj_set_pos(title, 45, 12);

    lv_obj_t* card = lv_obj_create(connection_panel_);
    lv_obj_set_size(card, 292, 157);
    lv_obj_set_pos(card, 14, 49);
    lv_obj_set_style_radius(card, 7, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x28506b), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x102c46), 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    connection_status_ = lv_label_create(card);
    lv_label_set_text(connection_status_, "等待电脑连接");
    lv_obj_set_style_text_font(connection_status_, &font_atian_ui_20_4, 0);
    lv_obj_set_style_text_color(connection_status_, lv_color_hex(0x35d0c8), 0);
    lv_obj_set_pos(connection_status_, 18, 16);

    connection_ip_ = lv_label_create(card);
    lv_obj_set_style_text_font(connection_ip_, &font_atian_ui_14_4, 0);
    lv_obj_set_style_text_color(connection_ip_, lv_color_hex(0xe5edf7), 0);
    lv_obj_set_pos(connection_ip_, 18, 56);

    connection_stats_ = lv_label_create(card);
    lv_obj_set_style_text_font(connection_stats_, &font_atian_ui_14_4, 0);
    lv_obj_set_style_text_color(connection_stats_, lv_color_hex(0x9bb0c5), 0);
    lv_obj_set_pos(connection_stats_, 18, 88);

    lv_obj_t* hint = lv_label_create(connection_panel_);
    lv_label_set_text(hint, "长按返回");
    lv_obj_set_style_text_font(hint, &font_atian_ui_14_4, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x8ca2b8), 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -10);
}

void ScreenStreamUi::ShowConnection(const std::string& ip, uint16_t port) {
    ip_ = ip.empty() ? "0.0.0.0" : ip;
    port_ = port;
    DisplayLockGuard lock(display_);
    if (!connection_panel_) CreateConnectionPage();
    char address[80];
    snprintf(address, sizeof(address), "IP：%s\nUDP 端口：%u", ip_.c_str(), port_);
    lv_label_set_text(connection_ip_, address);
    lv_label_set_text(connection_status_, "等待电脑连接");
    lv_label_set_text(connection_stats_, "FPS：0    完整帧：0\n残缺帧：0    无效包：0");
    if (game_panel_) lv_obj_add_flag(game_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(connection_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(connection_panel_);
    game_menu_visible_.store(false);
}

void ScreenStreamUi::UpdateConnection(const StreamStats& stats, bool disconnected) {
    if (!connection_panel_) return;
    DisplayLockGuard lock(display_);
    lv_label_set_text(connection_status_,
                      disconnected ? "电脑连接已断开" :
                      stats.complete_frames ? "正在接收" : "等待电脑连接");
    char text[96];
    snprintf(text, sizeof(text), "FPS：%u    完整帧：%u\n残缺帧：%u    无效包：%u",
             static_cast<unsigned>(stats.fps),
             static_cast<unsigned>(stats.complete_frames),
             static_cast<unsigned>(stats.dropped_frames),
             static_cast<unsigned>(stats.invalid_packets));
    lv_label_set_text(connection_stats_, text);
}

void ScreenStreamUi::ShowError(const char* message) {
    DisplayLockGuard lock(display_);
    if (!connection_panel_) CreateConnectionPage();
    lv_label_set_text(connection_status_, "启动失败");
    lv_label_set_text(connection_stats_, message);
    lv_obj_remove_flag(connection_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(connection_panel_);
}

}  // namespace screen_stream
