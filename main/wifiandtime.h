#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_attr.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

void wifi_init_global();
void start_wifi(uint8_t forScan);
void stop_wifi();
void wifi_scan(wifi_ap_record_t *wifi_records);