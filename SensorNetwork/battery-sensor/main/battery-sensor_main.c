#include <esp_log.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include "can_network.h"
#include "network.h"

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "bsen_main"; //!< char pointer - logging group
#define CAN_MESSAGE_QUEUE_SIZE                  200
#define LOOP_SLEEP_TIME_MS                      50
#define LOOPS_BETWEEN_MCU_CONFIG_REQUEST        600
#define LOOPS_BETWEEN_MCU_STATUS_REQUEST        40
#define MCU_STATUS_REFRESH_RATE                 0x14  // 0x14 is a one-second rate 20 * 50ms = 1000ms 0x0A is a half second rate 10 * 50ms = 500ms
#define MCU_MSG_CONFIG_REPLY                    0x193 //403
#define MCU_MSG_SUMMARY_REPLY                   0x293
#define MCU_MSG_CELL_MAP_SUMMARY_REPLY          0x393
#define MCU_MSG_CONFIG_OPTIONS                  0
#define MCU_MSG_CONFIG_CHARGE_OPTIONS_ONE       1
#define MCU_MSG_CONFIG_CHARGE_OPTIONS_TWO       2
#define MCU_MSG_CONFIG_CHARGE_OPTIONS_THREE     3
#define MCU_MSG_CONFIG_CHARGE_OPTIONS_FOUR      4
#define MCU_MSG_CONFIG_CHARGE_OPTIONS_FIVE      5
#define MCU_MSG_CONFIG_CHARGE_OPTIONS_SIX       6
#define MCU_MSG_CONFIG_MCU_CONFIG               16 // 0x10
#define MCU_MSG_CONFIG_BMS_OPTIONS_ONE          17 // 0x11
#define MCU_MSG_CONFIG_BMS_OPTIONS_TWO          18 // 0x12
#define MCU_MSG_CONFIG_GROUP_MAP_ONE            24 // 0x18
#define MCU_MSG_CONFIG_GROUP_MAP_TWO            25 // 0x19
#define MCU_MSG_CONFIG_GROUP_MAP_THREE          26 // 0x18
#define MCU_STATUS_SUMMARY_BYTE                 1
#define MCU_STATUS_PACK_SUMMARY_BYTE            2
#define MCU_STATUS_CELL_VOLTAGE_SUMMARY_BYTE    3
#define MCU_STATUS_THERMISTOR_SUMMARY_BYTE      4
#define MCU_STATUS_STATE_OF_CHARGE_BYTE         5
#define MCU_STATUS_CELL_MAP_SUMMARY_BYTE        6
#define MCU_STATUS_SUMMARY_REPLY                1
#define MCU_STATUS_PACK_SUMMARY_REPLY           2
#define MCU_STATUS_CELL_VOLTAGE_SUMMARY_REPLY   3
#define MCU_STATUS_THERMISTOR_SUMMARY_REPLY     4
#define MCU_STATUS_STATE_OF_CHARGE_REPLY        5

///////////////////////////////////////////////////////////////////////////////
// COMPONENT DATA TYPES
///////////////////////////////////////////////////////////////////////////////
struct mcu_summary {
    uint16_t charge_kw;
    uint8_t charge_state;
    uint8_t plug_state;
    uint16_t alerts;
    uint16_t pack_voltage;
    uint16_t pack_current;
    uint8_t cell_count;
    uint16_t cell_voltage_low;
    uint16_t cell_voltage_mean;
    uint16_t cell_voltage_high;
    uint8_t thermistor_count;
    uint16_t thermistor_min;
    uint16_t thermistor_max;
    uint16_t thermistor_low;
    uint16_t thermistor_high;
    uint8_t state_of_charge;
    uint16_t pack_kwh;
    uint16_t pack_max_kwh;
    uint64_t cell_group_0; // byte 0 - id, 1 - cm_12, 2 - cm_0/1, 3 - cm_2/3, 4 - cm_4/5, 5 - cm_6/7, 6 - cm_8/9, 7 - cm_10/11
    uint64_t cell_group_1;
    uint64_t cell_group_2;
    uint64_t cell_group_3;
    uint64_t cell_group_4;
    uint64_t cell_group_5;
    uint64_t cell_group_6;
    uint64_t cell_group_7;
    uint64_t cell_group_8;
    uint64_t cell_group_9;
    uint64_t cell_group_10;
    uint64_t cell_group_11;
    uint64_t cell_group_12;
    uint64_t cell_group_13;
    uint64_t cell_group_14;
    uint64_t cell_group_15;
};

struct mcu_config {
    uint16_t evcc_options;
    uint16_t bms_options;
    uint16_t inst_options;
    uint8_t charge_one_type;
    uint8_t charge_two_type;
    uint8_t charge_three_type;
    uint8_t charge_four_type;
    uint16_t charge_termination_current;
    uint16_t charge_max_voltage;
    uint16_t charge_max_current;
    uint16_t charge_max_time;
    uint16_t charge_line_voltage;
    uint16_t charge_line_current;
    uint16_t charge_wait_time;
    uint8_t charge_one_phase;
    uint8_t charge_two_phase;
    uint8_t charge_three_phase;
    uint8_t charge_four_phase;
    uint8_t version;
    uint8_t revision;
    uint8_t arch;
    uint16_t bms_high_voltage_cutoff;
    uint16_t bms_low_voltage_cutoff;
    uint16_t bms_discharge_balancing_voltage_min;
    uint16_t bms_high_voltage_cutoff_clear;
    uint16_t bms_low_voltage_cutoff_clear;
    uint8_t bms_high_voltage_cutoff_delay;
    uint8_t bms_low_voltage_cutoff_delay;
    uint8_t group_0;
    uint8_t group_1;
    uint8_t group_2;
    uint8_t group_3;
    uint8_t group_4;
    uint8_t group_5;
    uint8_t group_6;
    uint8_t group_7;
    uint8_t group_8;
    uint8_t group_9;
    uint8_t group_10;
    uint8_t group_11;
    uint8_t group_12;
    uint8_t group_13;
    uint8_t group_14;
    uint8_t group_15;
};

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static QueueHandle_t can_message_queue = NULL; //!< QueueHandle_t esp_now queue that will hole can messages to be processed
static struct mcu_config mcu_config;

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
// UTILS
///////////////////////////////////////////////////////////////////////////////
uint16_t bytes_to_short(const uint8_t byte_one, const uint8_t byte_two) {
    uint16_t short_one = byte_one;
    uint16_t short_two = byte_two;
    return short_one + (short_two << 8);
}

///////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
// MCU COMMUNICATIONS
///////////////////////////////////////////////////////////////////////////////
void request_mcu_config(int request_type) {
    uint8_t msg_byte = 0x00;
    if (request_type == 1)
        can_network_send_message(0x213, &msg_byte);
    if (request_type == 2) {
        msg_byte = 0x01;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 3) {
        msg_byte = 0x02;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 4) {
        msg_byte = 0x03;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 5) {
        msg_byte = 0x04;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 6) {
        msg_byte = 0x10;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 7) {
        msg_byte = 0x11;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 8) {
        msg_byte = 0x12;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 9) {
        msg_byte = 0x18;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 10) {
        msg_byte = 0x19;
        can_network_send_message(0x213, &msg_byte);
    }
    if (request_type == 11) {
        msg_byte = 0x1a;
        can_network_send_message(0x213, &msg_byte);
    }
}

void request_mcu_status(int request_type) {
    uint8_t bytes[8];
    bytes[0] = 0x00;
    bytes[1] = 0x00;
    bytes[2] = 0x00;
    bytes[3] = 0x00;
    bytes[4] = 0x00;
    bytes[5] = 0x00;
    bytes[6] = 0x00;
    bytes[7] = 0x00;

    if (request_type == MCU_STATUS_SUMMARY_BYTE) {
        bytes[MCU_STATUS_SUMMARY_BYTE] = MCU_STATUS_REFRESH_RATE;
        can_network_send_message(0x313, bytes);
    }
    if (request_type == MCU_STATUS_PACK_SUMMARY_BYTE) {
        bytes[MCU_STATUS_PACK_SUMMARY_BYTE] = MCU_STATUS_REFRESH_RATE;
        can_network_send_message(0x313, bytes);
    }
    if (request_type == MCU_STATUS_CELL_VOLTAGE_SUMMARY_BYTE) {
        bytes[MCU_STATUS_CELL_VOLTAGE_SUMMARY_BYTE] = MCU_STATUS_REFRESH_RATE;
        can_network_send_message(0x313, bytes);
    }
    if (request_type == MCU_STATUS_THERMISTOR_SUMMARY_BYTE) {
        bytes[MCU_STATUS_THERMISTOR_SUMMARY_BYTE] = MCU_STATUS_REFRESH_RATE;
        can_network_send_message(0x313, bytes);
    }
    if (request_type == MCU_STATUS_STATE_OF_CHARGE_BYTE) {
        bytes[MCU_STATUS_STATE_OF_CHARGE_BYTE] = MCU_STATUS_REFRESH_RATE;
        can_network_send_message(0x313, bytes);
    }
    if (request_type == MCU_STATUS_CELL_MAP_SUMMARY_BYTE) {
        bytes[MCU_STATUS_CELL_MAP_SUMMARY_BYTE] = MCU_STATUS_REFRESH_RATE;
        can_network_send_message(0x313, bytes);
    }
}

void parse_mcu_message(struct can_message *msg) {
    if (msg->frame.header.dlc < 8)
        return;
    if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_OPTIONS) {
        mcu_config.evcc_options = bytes_to_short(msg->frame.buffer[2], msg->frame.buffer[3]);
        mcu_config.bms_options = bytes_to_short(msg->frame.buffer[4], msg->frame.buffer[5]);
        mcu_config.inst_options = bytes_to_short(msg->frame.buffer[6], msg->frame.buffer[7]);
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_CHARGE_OPTIONS_ONE) {
        mcu_config.charge_one_type = msg->frame.buffer[2];
        mcu_config.charge_two_type = msg->frame.buffer[3];
        mcu_config.charge_three_type = msg->frame.buffer[4];
        mcu_config.charge_four_type = msg->frame.buffer[5];
        mcu_config.charge_termination_current = bytes_to_short(msg->frame.buffer[6], msg->frame.buffer[7]);
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_CHARGE_OPTIONS_TWO) {
        mcu_config.charge_max_voltage = bytes_to_short(msg->frame.buffer[2], msg->frame.buffer[3]);
        mcu_config.charge_max_current = bytes_to_short(msg->frame.buffer[4], msg->frame.buffer[5]);
        mcu_config.charge_max_time = bytes_to_short(msg->frame.buffer[6], msg->frame.buffer[7]);
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_CHARGE_OPTIONS_THREE) {
        mcu_config.charge_line_voltage = bytes_to_short(msg->frame.buffer[2], msg->frame.buffer[3]);
        mcu_config.charge_line_current = bytes_to_short(msg->frame.buffer[4], msg->frame.buffer[5]);
        mcu_config.charge_wait_time = bytes_to_short(msg->frame.buffer[6], msg->frame.buffer[7]);
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_CHARGE_OPTIONS_FOUR) {
        mcu_config.charge_one_phase = msg->frame.buffer[2];
        mcu_config.charge_two_phase = msg->frame.buffer[3];
        mcu_config.charge_three_phase = msg->frame.buffer[4];
        mcu_config.charge_four_phase = msg->frame.buffer[5];
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_MCU_CONFIG) {
        mcu_config.version = msg->frame.buffer[2];
        mcu_config.revision = msg->frame.buffer[3];
        mcu_config.arch = msg->frame.buffer[4];
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_BMS_OPTIONS_ONE) {
        mcu_config.bms_high_voltage_cutoff = bytes_to_short(msg->frame.buffer[2], msg->frame.buffer[3]);
        mcu_config.bms_low_voltage_cutoff = bytes_to_short(msg->frame.buffer[4], msg->frame.buffer[5]);
        mcu_config.bms_discharge_balancing_voltage_min = bytes_to_short(msg->frame.buffer[6], msg->frame.buffer[7]);
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_BMS_OPTIONS_TWO) {
        mcu_config.bms_high_voltage_cutoff_clear = bytes_to_short(msg->frame.buffer[2], msg->frame.buffer[3]);
        mcu_config.bms_low_voltage_cutoff_clear = bytes_to_short(msg->frame.buffer[4], msg->frame.buffer[5]);
        mcu_config.bms_high_voltage_cutoff_delay = msg->frame.buffer[6];
        mcu_config.bms_low_voltage_cutoff_delay = msg->frame.buffer[7];
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_GROUP_MAP_ONE) {
        mcu_config.group_0 = msg->frame.buffer[2];
        mcu_config.group_1 = msg->frame.buffer[3];
        mcu_config.group_2 = msg->frame.buffer[4];
        mcu_config.group_3 = msg->frame.buffer[5];
        mcu_config.group_4 = msg->frame.buffer[6];
        mcu_config.group_5 = msg->frame.buffer[7];
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_GROUP_MAP_TWO) {
        mcu_config.group_6 = msg->frame.buffer[2];
        mcu_config.group_7 = msg->frame.buffer[3];
        mcu_config.group_8 = msg->frame.buffer[4];
        mcu_config.group_9 = msg->frame.buffer[5];
        mcu_config.group_10 = msg->frame.buffer[6];
        mcu_config.group_11 = msg->frame.buffer[7];
    }
    else if (msg->frame.header.id == MCU_MSG_CONFIG_REPLY && msg->frame.buffer[0] == MCU_MSG_CONFIG_GROUP_MAP_THREE) {
        mcu_config.group_12 = msg->frame.buffer[2];
        mcu_config.group_13 = msg->frame.buffer[3];
        mcu_config.group_14 = msg->frame.buffer[4];
        mcu_config.group_15 = msg->frame.buffer[5];
    }
}

/**
 * @brief setup can interface and start monitoring a can bus
 * @param pvParameter void pointer will eventually contain any can bus config
 */
void battery_sensor_task(void *pvParameter) {
    can_message_queue = xQueueCreate(CAN_MESSAGE_QUEUE_SIZE, sizeof(struct can_message));
    if (can_message_queue == NULL) {
        ESP_LOGE(LOG_TAG, "Create queue fail");
        goto graceful_exit;
    }

    struct can_network_config network_config;
    network_config.can_message_queue = &can_message_queue;
    can_network_start(&network_config);

    // create a queue to hold CAN messages
    // create call back on receive CAN message place it on the queue
    // check queue for un-processed messages and process them
    // keep internal data structure representing battery status
    // Every second send requests to get required MCU data
    // place battery update message on queue for ESPNOW task
    struct can_message *msg = NULL;
    uint16_t loops_since_last_config_request = 0;
    uint16_t loops_since_last_summary_request = 0;
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        while (xQueueReceive(can_message_queue, &msg, 0) == pdTRUE) {
            if (msg != NULL) {
                ESP_LOGI(LOG_TAG, "RX: %x [%d] %x %x %x %x %x %x %x %x",
                         msg->frame.header.id,
                         msg->frame.header.dlc,
                         msg->frame.buffer[0],
                         msg->frame.buffer[1],
                         msg->frame.buffer[2],
                         msg->frame.buffer[3],
                         msg->frame.buffer[4],
                         msg->frame.buffer[5],
                         msg->frame.buffer[6],
                         msg->frame.buffer[7]);
                parse_mcu_message(msg);
                free(msg);
                msg = NULL;
            }
            // ESP_LOGI(LOG_TAG, "MCU Version: %d", mcu_config.version);
            // ESP_LOGI(LOG_TAG, "MCU Revision: %d", mcu_config.revision);
            // ESP_LOGI(LOG_TAG, "MCU Arch: %d", mcu_config.arch);
        }
        loops_since_last_summary_request++;
        if (loops_since_last_summary_request > LOOPS_BETWEEN_MCU_STATUS_REQUEST) {
            request_mcu_status(loops_since_last_summary_request - LOOPS_BETWEEN_MCU_STATUS_REQUEST);
            if (loops_since_last_summary_request == LOOPS_BETWEEN_MCU_STATUS_REQUEST + 6)
                loops_since_last_summary_request = 0;
        }
        loops_since_last_config_request++;
        if (loops_since_last_config_request > LOOPS_BETWEEN_MCU_CONFIG_REQUEST) {
            request_mcu_config(loops_since_last_config_request - LOOPS_BETWEEN_MCU_CONFIG_REQUEST);
            if (loops_since_last_config_request == LOOPS_BETWEEN_MCU_CONFIG_REQUEST + 11)
                loops_since_last_config_request = 0;
        }
        vTaskDelay(LOOP_SLEEP_TIME_MS / portTICK_PERIOD_MS);
    }

graceful_exit: // cleanup
    can_network_stop();
    if (can_message_queue != NULL) {
        vQueueDelete(can_message_queue);
        can_message_queue = NULL;
    }
    vTaskDelete(NULL); // kill the task
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

    struct network_config ncfg;
    ncfg.mode = NETWORK_MODE_CLIENT;
    ncfg.client_id = NETWORK_CLIENT_BATTERY;

    // TODO: Setup queue to communicate between tasks, the can task will need to send messages on the esp-now network
    // the esp-now task will need to tell can task to shutdown
    // can task will need to tell esp-now task to shutdown

    TaskHandle_t networkTaskHandle = NULL;
    xTaskCreate(network_task, "network_task", 8192, &ncfg, 4, &networkTaskHandle);

    TaskHandle_t batterySensorTaskHandle = NULL;
    xTaskCreate(battery_sensor_task, "bsen_task", 8192, NULL, 4, &batterySensorTaskHandle);
}
