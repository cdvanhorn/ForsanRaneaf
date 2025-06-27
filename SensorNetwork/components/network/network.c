#include "network.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////
#define WIFI_PRIMARY_CHANNEL                    10

#define NETWORK_EVENT_QUEUE_SIZE                10
#define NETWORK_EVENT_QUEUE_MAX_DELAY           512

#define NETWORK_CONN_STATE_REG                  0
#define NETWORK_CONN_STATE_REG_ACK              1
#define NETWORK_CONN_STATE_READY                2

#define NETWORK_EVENT_RECEIVE                   0
#define NETWORK_EVENT_SEND                      1

///////////////////////////////////////////////////////////////////////////////
// COMPONENT DATA TYPES
///////////////////////////////////////////////////////////////////////////////
struct network_send_event{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
};

struct network_receive_event {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    uint16_t data_len;
};

union network_event_data {
    struct network_send_event send_event;
    struct network_receive_event receive_event;
};

struct network_event {
    uint8_t event_id;
    union network_event_data data;
};

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "sensor_network"; //!< char pointer - logging group

static uint8_t wifi_mac[ESP_NOW_ETH_ALEN]; //!< byte array - holds mac address for this node

static const uint8_t network_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //!< byte array contains broadcast mac address
static QueueHandle_t network_event_queue = NULL; //!< QueueHandle_t esp_now queue that will hold esp_now network events to be processed
static uint8_t network_mode = 0; //!< uint8_t what mode is node controller or client
static uint8_t network_client_id = 0; //!< uint8_t what is the client id for this node
static uint8_t network_conn_state = NETWORK_CONN_STATE_REG; //!< uint8_t what is the current connection state, register, register-ack, or ready

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
// UTILS
///////////////////////////////////////////////////////////////////////////////
static void log_mac_debug(const uint8_t *mac_addr) {
    ESP_LOGD(LOG_TAG, "%02x:%02x:%02x:%02x:%02x:%02x",
        mac_addr[0],
        mac_addr[1],
        mac_addr[2],
        mac_addr[3],
        mac_addr[4],
        mac_addr[5]);
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
// WIFI
///////////////////////////////////////////////////////////////////////////////
/**
 * @brief start the wi-fi driver, needed to create an esp-now network
 */
static void wifi_start() {
    ESP_ERROR_CHECK(esp_netif_init()); // initialize tcp/ip staci
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // create event loop so can handle tcp/ip events
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); // macro to create default wifi config
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) ); // store settings in volatile ram
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) ); // put wi-fi in station mode
    ESP_ERROR_CHECK( esp_wifi_start()); // start the wi-fi
    ESP_ERROR_CHECK( esp_wifi_set_channel(WIFI_PRIMARY_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESP_IF_WIFI_STA,
        WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );

    ESP_ERROR_CHECK( esp_wifi_get_mac(WIFI_MODE_STA, wifi_mac) );
}

/**
 * @brief stop the wi-fi driver and clean up
 */
static void wifi_stop() {
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

///////////////////////////////////////////////////////////////////////////////
// ESP-NOW NETWORK CALLBACKS
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief callback triggered when an esp-now message is received, create event and place on queue for processing
 * @param recv_info - esp_now_recv_info_t information on received message
 * @param data - byte array data received in message
 * @param len - int length of data byte array
 */
static void network_receive_callback(const esp_now_recv_info_t *recv_info, const uint8_t *data, const int len) {
    const uint8_t *src_addr = recv_info->src_addr;
    const uint8_t *des_addr = recv_info->des_addr;

    if (src_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(LOG_TAG, "receive call back argument error");
        return;
    }

    ESP_LOGD(LOG_TAG, "receive callback source address: ");
    log_mac_debug(src_addr);

    ESP_LOGD(LOG_TAG, "receive callback destination address: ");
    log_mac_debug(des_addr);

    struct network_event evt;
    struct network_receive_event *recv_evt = &evt.data.receive_event;
    evt.event_id = NETWORK_EVENT_RECEIVE;
    memcpy(recv_evt->mac_addr, src_addr, ESP_NOW_ETH_ALEN);
    recv_evt->data = malloc(len); // this will be need to be freed by function that processes the queue
    if (recv_evt->data == NULL) {
        ESP_LOGE(LOG_TAG, "malloc receive data fail");
        return;
    }
    memcpy(recv_evt->data, data, len);
    recv_evt->data_len = len;
    if (xQueueSend(network_event_queue, &evt, NETWORK_EVENT_QUEUE_MAX_DELAY) != pdTRUE) {
        ESP_LOGW(LOG_TAG, "send receive queue fail");
        free(recv_evt->data);
    }
}

/**
 * @brief callback triggered when an esp-now message is sent, create event and place on queue for processing
 * @param mac_addr - byte array contains mac address of destination
 * @param status - esp_now_send_status_t status of sent message
 */
static void network_send_callback(const uint8_t *mac_addr, const esp_now_send_status_t status) {
    if (mac_addr == NULL) {
        ESP_LOGE(LOG_TAG, "send call back argument error.");
        return;
    }

    struct network_event event;
    event.event_id = NETWORK_EVENT_SEND;
    struct network_send_event *send_event = &event.data.send_event;
    send_event->status = status;
    memcpy(send_event->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);

    if (xQueueSend(network_event_queue, &event, NETWORK_EVENT_QUEUE_MAX_DELAY) != pdTRUE) {
        ESP_LOGW(LOG_TAG, "send callback queue send failed.");
    }
}

///////////////////////////////////////////////////////////////////////////////
// ESP-NOW NETWORK
///////////////////////////////////////////////////////////////////////////////

/**
 *  @brief add the given mac address to the esp_now peer list
 *  @param mac_addr - byte array that holds mac address to add to the peer list
 *  @returns 0 on success other on failure
 */
static int network_add_peer(const uint8_t *mac_addr) {
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
static int network_start() {
    network_event_queue = xQueueCreate(NETWORK_EVENT_QUEUE_SIZE, sizeof(struct network_event));
    if (network_event_queue == NULL) {
        ESP_LOGE(LOG_TAG, "Create queue fail");
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(network_receive_callback) );
    ESP_ERROR_CHECK( esp_now_register_send_cb(network_send_callback) );

    // add broadcast mac to list of peers so we can use it for bootstrapping
    if (network_mode == NETWORK_CONTROLLER_MODE)
        ESP_ERROR_CHECK(network_add_peer(network_broadcast_mac));

    // start in register mode
    network_conn_state = NETWORK_CONN_STATE_REG;

    return ESP_OK;
}

/**
 * stop esp now service
 */
static void network_stop() {
    esp_now_deinit();
    if (network_event_queue != NULL) {
        vQueueDelete(network_event_queue);
        network_event_queue = NULL;
    }
}

void network_task(void *pvParameter) {
    // TODO: get network mode and client id from pvParameter
    wifi_start();
    ESP_LOGI(LOG_TAG, "WIFI interface started in station mode at 2.4G!");

    network_start();
    ESP_LOGI(LOG_TAG, "ESP-NOW network started!");

//graceful_exit: // cleanup
    // free(msg_buffer);
    network_stop();
    ESP_LOGI(LOG_TAG, "ESP_NOW network stopped!");
    wifi_stop();
    ESP_LOGI(LOG_TAG, "WIFI interface stopped!");
    vTaskDelete(NULL); // kill the task
}