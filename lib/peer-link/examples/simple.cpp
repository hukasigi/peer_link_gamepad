#include <Arduino.h>
#include <peer_link.h>
#include <string>

const uint8_t WIFI_CHANNEL = 14;
const peer_id_t PEER_ID = 0x12;

void peer_link_recv_cb(const peer_id_t peer_id, const std::vector<struct Message>& messages) {
    printf("id: 0x%02X\r\n", peer_id);
    for (struct Message message: messages) {
        printf("type = %d, data = ", message.type);
        for (uint8_t byte : message.data) {
            printf("%02X, ", byte);
        }
        printf("\r\n");
    }
}

void setup() {
    peer_link_task_init(WIFI_CHANNEL, PEER_ID);
}

void loop() {
    std::vector<struct Message> messages;
    std::string str = "Hello World!";
    struct Message message = {
        .type = 0xAB,
        .data = std::vector<uint8_t>(str.begin(), str.end())
    };
    messages.push_back(std::move(message));
    peer_link_send(BROADCAST_ID, messages);
    delay(1000);
}
