#include "string.h"
#include "data_parser.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include <stdlib.h>
#include "passwords.h"
#include <time.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "esp_sleep.h"

#define RX_BUF_SIZE 256

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

#define GET_LOCAL_TIME 0x06
#define REPORT_RECORD_TYPE_DATA 0x08
#define MCU_POWER_OFF_NOTIFICATION 0x22
#define GET_THE_GMT 0x10
#define REPORT_REAL_TIME_STATUS 0x05
#define REPORT_NETWORK_STATUS 0x02
#define MODULE_SENDS_COMMAND 0x09
#define COMPARE_PASSWORD 0x16
#define FACTORY_RESET 0X34
#define RESET_WIFI 0x03
#define TRIGGER_CAPTURING 0x64

#define DATA_HEADER 0xaa55

extern uint8_t time_synced;

// List of DP begins


void initiate_uart(void)
{
    const uart_config_t uart_config =
    {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);   //use queue??
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
};

void store_battery_level(uint8_t);
extern uint8_t battery_level;

static void dp_parser(const uint8_t *const data, const int dp_start_index, const int dp_end_index, char* const log_str_buffer)
{
    //printf("%d\n\n\n", dp_end_index);
    printf("inside 0x08 with dp:%hhx\n", data[dp_start_index]);
    switch (data[dp_start_index])
    {
        case 0x0B:  //battery level DP 11
        {
            if(data[dp_end_index]!=battery_level)
            {
                store_battery_level(data[dp_end_index]);
            };
            //sprintf(log_str_buffer, "battery level %x", data[dp_end_index]);
            break;
        };

        default:
        break;

    };
};

static void report_record_type_data(const uint8_t *const data)
{
    uint16_t data_length = *(data+4);
    data_length = data_length<<8;
    data_length |= *(data+5);

    const int dp_end_index = 5 + data_length;
    char log_str_buffer[50];
    dp_parser(data, 13, dp_end_index, log_str_buffer);
    
    unsigned char response[] = {0x55, 0xAA, 0x00, 0x08, 0x00, 0x01, 0x00, 0x08};  //check docs for retained/unretained data to be reported
    uart_write_bytes(UART_NUM_1, response, 8);

};

void remote_unlock_request()
{
    unsigned char response[] = {0x55,0xAA,0x00,0x16,0x00,0x13,0x00,0x01,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x39};
    uart_write_bytes(UART_NUM_1, response, 26);
    printf("Success\n"); 
    // ADD TO LOGS
};

static uint8_t checksum_calculator(uint8_t *const data, const int length)
{
    int check_sum=255;  //since sum of the header is 255

    for(int i=length-2; i>1; --i)
    {
        check_sum += data[i];
    }
    
    return (uint8_t)(check_sum%256);
}

static void get_local_time()
{
    time_t now;
    struct tm localtimedata;
    localtimedata.tm_year = 0;
    time(&now);
    localtime_r(&now, &localtimedata);

    if(localtimedata.tm_year>100)
    {
        printf("Current local time: %s", asctime(&localtimedata));
        uint8_t tx_data[] = {0x55, 0xaa, 0x00, 0x06, 0x00, 0x08, 0x01, localtimedata.tm_year -100, localtimedata.tm_mon+1, localtimedata.tm_mday, localtimedata.tm_hour, localtimedata.tm_min, localtimedata.tm_sec, ((localtimedata.tm_wday==0) ? 7 : localtimedata.tm_wday), 0};
        tx_data[14] = checksum_calculator(tx_data, 15);
        const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 15);
        //ESP_LOG_BUFFER_HEXDUMP("LOG", tx_data, 15, ESP_LOG_INFO);
    }
    else
    {
        uint8_t tx_data[] = {0x55, 0xaa, 0x00, 0x06, 0x00, 0x08, 0x00, 0, 0, 0, 0, 0, 0, 0 ,0x0d};
        const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 15);
    }    
};

static void get_gmt_time()
{
    time_t now;
    struct tm gmttimedata;
    gmttimedata.tm_year = 0;
    time(&now);
    gmtime_r(&now, &gmttimedata);

    if(gmttimedata.tm_year>100)
    {
        printf("Current UTC time: %s", asctime(&gmttimedata));
        uint8_t tx_data[] = {0x55, 0xaa, 0x00, 0x10, 0x00, 0x08, 0x01, gmttimedata.tm_year -100, gmttimedata.tm_mon+1, gmttimedata.tm_mday, gmttimedata.tm_hour, gmttimedata.tm_min, gmttimedata.tm_sec, ((gmttimedata.tm_wday==0) ? 7 : gmttimedata.tm_wday), 0};
        tx_data[14] = checksum_calculator(tx_data, 15);
        const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 15);
        //ESP_LOG_BUFFER_HEXDUMP("LOG", tx_data, 15, ESP_LOG_INFO);
    }
    else
    {
        uint8_t tx_data[] = {0x55, 0xaa, 0x00, 0x10, 0x00, 0x08, 0x00, 0, 0, 0, 0, 0, 0, 0 ,0x17};
        const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 15);
    }
};

static void mcu_power_off_notification()
{
    // disconnect from wifi
    uint8_t tx_data[] = {0x55, 0xaa, 0x00, 0x22, 0x00, 0x01, 0x00, 0x22}; //last byte is checksum
    const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 8);
    esp_deep_sleep_start();
    // where to write the reconnect on wake-up algorthm?
};

extern TaskHandle_t  uarttask;

static void factory_reset(void)
{
    nvs_flash_erase();
    esp_restart();
};

static void reset_wifi(void)
{
    comm_initiator();
};

static void trigger_capturing(void)
{
    uint8_t tx_data[] = {0x55, 0xaa, 0x00, 0x64, 0x00, 0x01, 0x01, 0x65};  // failed to capture video
    const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 8);
};

void write_reset_flag();

void parse_received_data(const uint8_t *const data, const int length)
{
    switch (data[3])
    {
        case REPORT_RECORD_TYPE_DATA:
        report_record_type_data(data);
        printf("Report record type data\n");
        break;

        case REPORT_REAL_TIME_STATUS:
        printf("report_real_time_status\n");
        if(data[6]==49)
        {
            printf("Remote Unlock Key requested\n");
            // Hardocoded Key; Check its validity
            uint8_t tx_data[] = {0x55,0xAA,0x00,0x09,0x00,0x19,0x31,0x00,0x00,0x15,0x01,0x00,0x01,0x66,0xB1,0xC4,0x49,0x72,0xBC,0x9B,0x7F,0xFF,0xFF,0x36,0x30,0x39,0x39,0x38,0x36,0x31,0x37,0x81};
            const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 32);
            write_reset_flag();
        }
        else
        {
            ;
        }
        break;

        case MODULE_SENDS_COMMAND:
        printf("module_sends_command\n");
        break;

        case GET_LOCAL_TIME:
        printf("get_local_time\n");
        get_local_time();
        break;

        case GET_THE_GMT:
        printf("get_gmt_time\n");
        get_gmt_time();
        break;

        case MCU_POWER_OFF_NOTIFICATION:
        printf("mcu_power_off_notification");
        mcu_power_off_notification();
        break;

        case FACTORY_RESET:
        printf("factory_reset");
        factory_reset();
        break;

        case RESET_WIFI:
        printf("reset_wifi\n");
        reset_wifi();
        break;

        case TRIGGER_CAPTURING:
        printf("trigger_capturing\n");
        //trigger_capturing();
        break;

        default:
        break;
    }
};
extern uint8_t reset_flag;

void comm_initiator()
{
    uint8_t tx_data[] = {0x55,0xAA,0x00,0x02,0x00,0x01,0x07,0x09};
    const int txBytes = uart_write_bytes(UART_NUM_1, tx_data, 8);
    vTaskDelay(pdMS_TO_TICKS(50));
    uint8_t tx_data2[] = {0x55,0xAA,0x00,0x02,0x00,0x01,0x04,0x06};
    const int txBytes2 = uart_write_bytes(UART_NUM_1, tx_data2, 8);
};

void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);

    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);

    while (1) {
        //printf("Waiting for data\n");
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, pdMS_TO_TICKS(1000)); //why 1s ?
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes", rxBytes);
            ESP_LOG_BUFFER_HEX_LEVEL(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);

            uint16_t message_begin_index = 0;
            uint16_t message_end_index = 0;

            while((rxBytes-message_begin_index) >= 7) // 7 is the minimum length of a valid message
            {
            if(data[message_begin_index]==0x55)
            {
                uint16_t data_length = *(data+message_begin_index+4);
                data_length = data_length<<8;
                data_length |= *(data+message_begin_index+5);

                uint16_t message_end_index = 5 + message_begin_index + data_length;
                ++message_end_index; //including checksum

                //printf("%hhx is checksum result; %hhx is the real checksum\n", checksum_calculator(data+message_begin_index, 7+data_length), data[message_end_index]);
                
                if(0xaa55==(*((uint16_t*)(data+message_begin_index))) && checksum_calculator(data+message_begin_index, 7+data_length)==data[message_end_index])
                parse_received_data(data+message_begin_index, 7+data_length);
                
                printf("MESSAGE PARSED\n");
            
                message_begin_index = message_end_index+1;

            }

            else{
                while((data[++message_begin_index]!=0x55) && (message_begin_index<rxBytes))
                ;
                // find out 0x55 in the data stream
            }

            };
        }
    }
    free(data);
};