
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
#include "esp_system.h"
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

static bool wifi_long_range = true;
static uint8_t esp_now_mode = ESP_NOW_CONTROLLER_MODE;
static uint8_t esp_now_conn_state = ESP_NOW_CONN_STATE_REG;
static const char *TAG = "battery_sensor";
static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static QueueHandle_t esp_now_queue = NULL;
static uint16_t seq = 0;
static uint8_t magic = 132;
static uint8_t wifi_mac[ESP_NOW_ETH_ALEN];

struct esp_now_send_event{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
};

struct esp_now_receive_event {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
};

union esp_now_event_data {
    struct esp_now_send_event *send_event;
    struct esp_now_receive_event *receive_event;
};

struct esp_now_event {
    uint8_t event_id;
    union esp_now_event_data *data;
};

struct esp_now_header{
    uint32_t flags; // 4 bit field (type, magic, broadcast/unicast)
    uint16_t seq_num;
    uint16_t crc;
};

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

}

static void send_callback(const uint8_t *mac_addr, esp_now_send_status_t status) {

}

/**
 * @brief start esp_now service and any accompanying objects
 */
static int start_esp_now() {
    esp_now_queue = xQueueCreate(ESP_NOW_QUEUE_SIZE, sizeof(struct esp_now_event *));
    if (esp_now_queue == NULL) {
        ESP_LOGE(TAG, "Create queue fail");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(send_callback) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(receive_callback) );

    // add broadcast mac to list of peers so we can use it for bootstrapping
    esp_now_peer_info_t peer_info;
    peer_info.channel = WIFI_PRIMARY_CHANNEL;
    peer_info.ifidx = ESP_IF_WIFI_STA;
    peer_info.encrypt = false;
    memcpy(&(peer_info.peer_addr), broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(&peer_info) );

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
    return false;
}

static int send_broadcast_register_message() {
    uint32_t flags = ESP_NOW_MSG_BROADCAST;
    flags <<= 4;
    flags = flags | ESP_NOW_MSG_TYPE_REG;
    flags <<= 8;
    flags |= magic;
    flags <<= 19;

    const size_t buffer_size = sizeof(struct esp_now_header) + ESP_NOW_ETH_ALEN;
    uint8_t *buffer = malloc(buffer_size);
    struct esp_now_header *hdr = (struct esp_now_header *)buffer;
    hdr->flags = flags;
    hdr->seq_num = seq++;
    hdr->crc = 0;
    memcpy(buffer + sizeof(struct esp_now_header), wifi_mac, ESP_NOW_ETH_ALEN);
    hdr->crc = esp_crc16_le(UINT16_MAX, buffer, buffer_size);
    const esp_err_t result = esp_now_send(broadcast_mac, buffer, buffer_size);
    ESP_LOGI(TAG, "Broadcast register message sent: %d", result);
    free(buffer);
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

    // struct esp_now_event *event;
    // while (xQueueReceive(esp_now_queue, &event, 0) == pdTRUE) {
    //     // if (event->event_id == ESP_NOW_EVENT_SEND) {
    //     //
    //     // } else if (event->event_id == ESP_NOW_EVENT_RECEIVE) {
    //     //
    //     // }
    // }

    if (esp_now_conn_state == ESP_NOW_CONN_STATE_REG && !all_clients_checked_in()) {
        return send_broadcast_register_message();
    }

    return ESP_OK;
}

static int esp_now_client_process() {
    // process each event in the queue
    // if send event check for failures and log them and potentially resend them
    // if receive event
        // register broadcast message in register mode, add to peer list, register-ack mode
            // send unicast register message to controller
        // register-ack unicast message, change to ready mode

    return ESP_OK;
}

static int esp_now_process() {
    if (esp_now_mode == ESP_NOW_CLIENT_MODE)
        return esp_now_client_process();
    if (esp_now_mode == ESP_NOW_CONTROLLER_MODE)
        return esp_now_controller_process();
    return ESP_OK;
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

    start_wifi();
    ESP_LOGI(TAG, "WIFI interface started in station mode at 2.4G!");

    start_esp_now();
    ESP_LOGI(TAG, "ESP-NOW started!");

    esp_now_process();

    stop_esp_now();
    ESP_LOGI(TAG, "ESP_NOW stopped!");

    stop_wifi();
    ESP_LOGI(TAG, "WIFI interface stopped!");

    /* Print chip information */
    // esp_chip_info_t chip_info;
    // uint32_t flash_size;
    // esp_chip_info(&chip_info);
    // printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
    //        CONFIG_IDF_TARGET,
    //        chip_info.cores,
    //        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
    //        (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
    //        (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
    //        (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");
    //
    // unsigned major_rev = chip_info.revision / 100;
    // unsigned minor_rev = chip_info.revision % 100;
    // printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    // if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
    //     printf("Get flash size failed");
    //     return;
    // }
    //
    // printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
    //        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");
    //
    // printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    // uint32_t flags = ESP_NOW_MSG_BROADCAST; // 0000 0000 0000 0000 0000 0000 0000 0000
    // flags <<= 4; // 4 bits for type 0000 0000 0000 0000 0000 0000 0000 0000
    // flags = flags | ESP_NOW_MSG_TYPE_REG; // 0000 0000 0000 0000 0000 0000 0000 0000
    // flags <<= 8; // 8 bits for magic 0000 0000 0000 0000 0000 0000 0000 0000
    // printf("%lu\n", flags); // should be 0
    // uint8_t magic = 123; // 0111 1011
    // flags |= magic; // 0000 0000 0000 0000 0000 0000 0111 1011
    // printf("%lu\n", flags); // should be 123
    // flags <<= 19; // 0000 0011 1101 1000 0000 0000 0000 0000
    // printf("%lu\n", flags); // should be 64487424

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
