#include "screen_stream_receiver.h"

#include "screen_stream_protocol.h"
#include "screen_stream_renderer.h"

#include <cerrno>
#include <cstring>

#include <arpa/inet.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/idf_additions.h>
#include <lwip/sockets.h>
#include <unistd.h>

namespace screen_stream {
namespace {
constexpr char kTag[] = "SCREEN_STREAM";
constexpr int64_t kFrameTimeoutUs = 100000;
constexpr uint64_t kCompleteMap = (1ULL << kChunksPerFrame) - 1;

static const uint16_t rgb332_to_rgb565_lut[256] = {
    0x0000, 0x0a00, 0x1500, 0x1f00, 0x2001, 0x2a01, 0x3501, 0x3f01,
    0x4002, 0x4a02, 0x5502, 0x5f02, 0x6003, 0x6a03, 0x7503, 0x7f03,
    0x8004, 0x8a04, 0x9504, 0x9f04, 0xa005, 0xaa05, 0xb505, 0xbf05,
    0xc006, 0xca06, 0xd506, 0xdf06, 0xe007, 0xea07, 0xf507, 0xff07,
    0x0020, 0x0a20, 0x1520, 0x1f20, 0x2021, 0x2a21, 0x3521, 0x3f21,
    0x4022, 0x4a22, 0x5522, 0x5f22, 0x6023, 0x6a23, 0x7523, 0x7f23,
    0x8024, 0x8a24, 0x9524, 0x9f24, 0xa025, 0xaa25, 0xb525, 0xbf25,
    0xc026, 0xca26, 0xd526, 0xdf26, 0xe027, 0xea27, 0xf527, 0xff27,
    0x0048, 0x0a48, 0x1548, 0x1f48, 0x2049, 0x2a49, 0x3549, 0x3f49,
    0x404a, 0x4a4a, 0x554a, 0x5f4a, 0x604b, 0x6a4b, 0x754b, 0x7f4b,
    0x804c, 0x8a4c, 0x954c, 0x9f4c, 0xa04d, 0xaa4d, 0xb54d, 0xbf4d,
    0xc04e, 0xca4e, 0xd54e, 0xdf4e, 0xe04f, 0xea4f, 0xf54f, 0xff4f,
    0x0068, 0x0a68, 0x1568, 0x1f68, 0x2069, 0x2a69, 0x3569, 0x3f69,
    0x406a, 0x4a6a, 0x556a, 0x5f6a, 0x606b, 0x6a6b, 0x756b, 0x7f6b,
    0x806c, 0x8a6c, 0x956c, 0x9f6c, 0xa06d, 0xaa6d, 0xb56d, 0xbf6d,
    0xc06e, 0xca6e, 0xd56e, 0xdf6e, 0xe06f, 0xea6f, 0xf56f, 0xff6f,
    0x0090, 0x0a90, 0x1590, 0x1f90, 0x2091, 0x2a91, 0x3591, 0x3f91,
    0x4092, 0x4a92, 0x5592, 0x5f92, 0x6093, 0x6a93, 0x7593, 0x7f93,
    0x8094, 0x8a94, 0x9594, 0x9f94, 0xa095, 0xaa95, 0xb595, 0xbf95,
    0xc096, 0xca96, 0xd596, 0xdf96, 0xe097, 0xea97, 0xf597, 0xff97,
    0x00b0, 0x0ab0, 0x15b0, 0x1fb0, 0x20b1, 0x2ab1, 0x35b1, 0x3fb1,
    0x40b2, 0x4ab2, 0x55b2, 0x5fb2, 0x60b3, 0x6ab3, 0x75b3, 0x7fb3,
    0x80b4, 0x8ab4, 0x95b4, 0x9fb4, 0xa0b5, 0xaab5, 0xb5b5, 0xbfb5,
    0xc0b6, 0xcab6, 0xd5b6, 0xdfb6, 0xe0b7, 0xeab7, 0xf5b7, 0xffb7,
    0x00d8, 0x0ad8, 0x15d8, 0x1fd8, 0x20d9, 0x2ad9, 0x35d9, 0x3fd9,
    0x40da, 0x4ada, 0x55da, 0x5fda, 0x60db, 0x6adb, 0x75db, 0x7fdb,
    0x80dc, 0x8adc, 0x95dc, 0x9fdc, 0xa0dd, 0xaadd, 0xb5dd, 0xbfdd,
    0xc0de, 0xcade, 0xd5de, 0xdfde, 0xe0df, 0xeadf, 0xf5df, 0xffdf,
    0x00f8, 0x0af8, 0x15f8, 0x1ff8, 0x20f9, 0x2af9, 0x35f9, 0x3ff9,
    0x40fa, 0x4afa, 0x55fa, 0x5ffa, 0x60fb, 0x6afb, 0x75fb, 0x7ffb,
    0x80fc, 0x8afc, 0x95fc, 0x9ffc, 0xa0fd, 0xaafd, 0xb5fd, 0xbffd,
    0xc0fe, 0xcafe, 0xd5fe, 0xdffe, 0xe0ff, 0xeaff, 0xf5ff, 0xffff,
};
}  // namespace

ScreenStreamReceiver::ScreenStreamReceiver(ScreenStreamRenderer* renderer) : renderer_(renderer) {}
ScreenStreamReceiver::~ScreenStreamReceiver() { Stop(); }

bool ScreenStreamReceiver::AllocateFrames() {
    constexpr size_t bytes = kWidth * kHeight * sizeof(uint16_t);
    for (auto& frame : frames_) {
        frame = static_cast<uint16_t*>(heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!frame) {
            ReleaseFrames();
            return false;
        }
        memset(frame, 0, bytes);
    }
    ESP_LOGI(kTag, "PSRAM frames allocated: %u bytes, free PSRAM=%u", bytes * 2,
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return true;
}

void ScreenStreamReceiver::ReleaseFrames() {
    for (auto& frame : frames_) {
        if (frame) heap_caps_free(frame);
        frame = nullptr;
    }
}

bool ScreenStreamReceiver::Start(uint16_t port) {
    if (running_.exchange(true)) return true;
    port_ = port;
    receive_index_ = 0;
    assembling_ = false;
    has_completed_frame_ = false;
    packets_.store(0);
    complete_frames_.store(0);
    dropped_frames_.store(0);
    invalid_packets_.store(0);
    fps_.store(0);
    last_receive_us_.store(0);
    if (!AllocateFrames()) {
        ESP_LOGE(kTag, "Unable to allocate frame buffers");
        running_.store(false);
        return false;
    }
    stopped_ = xSemaphoreCreateBinary();
    if (!stopped_ || xTaskCreateWithCaps(TaskEntry, "stream_udp", 5120, this, 5, &task_,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) != pdPASS) {
        ESP_LOGE(kTag, "Unable to start UDP task");
        running_.store(false);
        if (stopped_) vSemaphoreDelete(stopped_);
        stopped_ = nullptr;
        ReleaseFrames();
        return false;
    }
    return true;
}

void ScreenStreamReceiver::Stop() {
    const bool was_running = running_.exchange(false);
    if (!was_running && !frames_[0] && !stopped_) return;
    const int sock = socket_.exchange(-1);
    if (sock >= 0) {
        shutdown(sock, SHUT_RDWR);
        close(sock);
    }
    if (stopped_) xSemaphoreTake(stopped_, pdMS_TO_TICKS(3000));
    task_ = nullptr;
    if (stopped_) vSemaphoreDelete(stopped_);
    stopped_ = nullptr;
    ReleaseFrames();
}

StreamStats ScreenStreamReceiver::Stats() const {
    return {
        .packets = packets_.load(),
        .complete_frames = complete_frames_.load(),
        .dropped_frames = dropped_frames_.load(),
        .invalid_packets = invalid_packets_.load(),
        .fps = fps_.load(),
        .last_receive_us = last_receive_us_.load(),
    };
}

void ScreenStreamReceiver::ResetAssembly(uint16_t frame_id, int64_t now) {
    active_frame_id_ = frame_id;
    chunk_map_ = 0;
    frame_started_us_ = now;
    assembling_ = true;
}

void ScreenStreamReceiver::TaskEntry(void* context) {
    static_cast<ScreenStreamReceiver*>(context)->TaskLoop();
}

void ScreenStreamReceiver::TaskLoop() {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(kTag, "socket failed: %d", errno);
        running_.store(false);
        if (stopped_) xSemaphoreGive(stopped_);
        vTaskDelete(nullptr);
        return;
    }
    socket_.store(sock);
    timeval timeout = {.tv_sec = 0, .tv_usec = 20000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port_);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        ESP_LOGE(kTag, "bind UDP %u failed: %d", port_, errno);
        running_.store(false);
        if (socket_.exchange(-1) == sock) close(sock);
        if (stopped_) xSemaphoreGive(stopped_);
        vTaskDelete(nullptr);
        return;
    }
    ESP_LOGI(kTag, "UDP receiver listening on port %u", port_);

    {
        uint8_t packet[kHeaderSize + kPayloadSize];
        uint32_t last_complete = 0;
        int64_t last_log = esp_timer_get_time();
        while (running_.load()) {
            const ssize_t length = recvfrom(sock, packet, sizeof(packet), 0, nullptr, nullptr);
            const int64_t now = esp_timer_get_time();
            if (length < 0) {
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR && running_.load()) {
                    ESP_LOGW(kTag, "recvfrom failed: %d", errno);
                }
                if (assembling_ && now - frame_started_us_ > kFrameTimeoutUs) {
                    dropped_frames_.fetch_add(1);
                    assembling_ = false;
                }
                continue;
            }
            packets_.fetch_add(1);
            last_receive_us_.store(now);
            PacketHeader header;
            if (!ParseHeader(packet, static_cast<size_t>(length), &header)) {
                invalid_packets_.fetch_add(1);
                continue;
            }
            if (header.packet_type == kPacketHeartbeat) continue;
            if (header.packet_type != kPacketFrameData || header.color_mode != kColorRgb332 ||
                header.line_count != kLinesPerPacket || header.y_start % kLinesPerPacket != 0 ||
                header.y_start + header.line_count > kHeight ||
                header.payload_length != kPayloadSize) {
                invalid_packets_.fetch_add(1);
                continue;
            }
            if (!assembling_ && has_completed_frame_ &&
                header.frame_id == last_completed_frame_id_) {
                continue;
            }

            if (!assembling_ || header.frame_id != active_frame_id_) {
                if (assembling_ && chunk_map_ != kCompleteMap) dropped_frames_.fetch_add(1);
                ResetAssembly(header.frame_id, now);
            } else if (now - frame_started_us_ > kFrameTimeoutUs) {
                dropped_frames_.fetch_add(1);
                ResetAssembly(header.frame_id, now);
            }

            const int chunk = header.y_start / kLinesPerPacket;
            const uint64_t bit = 1ULL << chunk;
            if ((chunk_map_ & bit) == 0) {
                uint16_t* destination = frames_[receive_index_] + header.y_start * kWidth;
                const uint8_t* source = packet + kHeaderSize;
                for (size_t i = 0; i < kPayloadSize; ++i) {
                    destination[i] = rgb332_to_rgb565_lut[source[i]];
                }
                chunk_map_ |= bit;
            }

            if (chunk_map_ == kCompleteMap) {
                complete_frames_.fetch_add(1);
                last_completed_frame_id_ = header.frame_id;
                has_completed_frame_ = true;
                if (renderer_->TrySubmit(frames_[receive_index_])) {
                    receive_index_ ^= 1;
                } else {
                    dropped_frames_.fetch_add(1);
                }
                assembling_ = false;
            }

            if (now - last_log >= 1000000) {
                const uint32_t complete = complete_frames_.load();
                fps_.store(complete - last_complete);
                last_complete = complete;
                last_log = now;
                ESP_LOGI(kTag,
                         "packets=%u complete=%u dropped=%u invalid=%u fps=%u draw_avg=%uus draw_max=%uus internal=%u psram=%u",
                         packets_.load(), complete, dropped_frames_.load(), invalid_packets_.load(),
                         fps_.load(), renderer_->AverageDrawUs(), renderer_->MaxDrawUs(),
                         heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                         heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
            }
        }
    }

    if (socket_.exchange(-1) == sock && sock >= 0) close(sock);
    if (stopped_) xSemaphoreGive(stopped_);
    vTaskDelete(nullptr);
}

}  // namespace screen_stream
