
//#include <esp_log.h>
//#include <esp_now.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
//#include "esp_chip_info.h"
//#include "esp_event.h"
//#include "esp_netif.h"
//#include "esp_wifi.h"
#include "nvs_flash.h"
//#include "esp_crc.h"

#include "network.h"

#define ESP_NOW_MSG_TYPE_REG            0
#define ESP_NOW_MSG_TYPE_REG_ACK        1

#define ESP_NOW_MSG_BROADCAST           0
#define ESP_NOW_MSG_UNICAST             1


//
// struct esp_now_msg_header{
//     uint32_t flags; // bit field (type, broadcast/unicast, msg length)
//     uint16_t magic;
//     uint16_t crc;
// };
//
// // client states
// static uint8_t client_states[ESP_NOW_NUM_CLIENTS];
// static uint8_t client_macs[ESP_NOW_NUM_CLIENTS][ESP_NOW_ETH_ALEN];
//
// static bool wifi_long_range = true;
// static uint8_t esp_now_mode = ESP_NOW_CONTROLLER_MODE;
// static uint8_t esp_now_conn_state = ESP_NOW_CONN_STATE_REG;
// static const char *TAG = "battery_sensor";
// static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
// static QueueHandle_t esp_now_queue = NULL;
// static uint8_t magic = 132;
// static uint8_t wifi_mac[ESP_NOW_ETH_ALEN];
// static size_t common_message_size = sizeof(struct esp_now_msg_header);
// static uint8_t *msg_buffer;
//
//
//
// static bool all_clients_checked_in() {
//     bool all_checked_in = true;
//     for (uint8_t i = 0; i < ESP_NOW_NUM_CLIENTS; i++) {
//         if (client_states[i] != ESP_NOW_CONN_STATE_READY) {
//             all_checked_in = false;
//             break;
//         }
//     }
//     return all_checked_in;
// }
//
// static int send_broadcast_register_message() {
//     // |0                |0000        |0000     |00000000   |000000000000000|
//     // |broadcast/unicast|message type|client id|msg_len    |unused         |
//     // example flags - 0 0000 10000100 00000110 00000000000 - 69218304
//     uint32_t flags = ESP_NOW_MSG_BROADCAST;
//     flags <<= 4; // width of message type 4 bits
//     flags = flags | ESP_NOW_MSG_TYPE_REG;
//     flags <<= 4; // width of client id
//     flags |= ESP_NOW_CONTROLLER_CLIENT;
//     flags <<= 8; // width of message length
//     flags |= 0; // broadcast register message contains no data
//     flags <<= 15; // left over bits
//
//     struct esp_now_msg_header *hdr = (struct esp_now_msg_header *)msg_buffer;
//     hdr->flags = flags;
//     hdr->magic = magic;
//     hdr->crc = 0;
//     hdr->crc = esp_crc16_le(UINT16_MAX, msg_buffer, common_message_size);
//     const esp_err_t result = esp_now_send(broadcast_mac, msg_buffer, common_message_size);
//     ESP_LOGI(TAG, "Broadcast register message sent: %d", result);
//     return result;
// }
//
// static int send_registration_ack_message(const uint8_t *mac_addr, const uint8_t client_id) {
//     uint32_t flags = ESP_NOW_MSG_UNICAST;
//     flags <<= 4; // width of message type 4 bits
//     flags = flags | ESP_NOW_MSG_TYPE_REG_ACK;
//     flags <<= 4; // width of client id
//     flags |= client_id;
//     flags <<= 8; // width of message length
//     flags |= 0; // unicast register ack message contains no data
//     flags <<= 15; // left over bits
//
//     struct esp_now_msg_header *hdr = (struct esp_now_msg_header *)msg_buffer;
//     hdr->flags = flags;
//     hdr->magic = magic;
//     hdr->crc = 0;
//     hdr->crc = esp_crc16_le(UINT16_MAX, msg_buffer, common_message_size);
//     const esp_err_t result = esp_now_send(mac_addr, msg_buffer, common_message_size);
//     ESP_LOGI(TAG, "register-ack message sent: %d", result);
//     return result;
// }
//
// static int esp_now_parse_message(const uint8_t *msg, const size_t msg_len, struct esp_now_msg_header **hdr) {
//     // TODO make sure the given buffer is big enough and we don't have a truncated message
//     *hdr = (struct esp_now_msg_header *)msg;
//
//     // is it magical
//     if ((*hdr)->magic != magic) {
//         ESP_LOGW(TAG, "message has wrong magic.");
//         return ESP_FAIL;
//     }
//
//     // CRC check
//     const uint16_t crc = (*hdr)->crc;
//     (*hdr)->crc = 0;
//     const uint16_t crc_cal = esp_crc16_le(UINT16_MAX, msg, msg_len);
//
//     ESP_LOGI(TAG, "CRC: 0x%04X", crc);
//     ESP_LOGI(TAG, "CRC CALC: 0x%04X", crc_cal);
//     if (crc_cal != crc) {
//         ESP_LOGW(TAG, "CRC CHECK FAILED: 0x%04X 0x%04X", crc, crc_cal);
//         return ESP_FAIL;
//     }
//     return ESP_OK;
// }
//
// static uint8_t get_message_type(const struct esp_now_msg_header *hdr) {
//     uint32_t type_mask = 15;
//     type_mask <<= 27;
//     const uint32_t type = (hdr->flags & type_mask) >> 27;
//     return (uint8_t)type;
// }
//
// static uint8_t get_message_client_id(const struct esp_now_msg_header *hdr) {
//     uint32_t type_mask = 15;
//     type_mask <<= 23;
//     const uint32_t client_id = (hdr->flags & type_mask) >> 23;
//     return (uint8_t)client_id;
// }
//
//
// static int esp_now_controller_process() {
//     struct esp_now_event event;
//     while (xQueueReceive(esp_now_queue, &event, 0) == pdTRUE) {
//         ESP_LOGI(TAG, "Received event: %d", event.event_id);
//         if (event.event_id == ESP_NOW_EVENT_SEND) {
//             const bool is_broadcast = memcmp(event.data.send_event.mac_addr, broadcast_mac, ESP_NOW_ETH_ALEN) == 0;
//             uint8_t client_id = 0;
//             if (!is_broadcast)
//                 client_id = find_client_id_with_mac(event.data.send_event.mac_addr); // find the client with the associated mac address
//             if (event.data.send_event.status == ESP_NOW_SEND_SUCCESS) {
//                 if (is_broadcast)
//                     ESP_LOGI(TAG, "broadcast message sent successfully!");
//                 else {
//                     if (client_id < ESP_NOW_NUM_CLIENTS && client_states[client_id] == ESP_NOW_CONN_STATE_REG_ACK) {
//                         client_states[client_id] = ESP_NOW_CONN_STATE_READY;
//                         ESP_LOGI(TAG, "client id, %d is ready!", client_id);
//                     }
//                 }
//             } else {
//                 // if there is a failure, and it's not a broadcast message, and it's a reg-ack, resend
//                 if (!is_broadcast && client_id < ESP_NOW_NUM_CLIENTS && client_states[client_id] == ESP_NOW_CONN_STATE_REG_ACK) {
//                     send_registration_ack_message(event.data.send_event.mac_addr, ESP_NOW_CONTROLLER_CLIENT);
//                 }
//             }
//         } else if (event.event_id == ESP_NOW_EVENT_RECEIVE) {
//             // verify message header CRC and magic
//             const struct esp_now_receive_event *receive_event = &event.data.receive_event;
//             struct esp_now_msg_header *hdr;
//             if (esp_now_parse_message(receive_event->data, receive_event->data_len, &hdr) != ESP_OK)
//                 continue;
//             // if reg-ack message does it matter what state we are in no we'll just respond no matter what
//             if (get_message_type(hdr) == ESP_NOW_MSG_TYPE_REG_ACK) {
//                 const uint8_t client_id = get_message_client_id(hdr);
//                 memcpy(client_macs[client_id], receive_event->mac_addr, ESP_NOW_ETH_ALEN); // save mac for the given client
//                 add_peer(receive_event->mac_addr);
//                 // send reg-ack message to client completing registration
//                 if (send_registration_ack_message(receive_event->mac_addr, ESP_NOW_CONTROLLER_CLIENT) == ESP_OK)
//                     client_states[client_id] = ESP_NOW_CONN_STATE_REG_ACK; // update client state to reg-ack
//             }
//         }
//     }
//
//     if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG) {
//         if (all_clients_checked_in())
//             esp_now_conn_state = ESP_NOW_CONN_STATE_READY;
//         else
//             return send_broadcast_register_message();
//     }
//
//     return ESP_OK;
// }
//
// static int esp_now_client_process() {
//     // TODO: figure out what to do with sequence number, do we really care if messages are in order
//
//     struct esp_now_event event;
//     while (xQueueReceive(esp_now_queue, &event, 0) == pdTRUE) {
//         ESP_LOGI(TAG, "Received event: %d", event.event_id);
//         if (event.event_id == ESP_NOW_EVENT_SEND) {
//             if (event.data.send_event.status == ESP_NOW_SEND_SUCCESS) {
//                 ESP_LOGI(TAG, "message sent successfully!");
//             } else {
//                 if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG_ACK) {
//                     ESP_LOGI(TAG, "register ack message failed to send, reset state to register");
//                     esp_now_conn_state = ESP_NOW_CONN_STATE_REG;
//                 }
//             }
//         } else if (event.event_id == ESP_NOW_EVENT_RECEIVE) {
//             const struct esp_now_receive_event *receive_event = &event.data.receive_event;
//             struct esp_now_msg_header *hdr;
//             if (esp_now_parse_message(receive_event->data, receive_event->data_len, &hdr) != ESP_OK)
//                 continue;
//             if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG) {
//                 if (get_message_type(hdr) == ESP_NOW_MSG_TYPE_REG) {
//                     add_peer(receive_event->mac_addr);
//                     if (send_registration_ack_message(receive_event->mac_addr, ESP_NOW_BATTERY_CLIENT) == ESP_OK)
//                         esp_now_conn_state = ESP_NOW_CONN_STATE_REG_ACK;
//                 }
//             } else if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG_ACK) {
//                 // TODO: timeout if we've in the reg-ack state for too long
//                 // make sure this is a register-ack message type
//                 // change connection state to ready
//                 if (get_message_type(hdr) == ESP_NOW_MSG_TYPE_REG_ACK && get_message_client_id(hdr) == ESP_NOW_CONTROLLER_CLIENT) {
//                     esp_now_conn_state = ESP_NOW_CONN_STATE_READY;
//                     ESP_LOGI(TAG, "connection to controller ready!");
//                 }
//             } else if (esp_now_conn_state == ESP_NOW_CONN_STATE_READY) {
//                 // take action based on the message type
//             }
//             free(receive_event->data);
//         }
//     }
//
//     return ESP_OK;
// }
//
// static void sensor_network_task(void *pvParameter) {
//     start_wifi();
//     ESP_LOGI(TAG, "WIFI interface started in station mode at 2.4G!");
//
//     start_esp_now();
//     ESP_LOGI(TAG, "ESP-NOW started!");
//
//     for (uint8_t i = 0; i < ESP_NOW_NUM_CLIENTS; i++) {
//         client_states[i] = ESP_NOW_CONN_STATE_REG;
//     }
//
//     msg_buffer = malloc(common_message_size);
//     if (msg_buffer == NULL) {
//         ESP_LOGE(TAG, "message buffer allocation failed!");
//         goto graceful_exit;
//     }
//     memset(msg_buffer, 0, common_message_size);
//
//     int result = ESP_OK;
//
//     // ReSharper disable once CppDFAEndlessLoop
//     while (1) {
//         // TODO: Check if someone says we should shutdown
//
//         if (esp_now_mode == ESP_NOW_CLIENT_MODE)
//             result = esp_now_client_process();
//         if (esp_now_mode == ESP_NOW_CONTROLLER_MODE)
//             result = esp_now_controller_process();
//
//         // TODO: Change led color based on connection status
//
//         if (result != ESP_OK) { // catastrophic failure try restart?
//             break;
//         }
//
//         if (esp_now_mode == ESP_NOW_CLIENT_MODE)
//             vTaskDelay(250 / portTICK_PERIOD_MS);
//         else
//             vTaskDelay(1000 / portTICK_PERIOD_MS);
//     }
//
// graceful_exit:
//     // cleanup
//     free(msg_buffer);
//     stop_esp_now();
//     ESP_LOGI(TAG, "ESP_NOW stopped!");
//     stop_wifi();
//     ESP_LOGI(TAG, "WIFI interface stopped!");
//     // adios
//     vTaskDelete(NULL);
// }

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    struct network_config ncfg;
    ncfg.mode = NETWORK_MODE_CONTROLLER;
    ncfg.client_id = NETWORK_CLIENT_CONTROLLER;

    // TODO: Setup queue to communicate between tasks, the can task will need to send messages on the esp-now network
    // the esp-now task will need to tell can task to shutdown
    // can task will need to tell esp-now task to shutdown

    TaskHandle_t taskHandle = NULL;
    xTaskCreate(network_task, "network_task", 8192, &ncfg, 4, &taskHandle);

    // TODO: infinite loop to monitor tasks

    // TODO: find peek memory usage by esp-now task so can give proper heap size
}
