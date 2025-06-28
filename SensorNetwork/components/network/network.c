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

struct network_msg_header{
    uint32_t flags; // bit field (type, broadcast/unicast, msg length)
    uint16_t magic;
    uint16_t crc;
};

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "sensor_network"; //!< char pointer - logging group

static uint8_t wifi_mac[ESP_NOW_ETH_ALEN]; //!< byte array - holds mac address for this node

static const uint8_t network_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //!< byte array contains broadcast mac address
static size_t network_largest_message_size = sizeof(struct network_msg_header); //!< size_t, largest message size used to allocate a message buffer
static uint8_t *msg_buffer; //!< byte array used for message buffer
static QueueHandle_t network_event_queue = NULL; //!< QueueHandle_t esp_now queue that will hold esp_now network events to be processed
static uint8_t network_mode = 0; //!< uint8_t what mode is node controller or client
static uint8_t network_client_id = 0; //!< uint8_t what is the client id for this node
static uint8_t network_conn_state = NETWORK_CONN_STATE_REG; //!< uint8_t what is the current connection state, register, register-ack, or ready
static uint8_t network_client_states[NETWORK_CLIENT_COUNT]; //!< uint8_t array, connection state for each client used by the controller
static uint8_t network_client_macs[NETWORK_CLIENT_COUNT][ESP_NOW_ETH_ALEN]; //!< 2 dimensional byte array containing client mac addresses

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

static uint8_t find_client_id_with_mac(const uint8_t *mac_addr) {
    for (uint8_t i = 0; i < NETWORK_CLIENT_COUNT; i++) {
        const int result = memcmp(mac_addr, network_client_macs[i], ESP_NOW_ETH_ALEN);
        ESP_LOGI(LOG_TAG, "mac memory compare result: (%d) %d", i, result);
        if (result == 0)
            return i;
    }
    return NETWORK_CLIENT_COUNT;
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
    if (network_mode == NETWORK_MODE_CONTROLLER)
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

///////////////////////////////////////////////////////////////////////////////
// NETWORK MESSAGE METHODS
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// NETWORK QUEUE PROCESSORS
///////////////////////////////////////////////////////////////////////////////

static int esp_now_controller_process() {
    struct network_event event;
    while (xQueueReceive(network_event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(LOG_TAG, "Received event: %d", event.event_id);
        if (event.event_id == NETWORK_EVENT_SEND) {
            const bool is_broadcast = memcmp(event.data.send_event.mac_addr, network_broadcast_mac, ESP_NOW_ETH_ALEN) == 0;
            uint8_t client_id = 0; // what client did we send this message to
            if (!is_broadcast)
                client_id = find_client_id_with_mac(event.data.send_event.mac_addr); // find the client with the associated mac address
            if (event.data.send_event.status == ESP_NOW_SEND_SUCCESS) {
                if (is_broadcast)
                    ESP_LOGD(LOG_TAG, "broadcast message sent successfully!");
                else {
                    // if we send a reg-ack to a client, and it's successful the connection to that client is ready
                    if (client_id < NETWORK_CLIENT_COUNT && network_client_states[client_id] == NETWORK_CONN_STATE_REG_ACK) {
                        network_client_states[client_id] = NETWORK_CONN_STATE_READY;
                        ESP_LOGI(LOG_TAG, "client id, %d is ready!", client_id);
                    }
                }
            } else { // send was a failure, the destination didn't acknowledge
                // if there is a failure, and it's not a broadcast message, and it's a reg-ack, resend
                if (!is_broadcast && client_id < NETWORK_CLIENT_COUNT && network_client_states[client_id] == NETWORK_CONN_STATE_REG_ACK) {
                    send_registration_ack_message(event.data.send_event.mac_addr, NETWORK_CLIENT_CONTROLLER);
                }
            }
        } else if (event.event_id == ESP_NOW_EVENT_RECEIVE) {
            // verify message header CRC and magic
            const struct esp_now_receive_event *receive_event = &event.data.receive_event;
            struct esp_now_msg_header *hdr;
            if (esp_now_parse_message(receive_event->data, receive_event->data_len, &hdr) != ESP_OK)
                continue;
            // if reg-ack message does it matter what state we are in no we'll just respond no matter what
            if (get_message_type(hdr) == ESP_NOW_MSG_TYPE_REG_ACK) {
                const uint8_t client_id = get_message_client_id(hdr);
                memcpy(client_macs[client_id], receive_event->mac_addr, ESP_NOW_ETH_ALEN); // save mac for the given client
                add_peer(receive_event->mac_addr);
                // send reg-ack message to client completing registration
                if (send_registration_ack_message(receive_event->mac_addr, ESP_NOW_CONTROLLER_CLIENT) == ESP_OK)
                    client_states[client_id] = ESP_NOW_CONN_STATE_REG_ACK; // update client state to reg-ack
            }
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

void network_task(void *pvParameter) {
    struct network_config *ncfg = (struct network_config *)pvParameter;
    network_client_id = ncfg->client_id;
    network_mode = ncfg->mode;

    wifi_start();
    ESP_LOGI(LOG_TAG, "WIFI interface started in station mode at 2.4G!");

    network_start();
    ESP_LOGI(LOG_TAG, "ESP-NOW network started!");

    // log configuration information
    if (network_mode == NETWORK_MODE_CONTROLLER) {
        ESP_LOGI(LOG_TAG, "Network controller started");
        network_client_id = NETWORK_CLIENT_CONTROLLER; // hard code the client id if in controller mode
    } else {
        ESP_LOGI(LOG_TAG, "Network client started");
    }

    // log client id information
    switch (network_client_id) {
        case NETWORK_CLIENT_BATTERY:
            ESP_LOGI(LOG_TAG, "Network battery client!");
            break;
        case NETWORK_CLIENT_COOLANT:
            ESP_LOGI(LOG_TAG, "Network coolant client!");
            break;
        case NETWORK_CLIENT_VCU:
            ESP_LOGI(LOG_TAG, "Network VCU client!");
            break;
        case NETWORK_CLIENT_CONTROLLER:
            break;
        default:
            ESP_LOGE(LOG_TAG, "Unknown client id: %d", network_client_id);
            goto graceful_exit;
    }

    // initialize variables
    for (uint8_t i = 0; i < NETWORK_CLIENT_COUNT; i++) {
        network_client_states[i] = NETWORK_CONN_STATE_REG;
    }

    msg_buffer = malloc(network_largest_message_size);
    if (msg_buffer == NULL) {
        ESP_LOGE(LOG_TAG, "message buffer allocation failed!");
        goto graceful_exit;
    }
    memset(msg_buffer, 0, network_largest_message_size);

    int result = ESP_OK;

    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        // TODO: Check if someone says we should shutdown

        // if (esp_now_mode == ESP_NOW_CLIENT_MODE)
        //     result = esp_now_client_process();
        if (network_mode == NETWORK_MODE_CONTROLLER)
            result = esp_now_controller_process();

        // TODO: Change led color based on connection status, need to create led component to do this properly

        if (result != ESP_OK) { // catastrophic failure try restart?
            break;
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

graceful_exit: // cleanup
    free(msg_buffer);
    network_stop();
    ESP_LOGI(LOG_TAG, "ESP_NOW network stopped!");
    wifi_stop();
    ESP_LOGI(LOG_TAG, "WIFI interface stopped!");
    vTaskDelete(NULL); // kill the task
}