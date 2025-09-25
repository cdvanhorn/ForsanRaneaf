#include "network.h"

#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_crc.h"

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////
#define WIFI_PRIMARY_CHANNEL                    10

#define NETWORK_EVENT_QUEUE_SIZE                50
#define NETWORK_EVENT_QUEUE_MAX_DELAY           512

#define NETWORK_CONN_STATE_REG                  0
#define NETWORK_CONN_STATE_REG_ACK              1
#define NETWORK_CONN_STATE_READY                2

#define NETWORK_EVENT_RECEIVE                   0
#define NETWORK_EVENT_SEND                      1

#define NETWORK_MSG_BROADCAST                   0
#define NETWORK_MSG_UNICAST                     1

#define NETWORK_MSG_TYPE_REG                    0
#define NETWORK_MSG_TYPE_REG_ACK                1

///////////////////////////////////////////////////////////////////////////////
// COMPONENT DATA TYPES
///////////////////////////////////////////////////////////////////////////////

/**
 * @struct network_send_event
 * @brief representation of a send event placed on event queue during esp-now send callback
 */
struct network_send_event{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN]; //!< mac address message was sent to
    esp_now_send_status_t status; //!< status of the sent message
};

/**
 * @struct network_receive_event
 * @brief representation of a receive event placed on event queue during esp-now receive callback
 */
struct network_receive_event {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN]; //!< mac address of the receive dmessage
    uint8_t *data; //!< byte array contains the received message bytes
    uint16_t data_len; //!< number of bytes in the received message
};

/**
 * @union network_event_data
 * @brief either a receive event or a send event
 */
union network_event_data {
    struct network_send_event send_event; //!< will be populated if this is a send event
    struct network_receive_event receive_event; //!< will be populated if this is a receive event
};

/**
 * @struct network_event
 * @brief representation of network event placed on event queue after a send or receive callback is triggered
 */
struct network_event {
    uint8_t event_type_id; //!< what type of event is this so know how to interpret the event data
    union network_event_data data; //!< will either be a receive event or send event based on event_id
};

/**
 * @struct network_msg_header
 * @brief message header for esp-now messages
 */
struct network_msg_header{
    uint32_t flags; //!< bit field (broadcast/unicast, type, client id, and msg length)
    uint16_t magic; //!< magic number used for simple message verification
    uint16_t crc; //!< crc checksum of the whole message including header
};

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "sensor_network"; //!< char pointer - logging group

static uint8_t wifi_mac[ESP_NOW_ETH_ALEN]; //!< byte array - holds mac address for this node

static const uint8_t network_broadcast_mac[ESP_NOW_ETH_ALEN] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; //!< byte array contains broadcast mac address
static uint8_t network_msg_magic = 132;
static size_t network_largest_msg_size = sizeof(struct network_msg_header); //!< size_t, largest message size used to allocate a message buffer
static size_t network_reg_ack_msg_size = sizeof(struct network_msg_header); //!< size_t, sizes of registration ack message
static size_t network_reg_msg_size = sizeof(struct network_msg_header); //!< size_t, sizes of registration broadcast message
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

/**
 * @brief log a mac address to the debug log
 * @param mac_addr - byte array hold mac address to log as a debug message
 */
static void log_mac_debug(const uint8_t *mac_addr) {
    ESP_LOGD(LOG_TAG, "%02x:%02x:%02x:%02x:%02x:%02x",
        mac_addr[0],
        mac_addr[1],
        mac_addr[2],
        mac_addr[3],
        mac_addr[4],
        mac_addr[5]);
}

/**
 * @brief Find the client id with the given mac address
 * @param mac_addr byte array holds mac address to find a client id for
 * @return uint8_t client id for corresponding mac address or client count if not found
 */
static uint8_t find_client_id_with_mac(const uint8_t *mac_addr) {
    for (uint8_t i = 0; i < NETWORK_CLIENT_COUNT; i++) {
        const int result = memcmp(mac_addr, network_client_macs[i], ESP_NOW_ETH_ALEN);
        ESP_LOGI(LOG_TAG, "mac memory compare result: (%d) %d", i, result);
        if (result == 0)
            return i;
    }
    return NETWORK_CLIENT_COUNT;
}

/**
 * @brief have all the clients checked in with the controller
 * @return bool true if all the clients have checked in
 */
static bool all_clients_checked_in() {
    bool all_checked_in = true;
    for (uint8_t i = 0; i < NETWORK_CLIENT_COUNT; i++) {
        if (network_client_states[i] != NETWORK_CONN_STATE_READY) {
            all_checked_in = false;
            break;
        }
    }
    return all_checked_in;
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
    evt.event_type_id = NETWORK_EVENT_RECEIVE;
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
 * @param tx_info - structure containing transmission information
 * @param status - esp_now_send_status_t status of sent message
 */
static void network_send_callback(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    if (tx_info == NULL) {
        ESP_LOGE(LOG_TAG, "send call back argument error.");
        return;
    }

    struct network_event event;
    event.event_type_id = NETWORK_EVENT_SEND;
    struct network_send_event *send_event = &event.data.send_event;
    send_event->status = status;
    memcpy(send_event->mac_addr, tx_info->des_addr, ESP_NOW_ETH_ALEN);

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

/**
 * @brief get the message type id for the given header
 * @param hdr - pointer to network_msg_hdr header to get the message type of
 * @return uint8_t id for the message type for the given header
 */
static uint8_t get_msg_type(const struct network_msg_header *hdr) {
    uint32_t type_mask = 15; // 0000 0000 0000 0000 0000 0000 0000 1111
    type_mask <<= 27; //        0111 1000 0000 0000 0000 0000 0000 0000
    const uint32_t type = (hdr->flags & type_mask) >> 27;
    return (uint8_t)type;
}

static uint8_t get_msg_client_id(const struct network_msg_header *hdr) {
    uint32_t type_mask = 15; // 0000 0000 0000 0000 0000 0000 0000 1111
    type_mask <<= 23; //        0000 0111 1000 0000 0000 0000 0000 0000
    const uint32_t client_id = (hdr->flags & type_mask) >> 23;
    return (uint8_t)client_id;
}

/**
 * @brief parse a message, check the CRC and that it contains the correct magic number
 * @param msg uint8_t pointer to byte array contains the message bytes
 * @param msg_len size_t, how many bytes in the message
 * @param hdr network_msg_header pointer will contain pointer to header after parsing
 * @return 0 on success -1 on failure
 */
static int parse_msg(const uint8_t *msg, const size_t msg_len, struct network_msg_header **hdr) {
    if (msg_len < sizeof(struct network_msg_header))
        return ESP_FAIL;

    *hdr = (struct network_msg_header *)msg;

    // is it magical
    if ((*hdr)->magic != network_msg_magic) {
        ESP_LOGW(LOG_TAG, "message has wrong magic.");
        return ESP_FAIL;
    }

    // CRC check
    const uint16_t crc = (*hdr)->crc;
    (*hdr)->crc = 0;
    const uint16_t crc_cal = esp_crc16_le(UINT16_MAX, msg, msg_len);

    ESP_LOGD(LOG_TAG, "CRC: 0x%04X", crc);
    ESP_LOGD(LOG_TAG, "CRC CALC: 0x%04X", crc_cal);
    if (crc_cal != crc) {
        ESP_LOGW(LOG_TAG, "CRC CHECK FAILED: 0x%04X 0x%04X", crc, crc_cal);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief generate the flag bitfield for a message
 * @param protocol uint8_t is this a broadcast or unicast message
 * @param msg_type uint8_t what type of message is this
 * @return uint32_t flags to place in your message
 */
static uint32_t build_msg_flags(const uint8_t protocol, const uint8_t msg_type) {
    // |0                |0000        |0000     |00000000   |000000000000000|
    // |broadcast/unicast|message type|client id|msg_len    |unused         |
    uint32_t flags = protocol;
    flags <<= 4; // width of message type 4 bits
    flags = flags | msg_type;
    flags <<= 4; // width of client id
    flags |= network_client_id;
    flags <<= 8; // width of message length
    flags |= 0; // unicast register ack message contains no data
    flags <<= 15; // left over bits
    return flags;
}

/**
 * @brief send a registration-ack message to the given mac address
 * @param mac_addr uint8_t byte array mac address to send message to
 * @return 0 on success 1 on failure result of esp_now_send
 */
static int send_registration_ack_message(const uint8_t *mac_addr) {
    const uint32_t flags = build_msg_flags(NETWORK_MSG_UNICAST, NETWORK_MSG_TYPE_REG_ACK);
    struct network_msg_header *hdr = (struct network_msg_header *)msg_buffer;
    hdr->flags = flags;
    hdr->magic = network_msg_magic;
    hdr->crc = 0;
    hdr->crc = esp_crc16_le(UINT16_MAX, msg_buffer, network_reg_ack_msg_size);
    const esp_err_t result = esp_now_send(mac_addr, msg_buffer, network_reg_ack_msg_size);
    ESP_LOGI(LOG_TAG, "register-ack message sent: %d", result);
    return result;
}

/**
 * @brief send a register broadcast message
 * @return 0 on success 1 on failure result of esp_now_send
 */
static int send_broadcast_register_message() {
    const uint32_t flags = build_msg_flags(NETWORK_MSG_BROADCAST, NETWORK_MSG_TYPE_REG);
    struct network_msg_header *hdr = (struct network_msg_header *)msg_buffer;
    hdr->flags = flags;
    hdr->magic = network_msg_magic;
    hdr->crc = 0;
    hdr->crc = esp_crc16_le(UINT16_MAX, msg_buffer, network_reg_msg_size);
    const esp_err_t result = esp_now_send(network_broadcast_mac, msg_buffer, network_reg_msg_size);
    ESP_LOGI(LOG_TAG, "Broadcast register message sent: %d", result);
    return result;
}

///////////////////////////////////////////////////////////////////////////////
// NETWORK QUEUE PROCESSORS
///////////////////////////////////////////////////////////////////////////////

/**
 * @brief process the event queue as a controller
 */
static void network_controller_process() {
    struct network_event event;
    while (xQueueReceive(network_event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(LOG_TAG, "Received event: %d", event.event_type_id);
        if (event.event_type_id == NETWORK_EVENT_SEND) {
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
                    send_registration_ack_message(event.data.send_event.mac_addr);
                }
            }
        } else if (event.event_type_id == NETWORK_EVENT_RECEIVE) {
            const struct network_receive_event *receive_event = &event.data.receive_event;
            struct network_msg_header *hdr;
            if (parse_msg(receive_event->data, receive_event->data_len, &hdr) != ESP_OK) {
                free(receive_event->data);
                continue;
            }
            // if reg-ack message does it matter what state we are in, no we'll just respond no matter what
            if (get_msg_type(hdr) == NETWORK_MSG_TYPE_REG_ACK) {
                const uint8_t client_id = get_msg_client_id(hdr);
                memcpy(network_client_macs[client_id], receive_event->mac_addr, ESP_NOW_ETH_ALEN); // save mac for the given client
                network_add_peer(receive_event->mac_addr);
                // send reg-ack message to client completing registration
                if (send_registration_ack_message(receive_event->mac_addr) == ESP_OK)
                    network_client_states[client_id] = NETWORK_CONN_STATE_REG_ACK; // update client state to reg-ack
            }
            free(receive_event->data);
        }
    }

    if (network_conn_state == NETWORK_CONN_STATE_REG) {
        if (all_clients_checked_in())
            network_conn_state = NETWORK_CONN_STATE_READY;
        else
            send_broadcast_register_message();
    }
}

/**
 * @brief process the event queue as a client
 */
static void network_client_process() {
    struct network_event event;
    while (xQueueReceive(network_event_queue, &event, 0) == pdTRUE) {
        ESP_LOGI(LOG_TAG, "Received event: %d", event.event_type_id);
        if (event.event_type_id == NETWORK_EVENT_SEND) {
            if (event.data.send_event.status == ESP_NOW_SEND_SUCCESS) {
                ESP_LOGI(LOG_TAG, "message sent successfully!");
            } else {
                if (network_conn_state == NETWORK_CONN_STATE_REG_ACK) {
                    ESP_LOGI(LOG_TAG, "register ack message failed to send, reset state to register");
                    network_conn_state = NETWORK_CONN_STATE_REG;
                }
            }
        } else if (event.event_type_id == NETWORK_EVENT_RECEIVE) {
            const struct network_receive_event *receive_event = &event.data.receive_event;
            struct network_msg_header *hdr;
            if (parse_msg(receive_event->data, receive_event->data_len, &hdr) != ESP_OK) {
                free(receive_event->data);
                continue;
            }
            if (network_conn_state == NETWORK_CONN_STATE_REG) {
                if (get_msg_type(hdr) == NETWORK_MSG_TYPE_REG) {
                    network_add_peer(receive_event->mac_addr);
                    if (send_registration_ack_message(receive_event->mac_addr) == ESP_OK)
                        network_conn_state = NETWORK_CONN_STATE_REG_ACK;
                }
            } else if (network_conn_state == NETWORK_CONN_STATE_REG_ACK) {
                // make sure this is a register-ack message type
                // change connection state to ready
                if (get_msg_type(hdr) == NETWORK_MSG_TYPE_REG_ACK && get_msg_client_id(hdr) == NETWORK_CLIENT_CONTROLLER) {
                    network_conn_state = NETWORK_CONN_STATE_READY;
                    ESP_LOGI(LOG_TAG, "connection to controller ready!");
                }
            } else if (network_conn_state == NETWORK_CONN_STATE_READY) {
                // take action based on the message type
            }
            free(receive_event->data);
        }
    }
}


/**
 * @brief setup network node and begin handling network events
 * @param pvParameter void pointer should point to a network_config struct to configure the network node
 */
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

    msg_buffer = malloc(network_largest_msg_size);
    if (msg_buffer == NULL) {
        ESP_LOGE(LOG_TAG, "message buffer allocation failed!");
        goto graceful_exit;
    }
    memset(msg_buffer, 0, network_largest_msg_size);

    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        // TODO: Check if someone says we should shutdown

        if (network_mode == NETWORK_MODE_CLIENT)
            network_client_process();
        if (network_mode == NETWORK_MODE_CONTROLLER)
            network_controller_process();

        // TODO: Figure out how much memory we're using
        // uxTaskGetStackHighWaterMark(NULL)

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