
#include <esp_log.h>
#include <esp_now.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_crc.h"

// TODO: Make a config variable
#define WIFI_PRIMARY_CHANNEL            10

#define ESP_NOW_CLIENT_MODE             0
#define ESP_NOW_CONTROLLER_MODE         1

#define ESP_NOW_COOLANT_CLIENT          0
#define ESP_NOW_VCU_CLIENT              1
#define ESP_NOW_BATTERY_CLIENT          2
#define ESP_NOW_NUM_CLIENTS             3

#define ESP_NOW_QUEUE_SIZE              10

#define ESP_NOW_CONN_STATE_REG          0
#define ESP_NOW_CONN_STATE_REG_ACK      1
#define ESP_NOW_CONN_STATE_READY        2

#define ESP_NOW_EVENT_RECEIVE           0
#define ESP_NOW_EVENT_SEND              1

#define ESP_NOW_MSG_TYPE_REG            0
#define ESP_NOW_MSG_TYPE_REG_ACK        1

#define ESP_NOW_MSG_BROADCAST           0
#define ESP_NOW_MSG_UNICAST             1

#define QUEUE_MAX_DELAY                 512

struct esp_now_send_event{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
};

struct esp_now_receive_event {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    uint16_t data_len;
};

union esp_now_event_data {
    struct esp_now_send_event send_event;
    struct esp_now_receive_event receive_event;
};

struct esp_now_event {
    uint8_t event_id;
    union esp_now_event_data data;
};

struct esp_now_msg_header{
    uint32_t flags; // bit field (type, broadcast/unicast, msg length)
    uint16_t magic;
    uint16_t crc;
};

// client states
static uint8_t client_states[ESP_NOW_NUM_CLIENTS];

static bool wifi_long_range = true;
static uint8_t esp_now_mode = ESP_NOW_CLIENT_MODE;
static uint8_t esp_now_conn_state = ESP_NOW_CONN_STATE_REG;
static const char *TAG = "battery_sensor";
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static QueueHandle_t esp_now_queue = NULL;
static uint8_t magic = 132;
static uint8_t wifi_mac[ESP_NOW_ETH_ALEN];
static size_t common_message_size = sizeof(struct esp_now_msg_header);
static uint8_t *msg_buffer;

/**
 * @brief start the wi-fi driver, needed to create an esp-now network
 */
static void start_wifi() {
    ESP_ERROR_CHECK(esp_netif_init()); // initialize tcp/ip staci
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // create event loop so can handle tcp/ip events
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // macro to create default wifi config
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) ); // store settings in volatile ram
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) ); // put wi-fi in station mode
    ESP_ERROR_CHECK( esp_wifi_start()); // start the wi-fi
    ESP_ERROR_CHECK( esp_wifi_set_channel(WIFI_PRIMARY_CHANNEL, WIFI_SECOND_CHAN_NONE));

    if (wifi_long_range) {
        ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA,
            WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
    }

    ESP_ERROR_CHECK( esp_wifi_get_mac(WIFI_MODE_STA, wifi_mac) );
}

/**
 * @brief stop the wi-fi driver and clean up
 */
static void stop_wifi() {
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

static void receive_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    const uint8_t *src_addr = recv_info->src_addr;
    const uint8_t *des_addr = recv_info->des_addr;

    if (src_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "receive call back argument error");
        return;
    }

    ESP_LOGI(TAG, "receive callback source address: %02x:%02x:%02x:%02x:%02x:%02x",
        src_addr[0],
        src_addr[1],
        src_addr[2],
        src_addr[3],
        src_addr[4],
        src_addr[5]);

    ESP_LOGI(TAG, "receive callback destination address: %02x:%02x:%02x:%02x:%02x:%02x",
        des_addr[0],
        des_addr[1],
        des_addr[2],
        des_addr[3],
        des_addr[4],
        des_addr[5]);

    struct esp_now_event evt;
    struct esp_now_receive_event *recv_evt = &evt.data.receive_event;
    evt.event_id = ESP_NOW_EVENT_RECEIVE;
    memcpy(recv_evt->mac_addr, src_addr, ESP_NOW_ETH_ALEN);
    recv_evt->data = malloc(len);
    if (recv_evt->data == NULL) {
        ESP_LOGE(TAG, "malloc receive data fail");
        return;
    }
    memcpy(recv_evt->data, data, len);
    recv_evt->data_len = len;
    if (xQueueSend(esp_now_queue, &evt, QUEUE_MAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "send receive queue fail");
        free(recv_evt->data);
    }
}

static void send_callback(const uint8_t *mac_addr, const esp_now_send_status_t status) {
    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "send call back argument error.");
        return;
    }

    struct esp_now_event event;
    event.event_id = ESP_NOW_EVENT_SEND;
    struct esp_now_send_event *send_event = &event.data.send_event;
    send_event->status = status;
    memcpy(send_event->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

    if (xQueueSend(esp_now_queue, &event, QUEUE_MAX_DELAY) != pdTRUE) {
        ESP_LOGW(TAG, "send callback queue send failed.");
    }
}

static int add_peer(const uint8_t *mac_addr) {
    if (esp_now_is_peer_exist(mac_addr))
        return ESP_OK;
    esp_now_peer_info_t peer_info;
    peer_info.channel = WIFI_PRIMARY_CHANNEL;
    peer_info.ifidx = ESP_IF_WIFI_STA;
    peer_info.encrypt = false;
    memcpy(&(peer_info.peer_addr), mac_addr, ESP_NOW_ETH_ALEN);
    return esp_now_add_peer(&peer_info);
}

/**
 * @brief start esp_now service and any accompanying objects
 */
static int start_esp_now() {
    esp_now_queue = xQueueCreate(ESP_NOW_QUEUE_SIZE, sizeof(struct esp_now_event));
    if (esp_now_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(send_callback) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(receive_callback) );

    // add broadcast mac to list of peers so we can use it for bootstrapping
    if (esp_now_mode == ESP_NOW_CONTROLLER_MODE)
        ESP_ERROR_CHECK(add_peer(broadcast_mac));

    // start in register mode
    esp_now_conn_state = ESP_NOW_CONN_STATE_REG;

    return ESP_OK;
}

/**
 * stop esp now service
 */
static void stop_esp_now() {
    esp_now_deinit();
    if (esp_now_queue != NULL) {
        vQueueDelete(esp_now_queue);
        esp_now_queue = NULL;
    }
}

static bool all_clients_checked_in() {
    bool all_checked_in = true;
    for (uint8_t i = 0; i < ESP_NOW_NUM_CLIENTS; i++) {
        if (client_states[i] != ESP_NOW_CONN_STATE_READY) {
            all_checked_in = false;
            break;
        }
    }
    return all_checked_in;
}

static int send_broadcast_register_message() {
    // |0                |0000        |00000000   |0000000000000000000|
    // |broadcast/unicast|message type|msg_len    |unused             |
    // example flags - 0 0000 10000100 00000110 00000000000 - 69218304
    uint32_t flags = ESP_NOW_MSG_BROADCAST;
    flags <<= 4; // width of message type 4 bits
    flags = flags | ESP_NOW_MSG_TYPE_REG;
    flags <<= 8; // width of message length
    flags |= 0; // broadcast register message contains no data
    flags <<= 19; // left over bits

    struct esp_now_msg_header *hdr = (struct esp_now_msg_header *)msg_buffer;
    hdr->flags = flags;
    hdr->magic = magic;
    hdr->crc = 0;
    hdr->crc = esp_crc16_le(UINT16_MAX, msg_buffer, common_message_size);
    const esp_err_t result = esp_now_send(broadcast_mac, msg_buffer, common_message_size);
    ESP_LOGI(TAG, "Broadcast register message sent: %d", result);
    return result;
}

static int send_registration_ack_message(const uint8_t *mac_addr) {
    uint32_t flags = ESP_NOW_MSG_UNICAST;
    flags <<= 4; // width of message type 4 bits
    flags = flags | ESP_NOW_MSG_TYPE_REG_ACK;
    flags <<= 8; // width of message length
    flags |= 0; // unicast register ack message contains no data
    flags <<= 19; // left over bits

    struct esp_now_msg_header *hdr = (struct esp_now_msg_header *)msg_buffer;
    hdr->flags = flags;
    hdr->magic = magic;
    hdr->crc = 0;
    hdr->crc = esp_crc16_le(UINT16_MAX, msg_buffer, common_message_size);
    const esp_err_t result = esp_now_send(mac_addr, msg_buffer, common_message_size);
    ESP_LOGI(TAG, "register-ack message sent: %d", result);
    return result;
}

static int esp_now_controller_process() {
    // process each event on the queue
    // if send event check for failures and potentially resend messages
        // if register_ack and success mark client as registered
    // if receive event
        // unicast register message and not registered, register the peer
        // send unicast register_ack message to client

    // register mode
    // if not all the clients have checked in (received unicast register and register_ack sent successfully),
        // send broadcast register message
    // if all have checked in change mode to ready

    struct esp_now_event event;
    while (xQueueReceive(esp_now_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Received event: %d", event.event_id);
        if (event.event_id == ESP_NOW_EVENT_SEND) {
            if (event.data.send_event.status == ESP_NOW_SEND_SUCCESS) {
                ESP_LOGI(TAG, "broadcast message sent successfully!");
            }
        } else if (event.event_id == ESP_NOW_EVENT_RECEIVE) {

        }
    }

    if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG) {
        if (all_clients_checked_in())
            esp_now_conn_state = ESP_NOW_CONN_STATE_READY;
        else
            return send_broadcast_register_message();
    }

    return ESP_OK;
}

static int esp_now_parse_message(const uint8_t *msg, const size_t msg_len, struct esp_now_msg_header **hdr) {
    // TODO make sure the given buffer is big enough and we don't have a truncated message
    *hdr = (struct esp_now_msg_header *)msg;

    // CRC check
    const uint16_t crc = (*hdr)->crc;
    (*hdr)->crc = 0;
    const uint16_t crc_cal = esp_crc16_le(UINT16_MAX, msg, msg_len);

    ESP_LOGI(TAG, "CRC: 0x%04X", crc);
    ESP_LOGI(TAG, "CRC CALC: 0x%04X", crc_cal);
    if (crc_cal != crc) {
        ESP_LOGW(TAG, "CRC CHECK FAILED: 0x%04X 0x%04X", crc, crc_cal);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static uint8_t get_message_type(const struct esp_now_msg_header *hdr) {
    // |0                |0000        |00000000   |0000000000000000000|
    // |broadcast/unicast|message type|msg_len    |unused             |
    uint32_t type_mask = 15;
    type_mask <<= 27;
    const uint32_t type = (hdr->flags & type_mask) >> 27;
    return (uint8_t)type;
}

static int esp_now_client_process() {
    // TODO: figure out what to do with sequence number, do we really care if messages are in order

    struct esp_now_event event;
    while (xQueueReceive(esp_now_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(TAG, "Received event: %d", event.event_id);
        if (event.event_id == ESP_NOW_EVENT_SEND) {
            if (event.data.send_event.status == ESP_NOW_SEND_SUCCESS) {
                ESP_LOGI(TAG, "message sent successfully!");
            } else {
                if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG_ACK) {
                    ESP_LOGI(TAG, "register ack message failed to send, reset state to register");
                    esp_now_conn_state = ESP_NOW_CONN_STATE_REG;
                }
            }
        } else if (event.event_id == ESP_NOW_EVENT_RECEIVE) {
            const struct esp_now_receive_event *receive_event = &event.data.receive_event;
            struct esp_now_msg_header *hdr;
            if (esp_now_parse_message(receive_event->data, receive_event->data_len, &hdr) != ESP_OK)
                continue;
            if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG) {
                // make sure this is a register message type
                // add mac to peer list
                // send unicast message to controller tell which client we are
                // change connection state to register-ack
                if (get_message_type(hdr) == ESP_NOW_MSG_TYPE_REG) {
                    add_peer(receive_event->mac_addr);
                    if (send_registration_ack_message(receive_event->mac_addr) == ESP_OK)
                        esp_now_conn_state = ESP_NOW_CONN_STATE_REG_ACK;
                }
            } else if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG_ACK) {
                // make sure this is a register-ack message type
                // change connection state to ready
            } else if (esp_now_conn_state == ESP_NOW_CONN_STATE_READY) {
                // take action based on the message type
            }
            free(receive_event->data);
        }
    }

    return ESP_OK;
}

static void sensor_network_task(void *pvParameter) {
    start_wifi();
    ESP_LOGI(TAG, "WIFI interface started in station mode at 2.4G!");

    start_esp_now();
    ESP_LOGI(TAG, "ESP-NOW started!");

    for (uint8_t i = 0; i < ESP_NOW_NUM_CLIENTS; i++) {
        client_states[i] = ESP_NOW_CONN_STATE_REG;
    }

    msg_buffer = malloc(common_message_size);
    if (msg_buffer == NULL) {
        ESP_LOGE(TAG, "message buffer allocation failed!");
        goto graceful_exit;
    }
    memset(msg_buffer, 0, common_message_size);

    int result = ESP_OK;

    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        // TODO: Check if someone says we should shutdown

        if (esp_now_mode == ESP_NOW_CLIENT_MODE)
            result = esp_now_client_process();
        if (esp_now_mode == ESP_NOW_CONTROLLER_MODE)
            result = esp_now_controller_process();

        if (result != ESP_OK) { // catastrophic failure try restart?
            break;
        }

        if (esp_now_mode == ESP_NOW_CLIENT_MODE)
            vTaskDelay(250 / portTICK_PERIOD_MS);
        else
            vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

graceful_exit:
    // cleanup
    free(msg_buffer);
    stop_esp_now();
    ESP_LOGI(TAG, "ESP_NOW stopped!");
    stop_wifi();
    ESP_LOGI(TAG, "WIFI interface stopped!");
    // adios
    vTaskDelete(NULL);
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    // TODO: put all esp-now stuff in own esp-idf component for sharing

    // TODO: Setup queue to communicate between tasks, the can task will need to send messages on the esp-now network
    // the esp-now task will need to tell can task to shutdown
    // can task will need to tell esp-now task to shutdown

    TaskHandle_t taskHandle = NULL;
    xTaskCreate(sensor_network_task, "esp_now_task", 8192, NULL, 4, &taskHandle);

    // TODO: infinite loop to monitor tasks

    // TODO: find peek memory usage by esp-now task so can give proper heap size
}
