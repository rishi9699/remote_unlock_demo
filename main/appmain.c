#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include <stdlib.h>
#include "esp_mac.h"
#include <time.h>

#include "wifiandtime.h"
#include "passwords.h"
#include "blemanager.h"

#include "data_parser.h"

#include "esp_sleep.h"
//#include "driver/gpio.h"

uint8_t mac[6];
char macId[13];
uint8_t provFlag = 0;  //store in NVS
uint8_t devLinkFlag = 0; //store in NVS
uint8_t mqttFlag = 1;
uint8_t wifiFlag = 1;

TaskHandle_t  uarttask = NULL;

void app_main()
{
    esp_deep_sleep_enable_gpio_wakeup(32, ESP_GPIO_WAKEUP_GPIO_HIGH);
    
    if(check_reset_init_nvs())
    {
        initiate_uart();
        xTaskCreate(rx_task, "uart_rx_task", 8192, NULL, configMAX_PRIORITIES-1, &uarttask);
        //comm_initiator();
        //write_reset_flag();
    }
    else
    {
        initiate_uart();
        xTaskCreate(rx_task, "uart_rx_task", 8192, NULL, configMAX_PRIORITIES-1, &uarttask);
    };
    
    esp_efuse_mac_get_default(mac);
    sprintf(macId, "%02x%02x%02x%02x%02x%02x", *mac, *(mac+1), *(mac+2), *(mac+3), *(mac+4), *(mac+5));
    
    startBleProcess();
    wifi_init_global();
    //ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    start_wifi(0);
};