#include "gamepad.h"
#include <Arduino.h>
#include <peer_link.h>
#define TAG "App"
const uint8_t       LED_PIN            = 21;
const peer_id_t     TO_PEER_ID         = SWERVE_S3_ID;
const peer_id_t     FROM_PEER_ID       = Gamepad_ESP_ID;
const uint8_t       GAMEPAD_TIMEOUT_MS = 250;
struct GamepadData  gamepad_data;
struct PS4Data      ps4_data;
struct PS4OutReport ps4_state = {.report_id    = PS4_OUT_REPORT_ID,
                                 .magic_number = PS4_OUT_REPORT_MAGIC_NUMBER,
                                 .__reserved   = 0,
                                 .small_rumble = 0,
                                 .big_rumble   = 0,
                                 .r            = 0,
                                 .g            = 0,
                                 .b            = 0,
                                 .flash_on     = 0,
                                 .flash_off    = 0};
Gamepad             gamepad   = Gamepad();
uint32_t            last_receive_gamepad;
uint32_t            rumble_start;
uint16_t            rumble_duration;

void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<struct Message>& messages) {
    for (auto message : messages) {
        if (message.type == static_cast<uint8_t>(MessageType::PS4SetRumble)) {
            struct PS4RumbleData* data = (struct PS4RumbleData*)(message.data.data());
            ps4_state.small_rumble     = data->small_rumble;
            ps4_state.big_rumble       = data->big_rumble;
            rumble_duration            = data->duration;
            rumble_start               = millis();
        } else if (message.type == static_cast<uint8_t>(MessageType::PS4SetLED)) {
            struct PS4LedData* data = (struct PS4LedData*)(message.data.data());
            ps4_state.r             = data->r;
            ps4_state.g             = data->g;
            ps4_state.b             = data->b;
            ps4_state.flash_on      = data->flash_on;
            ps4_state.flash_off     = data->flash_off;
        }
    }
}

void update_gamepad_data() {
    enum GamepadType gamepad_type;
    uint8_t          buffer[1024];
    if (gamepad_poll(&gamepad_type, buffer)) {
        switch (gamepad_type) {
        case GamepadType::PS4:
            ps4_report_parser(buffer, &gamepad_data, &ps4_data);
            gamepad.update(&gamepad_data);
            last_receive_gamepad = millis();
            break;
        }
    }
    if (millis() - last_receive_gamepad > GAMEPAD_TIMEOUT_MS) {
        gamepad.set_neutral();
        memset(&ps4_data, 0, sizeof(struct PS4Data));
        last_receive_gamepad = millis();
        ESP_LOGW(TAG, "Gamepad timeout.");
    }
}
void setup() {
    cherryusb_task_init(LED_PIN);
    peer_link_task_init(WIFI_CHANNEL, FROM_PEER_ID);
    last_receive_gamepad = millis();
    rumble_start         = millis();
    rumble_duration      = 0;
}

double percent = 0.0;
double step    = 0.05;
int8_t sign    = 1;

void loop() {
    update_gamepad_data();
    std::vector<struct Message> messages;
    struct Message              gamepad_message = {
                     .type = static_cast<uint8_t>(MessageType::Gamepad),
                     .data = std::vector((uint8_t*)&gamepad_data, (uint8_t*)&gamepad_data + sizeof(struct GamepadData))};
    struct Message ps4_message = {.type = static_cast<uint8_t>(MessageType::PS4Data),
                                  .data = std::vector((uint8_t*)&ps4_data, (uint8_t*)&ps4_data + sizeof(struct PS4Data))};
    messages.push_back(std::move(gamepad_message));
    messages.push_back(std::move(ps4_message));
    peer_link_send(TO_PEER_ID, messages);
    ps4_state.r         = 255 * percent;
    ps4_state.g         = 255 * abs(percent - 0.5);
    ps4_state.b         = 255 * (1. - percent);
    ps4_state.flash_on  = 10;
    ps4_state.flash_off = 10;
    percent += sign * step;
    if (percent >= 100.) {
        sign = -1;
    }
    if (0. >= percent) {
        sign = 1;
    }
    ps4_set_state(&ps4_state);
    if (millis() - rumble_start > rumble_duration) {
        ps4_state.small_rumble = 0;
        ps4_state.big_rumble   = 0;
    }
    vTaskDelay(50);
}