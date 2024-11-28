#include <stdio.h>
#include <string.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <time.h>
#include <esp_system.h>
#include <pthread.h>

static nvs_handle_t passwordhandle;
static esp_err_t err;

uint8_t battery_level;
uint8_t reset_flag = 0;

static void load_passwords_from_nvs()
{

    err = nvs_get_u8(passwordhandle, "battery", &battery_level);
    if (err == ESP_OK) {
        printf("Blob read from NVS successfully!\n");
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("The key does not exist in NVS yet!\n");
    } else {
        printf("Failed to read blob from NVS!\n");
    }

};

static void init_nvs() {
    err = nvs_flash_init();
    if(err==ESP_OK)
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated or is of a newer version
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
};

int check_reset_init_nvs()
{
    init_nvs();
    err = nvs_open("pwdmgr", NVS_READWRITE, &passwordhandle);
    if (err != ESP_OK) {
        printf("Error opening NVS handle!\n");
        return 0;
    };

    err = nvs_get_u8(passwordhandle, "reset_flag", &reset_flag);
    if (err == ESP_OK) {
        printf("Reset flag found in NVS successfully!\n");
        load_passwords_from_nvs();
        return 0;
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        load_passwords_from_nvs();
        return 1;
    } else {
        printf("Failed to read blob from NVS!\n");
        esp_restart();
        return 0;
    };
    //nvs_close(passwordhandle);
};

void store_battery_level(uint8_t bat_lev)
{
    battery_level = bat_lev;
    nvs_set_u8(passwordhandle, "battery", battery_level); // add ERR check here (in all set/get NVS commands)
    nvs_commit(passwordhandle);
};

void write_reset_flag()
{
    reset_flag = 1;
    nvs_set_u8(passwordhandle, "reset_flag", reset_flag); // add ERR check here (in all set/get NVS commands)
    nvs_commit(passwordhandle);
};