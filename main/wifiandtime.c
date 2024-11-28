#include "wifiandtime.h"

#define MAXIMUM_AP 5
static const char * TAG = "WIFI";
static esp_err_t err;

extern uint8_t devLinkFlag;
extern uint8_t mqttFlag;
extern nvs_handle_t wifi_handle;

size_t ssidlen = 0;
uint8_t ssid_len_uint8 = 0;
size_t pswdlen = 0;
uint8_t pswd_len_uint8 = 0;

char wifiSSID[32] = {0};
char wifiPSWD[64] = {0};

bool wifiConnected = false;

esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;

#define WIFI_SSID "Wifi_name"
#define WIFI_PASS "WIfi_password"


static unsigned char connected_to_internet = 0;

// uint8_t time_synced = 0;

void time_sync_callbk()
{
    
}

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        wifiConnected = true;
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI("wifi", "retrying...");
        connected_to_internet = 0;
        esp_wifi_connect();
        mqttFlag = 0;
        wifiConnected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI("wifi", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sntp_setoperatingmode(SNTP_OPMODE_POLL);  // Poll mode
        sntp_setservername(0, "pool.ntp.org");    // NTP server
        sntp_set_time_sync_notification_cb(time_sync_callbk);
        sntp_init();
        connected_to_internet = 1;
        mqttFlag = 1;
    }
};


void wifi_init_global(){
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "WiFi inited");
}

void start_wifi(uint8_t forScan){
    if(forScan == 0){
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));
    }
    wifi_config_t wifi_config = {
        .sta = {
        },
    };
    err = nvs_flash_init();
    if(err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    err = nvs_open("wifiCred", NVS_READWRITE, &wifi_handle);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Error (%s) opening NVS wifi_handle!\n", esp_err_to_name(err));
    } 
    else{   
        if(forScan == 0){ 
            err = nvs_get_u8(wifi_handle, "ssidLen", &ssid_len_uint8);
            switch(err){
                case ESP_OK:
                    ESP_LOGI(TAG, "ssidlen = %d\n", ssid_len_uint8);
                    break;
                case ESP_ERR_NVS_NOT_FOUND:
                    ESP_LOGE(TAG, "The value is not initialized yet!\n");
                    break;
                default :
                    ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
            }
            err = nvs_get_u8(wifi_handle, "pswdLen", &pswd_len_uint8);
            switch(err){
                case ESP_OK:
                    ESP_LOGI(TAG, "pswdlen = %d\n", pswd_len_uint8);
                    break;
                case ESP_ERR_NVS_NOT_FOUND:
                    ESP_LOGE(TAG, "The value is not initialized yet!\n");
                    break;
                default :
                    ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
            }
            ssid_len_uint8++;
            pswd_len_uint8++;
            ssidlen = ssid_len_uint8;
            pswdlen = pswd_len_uint8;
            ESP_LOGI(TAG, "ssidlen = %d\n", ssidlen);
            err = nvs_get_str(wifi_handle, "ssid", wifiSSID, &ssidlen);
            ESP_LOGI(TAG, "err* wifiSSID = %d\n", err);
            switch(err){
                case ESP_OK:
                    //ESP_LOGI(TAG, "wifiSSID = %s\n", &wifiSSID);
                    break;
                case ESP_ERR_NVS_NOT_FOUND:
                    ESP_LOGE(TAG, "The value is not initialized yet!\n");
                    break;
                default :
                    ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
            }
            err = nvs_get_str(wifi_handle, "password", wifiPSWD, &pswdlen);
            ESP_LOGI(TAG, "err* wifiPSWD = %d\n", err);
            switch(err){
                case ESP_OK:
                    //ESP_LOGI(TAG, "wifiPSWD = %s\n", &wifiPSWD);
                    break;
                case ESP_ERR_NVS_NOT_FOUND:
                    ESP_LOGE(TAG, "The value is not initialized yet!\n");
                    break;
                default :
                    ESP_LOGE(TAG, "Error (%s) reading!\n", esp_err_to_name(err));
            }

            //strcpy((char *)wifi_config.sta.ssid, wifiSSID);
            //strcpy((char *)wifi_config.sta.password, wifiPSWD); 
            strcpy((char *)wifi_config.sta.ssid, WIFI_SSID);
            strcpy((char *)wifi_config.sta.password, WIFI_PASS);   
            ESP_LOGI(TAG, "Setting WiFi configuration SSID = %s", wifi_config.sta.ssid);
            ESP_LOGI(TAG, "Setting WiFi configuration PSWD = %s", wifi_config.sta.password);

        }
        ESP_LOGI(TAG, "Before WiFi started");
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
        ESP_LOGI(TAG, "WiFi started");
        
        ESP_LOGI(TAG, "here\n");
    }
}

void stop_wifi(){
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    esp_wifi_disconnect();
    esp_wifi_stop();
}

void wifi_scan(wifi_ap_record_t *wifi_records)
{
   stop_wifi();
   start_wifi(1);
   wifi_scan_config_t scan_config = {
    .ssid = 0,
    .bssid = 0,
    .channel = 0,
    .show_hidden = 0};
    
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t max_records = MAXIMUM_AP;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&max_records, wifi_records));
    esp_wifi_scan_stop();
    stop_wifi();
    if(devLinkFlag == 1){start_wifi(0);}
    //return wifi_records;
}
