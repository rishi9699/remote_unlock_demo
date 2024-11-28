#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "cJSON.h"
#include <pthread.h>

#include "esp_log.h"
#include "nvs_flash.h"
/* BLE */
#include "bleprph.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "services/ans/ble_svc_ans.h"

#include "mbedtls/aes.h"
#include "mbedtls/base64.h"

#include "blemanager.h"

#include "wifiandtime.h"
#include "passwords.h"

#define MAXIMUM_AP 5

static const char *TAG = "ble";
nvs_handle_t wifi_handle;

static cJSON *dataBlecjson;

static int bleprph_gap_event(struct ble_gap_event *event, void *arg);
static uint8_t own_addr_type;

extern uint8_t provFlag;
extern uint8_t devLinkFlag;
uint8_t recentlyReceivedWiFiCreds = 0;
char bleName[60] = {0};
char wifi_scan_ssid[200] = {0};

static pthread_mutex_t bleMutex;

int16_t conn_handle1 = -1;
int16_t conn_handle2 = -1;
int16_t conn_handle3 = -1;
int16_t conn_handle4 = -1;
uint16_t hrs_hrm_handle;
static esp_err_t err;
extern char macId[];
extern uint8_t battery_level;

void remote_unlock_request(void); //temporarily placed

static const ble_uuid128_t gatt_svr_svc_control_config_uuid =
    BLE_UUID128_INIT(0xbe, 0x48, 0x12, 0x2e, 0x6a, 0x9b, 0xa4, 0xa4, 0xa6, 0x46, 0xb3, 0x85, 0x8a, 0xcc, 0x56, 0x92);

// e29ee02c-af3d-11ec-b909-0242ac120002
static const ble_uuid128_t gatt_svr_chr_lock_control_uuid =
    BLE_UUID128_INIT(0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0x09, 0xb9, 0xec, 0x11, 0x3d, 0xaf, 0x2c, 0xe0, 0x9e, 0xe2);

static const ble_uuid128_t gatt_svr_chr_lock_config_uuid =
    BLE_UUID128_INIT(0xaa, 0xb2, 0x0d, 0xb6, 0x7e, 0x77, 0x12, 0xa1, 0xa9, 0x43, 0x7a, 0x8a, 0x6f, 0x39, 0xbd, 0xf1);

// 9fa1610e-e6fc-11ec-8fea-0242ac120002
static const ble_uuid128_t gatt_svr_chr_lock_analytics_uuid =
    BLE_UUID128_INIT(0x02, 0x00, 0x12, 0xac, 0x42, 0x02, 0xea, 0x8f, 0xec, 0x11, 0xfc, 0xe6, 0x0e, 0x61, 0xa1, 0x9f);

// 36684e6a-df79-4e4b-b031-0620ecf10cae
static const ble_uuid128_t gatt_svr_chr_wifi_config_uuid =
    BLE_UUID128_INIT(0xae, 0x0c, 0xf1, 0xec, 0x20, 0x06, 0x31, 0xb0, 0x4b, 0x4e, 0x79, 0xdf, 0x6a, 0x4e, 0x68, 0x36);

// 9d1619e6-7130-4aec-a63e-f1ab2bb55fbd
static const ble_uuid128_t gatt_svr_chr_wifi_status_uuid =
    BLE_UUID128_INIT(0xbd, 0x5f, 0xb5, 0x2b, 0xab, 0xf1, 0x3e, 0xa6, 0xec, 0x4a, 0x30, 0x71, 0xe6, 0x19, 0x16, 0x9d);

static int
gatt_svr_chr_access_config_control(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt,
                                   void *arg);


static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Lock control and config */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_control_config_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){{
                                                           /*** Characteristic: Control */
                                                           .uuid = &gatt_svr_chr_lock_control_uuid.u,
                                                           .access_cb = gatt_svr_chr_access_config_control,
                                                           .val_handle = &hrs_hrm_handle,
                                                           .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY // Read and write together is important for pairing
                                                       },
                                                       {
                                                           /*** Characteristic: Config */
                                                           .uuid = &gatt_svr_chr_lock_config_uuid.u,
                                                           .access_cb = gatt_svr_chr_access_config_control,
                                                           .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE // Read and write together is important for pairing
                                                       },
                                                       {/*** Characteristic: Analytics */
                                                        .uuid = &gatt_svr_chr_lock_analytics_uuid.u,
                                                        .access_cb = gatt_svr_chr_access_config_control,
                                                        .flags = BLE_GATT_CHR_F_READ},
                                                       {/*** Characteristic: Wifi Scan */
                                                        .uuid = &gatt_svr_chr_wifi_config_uuid.u,
                                                        .access_cb = gatt_svr_chr_access_config_control,
                                                        .flags = BLE_GATT_CHR_F_READ},
                                                       {/*** Characteristic: Wifi Status*/
                                                        .uuid = &gatt_svr_chr_wifi_status_uuid.u,
                                                        .access_cb = gatt_svr_chr_access_config_control,
                                                        .flags = BLE_GATT_CHR_F_READ},
                                                       {
                                                           0, /* No more characteristics in this service. */
                                                       }},
    },

    {
        0, /* No more services. */
    },
};

static const struct ble_gatt_svc_def gatt_svr_svcs_wifi_on[] = {
    {
        /*** Service: Lock control only, no config */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_control_config_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){{
                                                           /*** Characteristic: Control*/
                                                           .uuid = &gatt_svr_chr_lock_control_uuid.u,
                                                           .access_cb = gatt_svr_chr_access_config_control,
                                                           .val_handle = &hrs_hrm_handle,
                                                           .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_NOTIFY // Read and write together is important for pairing
                                                       },
                                                       {/*** Characteristic: Analytics*/
                                                        .uuid = &gatt_svr_chr_lock_analytics_uuid.u,
                                                        .access_cb = gatt_svr_chr_access_config_control,
                                                        .flags = BLE_GATT_CHR_F_READ},
                                                       {/*** Characteristic: Wifi Scan */
                                                        .uuid = &gatt_svr_chr_wifi_config_uuid.u,
                                                        .access_cb = gatt_svr_chr_access_config_control,
                                                        .flags = BLE_GATT_CHR_F_READ},
                                                       {/*** Characteristic: Wifi Status*/
                                                        .uuid = &gatt_svr_chr_wifi_status_uuid.u,
                                                        .access_cb = gatt_svr_chr_access_config_control,
                                                        .flags = BLE_GATT_CHR_F_READ},
                                                       {
                                                           0, /* No more characteristics in this service. */
                                                       }},
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_chr_access_config_control(uint16_t conn_handle, uint16_t attr_handle,
                                   struct ble_gatt_access_ctxt *ctxt,
                                   void *arg)
{
    const ble_uuid_t *uuid;
    // int rand_num;
    int rc;

    uuid = ctxt->chr->uuid;
    uint8_t fullData[300] = {0};
    int om_len = OS_MBUF_PKTLEN(ctxt->om);
    ESP_LOGI(TAG, "Received length: %d\n", om_len);

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
    {
        // Flatten the buffer to get the full data
        uint16_t fullLength = OS_MBUF_PKTLEN(ctxt->om);
        if (ble_hs_mbuf_to_flat(ctxt->om, &fullData, fullLength, NULL) != 0)
        {
            ESP_LOGE(TAG, "Failed to get all write data");
            return 255;
        }

        ESP_LOGI(TAG, "Received data: %s\n", (char *)fullData);
        if (fullData[0] == '{')
        { // JSON opening brace
            dataBlecjson = cJSON_Parse((char *)fullData);
        }
        else
        {
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_lock_control_uuid.u) == 0)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
        {
            ESP_LOGI(TAG, "Within config event\n");

            if (cJSON_GetObjectItem(dataBlecjson, "command"))
            {
                char *recvdcmd = cJSON_GetStringValue(cJSON_GetObjectItem(dataBlecjson, "command"));
                if (0 == strcmp(recvdcmd, "battery_level"))
                {
                    printf("Battery Level: %d\n", battery_level);
                    // send battery level from variable
                }
                else if (0 == strcmp(recvdcmd, "remote_unlock"))
                {
                    remote_unlock_request();
                }
            }
            else if (cJSON_GetObjectItem(dataBlecjson, "read"))
            {
                // notify_over_ble();
            }
            else if (cJSON_GetObjectItem(dataBlecjson, "ssid"))
            {
                handle_wifi_creds(dataBlecjson, true);
            }

            cJSON_Delete(dataBlecjson);

            return 0;
        }
        else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            ESP_LOGI(TAG, "Received read req on control char");
            bool tempdata = 0;
            rc = os_mbuf_append(ctxt->om, &tempdata, 1);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_lock_config_uuid.u) == 0)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR)
        {
            ESP_LOGI(TAG, "Within config event\n");

            handle_wifi_creds(dataBlecjson, false);
            cJSON_Delete(dataBlecjson);
            return 0;
        }
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_lock_analytics_uuid.u) == 0)
    {
        ESP_LOGI(TAG, "Received analytics char");
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            ESP_LOGI(TAG, "Analytics info nothing\n");
            bool tempdata = 0;
            rc = os_mbuf_append(ctxt->om, &tempdata, 1);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_wifi_config_uuid.u) == 0)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            if (om_len == 0 && strlen(wifi_scan_ssid) > 0)
            {
                // setWiFiScanTime();
                rc = os_mbuf_append(ctxt->om, wifi_scan_ssid, strlen(wifi_scan_ssid));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            else
            {
                memset(wifi_scan_ssid, 0, sizeof(wifi_scan_ssid));
                ESP_LOGI(TAG, "Received read req");
                wifi_ap_record_t wifi_records[MAXIMUM_AP];
                wifi_scan(wifi_records);
                for (int i = 0; i < MAXIMUM_AP; i++)
                {
                    char rssi[10] = {0};
                    sprintf(rssi, "%i", wifi_records[i].rssi);

                    strcat(wifi_scan_ssid, (char *)wifi_records[i].ssid);
                    strcat(wifi_scan_ssid, " (");
                    strcat(wifi_scan_ssid, rssi);
                    strcat(wifi_scan_ssid, ")");
                    strcat(wifi_scan_ssid, "\n");
                }
                ESP_LOGI(TAG, "Wifi networks scanned %s\n", wifi_scan_ssid);
                rc = os_mbuf_append(ctxt->om, wifi_scan_ssid, strlen(wifi_scan_ssid));
                return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
            }
        }
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_wifi_status_uuid.u) == 0)
    {
        if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR)
        {
            bool tempdata = 0;
            rc = os_mbuf_append(ctxt->om, &tempdata, 1);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
    }

    return BLE_ATT_ERR_UNLIKELY;
}

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op)
    {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d\n",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with "
                      "def_handle=%d val_handle=%d\n",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle,
                 ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d\n",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    if ((provFlag == 1) && (devLinkFlag == 1))
    {
        rc = ble_gatts_count_cfg(gatt_svr_svcs_wifi_on);
    }
    else
    {
        rc = ble_gatts_count_cfg(gatt_svr_svcs);
    }
    if (rc != 0)
    {
        return rc;
    }

    if ((provFlag == 1) && (devLinkFlag == 1))
    {
        rc = ble_gatts_add_svcs(gatt_svr_svcs_wifi_on);
    }
    else
    {
        rc = ble_gatts_add_svcs(gatt_svr_svcs);
    }
    if (rc != 0)
    {
        return rc;
    }

    return 0;
};

void ble_store_config_init(void);

/**
 * Logs information about a connection to the console.
 */
static void
bleprph_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    ESP_LOGI(TAG, "handle=%d our_ota_addr_type=%d our_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->conn_handle, desc->our_ota_addr.type,
             desc->our_ota_addr.val[5],
             desc->our_ota_addr.val[4],
             desc->our_ota_addr.val[3],
             desc->our_ota_addr.val[2],
             desc->our_ota_addr.val[1],
             desc->our_ota_addr.val[0]);

    ESP_LOGI(TAG, "our_id_addr_type=%d our_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->our_id_addr.type,
             desc->our_id_addr.val[5],
             desc->our_id_addr.val[4],
             desc->our_id_addr.val[3],
             desc->our_id_addr.val[2],
             desc->our_id_addr.val[1],
             desc->our_id_addr.val[0]);

    ESP_LOGI(TAG, "peer_ota_addr_type=%d peer_ota_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_ota_addr.type,
             desc->peer_ota_addr.val[5],
             desc->peer_ota_addr.val[4],
             desc->peer_ota_addr.val[3],
             desc->peer_ota_addr.val[2],
             desc->peer_ota_addr.val[1],
             desc->peer_ota_addr.val[0]);

    ESP_LOGI(TAG, "peer_id_addr_type=%d peer_id_addr=%02x:%02x:%02x:%02x:%02x:%02x",
             desc->peer_id_addr.type,
             desc->peer_id_addr.val[5],
             desc->peer_id_addr.val[4],
             desc->peer_id_addr.val[3],
             desc->peer_id_addr.val[2],
             desc->peer_id_addr.val[1],
             desc->peer_id_addr.val[0]);

    ESP_LOGI(TAG, "conn_itvl=%d conn_latency=%d supervision_timeout=%d "
                  "encrypted=%d authenticated=%d bonded=%d",
             desc->conn_itvl, desc->conn_latency,
             desc->supervision_timeout,
             desc->sec_state.encrypted,
             desc->sec_state.authenticated,
             desc->sec_state.bonded);
};

/**
 * Enables advertising with the following parameters:
 *     o General discoverable mode.
 *     o Undirected connectable mode.
 */
static void
bleprph_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    const char *name;
    int rc;

    /**
     *  Set the advertisement data included in our advertisements:
     *     o Flags (indicates advertisement type and other general info).
     *     o Advertising tx power.
     *     o Device name.
     *     o 16-bit service UUIDs (alert notifications).
     */

    memset(&fields, 0, sizeof fields);

    /* Advertise two flags:
     *     o Discoverability in forthcoming advertisement (general)
     *     o BLE-only (BR/EDR unsupported).
     */
    fields.flags = BLE_HS_ADV_F_DISC_GEN |
                   BLE_HS_ADV_F_BREDR_UNSUP;

    /* Indicate that the TX power level field should be included; have the
     * stack fill this value automatically.  This is done by assigning the
     * special value BLE_HS_ADV_TX_PWR_LVL_AUTO.
     */
    fields.tx_pwr_lvl_is_present = 0;
    // fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    ESP_LOGI(TAG, "Name is %s\n", (char *)name);
    ESP_LOGI(TAG, "Name length is %d\n", strlen(name));
    fields.name_is_complete = 1;

    fields.uuids16 = (ble_uuid16_t[]){};
    fields.num_uuids16 = 0;
    fields.uuids16_is_complete = 0;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error setting advertisement data; rc=%d", rc);
        return;
    }

    /* Begin advertising. */
    memset(&adv_params, 0, sizeof adv_params);
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, bleprph_gap_event, NULL);

    ESP_LOGI(TAG, "BLE Task watermark: %d", uxTaskGetStackHighWaterMark(NULL));
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error enabling advertisement; rc=%d", rc);
        return;
    }
};

/**
 * The nimble host executes this callback when a GAP event occurs.  The
 * application associates a GAP event callback with each connection that forms.
 * bleprph uses the same callback for all connections.
 *
 * @param event                 The type of event being signalled.
 * @param ctxt                  Various information pertaining to the event.
 * @param arg                   Application-specified argument; unused by
 *                                  bleprph.
 *
 * @return                      0 if the application successfully handled the
 *                                  event; nonzero on failure.  The semantics
 *                                  of the return code is specific to the
 *                                  particular GAP event being signalled.
 */
static int
bleprph_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    int rc;

    switch (event->type)
    {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed. */
        ESP_LOGI(TAG, "connection %s; status=%d ",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0)
        {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            assert(rc == 0);
            bleprph_print_conn_desc(&desc);
        }

        if (event->connect.status != 0)
        {
            /* Connection failed; resume advertising. */
            bleprph_advertise();
        }
        // smart_indication();
        bleprph_advertise();
        if (conn_handle1 == -1)
            conn_handle1 = (int16_t)event->connect.conn_handle;
        else if (conn_handle2 == -1)
            conn_handle2 = (int16_t)event->connect.conn_handle;
        else if (conn_handle3 == -1)
            conn_handle3 = (int16_t)event->connect.conn_handle;
        else if (conn_handle4 == -1)
            conn_handle4 = (int16_t)event->connect.conn_handle;
        // notify_over_ble();
        // memset(analytics_info , 0, sizeof(analytics_info));

        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnect; reason=%d ", event->disconnect.reason);
        bleprph_print_conn_desc(&event->disconnect.conn);

        if ((&event->disconnect.conn)->conn_handle == (uint16_t)conn_handle1)
            conn_handle1 = -1;
        else if ((&event->disconnect.conn)->conn_handle == (uint16_t)conn_handle2)
            conn_handle2 = -1;
        else if ((&event->disconnect.conn)->conn_handle == (uint16_t)conn_handle3)
            conn_handle3 = -1;
        else if ((&event->disconnect.conn)->conn_handle == (uint16_t)conn_handle4)
            conn_handle4 = -1;

        // memset(analytics_info, 0, sizeof(analytics_info));
        /* Connection terminated; resume advertising. */
        bleprph_advertise();
        // smart_stop_indication();
        return 0;

    case BLE_GAP_EVENT_CONN_UPDATE:
        /* The central has updated the connection parameters. */
        ESP_LOGI(TAG, "connection updated; status=%d ",
                 event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "advertise complete; reason=%d",
                 event->adv_complete.reason);
        bleprph_advertise();
        return 0;

    case BLE_GAP_EVENT_ENC_CHANGE:
        /* Encryption has been enabled or disabled for this connection. */
        ESP_LOGI(TAG, "encryption change event; status=%d ",
                 event->enc_change.status);
        rc = ble_gap_conn_find(event->enc_change.conn_handle, &desc);
        assert(rc == 0);
        bleprph_print_conn_desc(&desc);
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d "
                      "reason=%d prevn=%d curn=%d previ=%d curi=%d",
                 event->subscribe.conn_handle,
                 event->subscribe.attr_handle,
                 event->subscribe.reason,
                 event->subscribe.prev_notify,
                 event->subscribe.cur_notify,
                 event->subscribe.prev_indicate,
                 event->subscribe.cur_indicate);
        // notify_over_ble();
        return 0;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "mtu update event; conn_handle=%d cid=%d mtu=%d",
                 event->mtu.conn_handle,
                 event->mtu.channel_id,
                 event->mtu.value);
        return 0;

    case BLE_GAP_EVENT_REPEAT_PAIRING:
        /* We already have a bond with the peer, but it is attempting to
         * establish a new secure link.  This app sacrifices security for
         * convenience: just throw away the old bond and accept the new link.
         */

        /* Delete the old bond. */
        rc = ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        assert(rc == 0);
        ble_store_util_delete_peer(&desc.peer_id_addr);

        /* Return BLE_GAP_REPEAT_PAIRING_RETRY to indicate that the host should
         * continue with the pairing operation.
         */
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    return 0;
}

static void
bleprph_on_reset(int reason)
{
    ESP_LOGE(TAG, "Resetting state; reason=%d", reason);
}

static void
bleprph_on_sync(void)
{
    int rc;

    rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    /* Figure out address to use while advertising (no privacy for now) */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "error determining address type; rc=%d", rc);
        return;
    }

    /* Printing ADDR */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);

    ESP_LOGI(TAG, "Device Address:%02x:%02x:%02x:%02x:%02x:%02x",
             addr_val[5],
             addr_val[4],
             addr_val[3],
             addr_val[2],
             addr_val[1],
             addr_val[0]);
    /* Begin advertising. */
    bleprph_advertise();
}

void bleprph_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    ESP_LOGI(TAG, "After nimble port run\n");
    nimble_port_freertos_deinit();
    ESP_LOGI(TAG, "After nimble port freertos deinit\n");
}

char *SSID;
char *PSWD;

void handle_wifi_creds(cJSON* ssidJson, bool restart){
    recentlyReceivedWiFiCreds = 1;
    if(cJSON_GetObjectItem(ssidJson, "ssid")){
        SSID = cJSON_GetObjectItem(ssidJson,"ssid")->valuestring;
        ESP_LOGI(TAG, "SSID=%s",SSID);
    }
    if(cJSON_GetObjectItem(ssidJson, "pswd")){
        PSWD = cJSON_GetObjectItem(ssidJson,"pswd")->valuestring;
        ESP_LOGI(TAG, "PSWD=%s",PSWD);
    }
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
        err = nvs_set_str(wifi_handle, "ssid", SSID);
        //ESP_LOGI(TAG, "err ssid = %d", err);
        err = nvs_set_str(wifi_handle, "password", PSWD);
        //ESP_LOGI(TAG, "err password = %d", err);
        err = nvs_set_u8(wifi_handle, "ssidLen", strlen(SSID));
        //ESP_LOGI(TAG, "err ssidLen = %d", err);
        err = nvs_set_u8(wifi_handle, "pswdLen", strlen(PSWD));
        err = nvs_commit(wifi_handle);
        //ESP_LOGI(TAG, "err pswdLen = %d", err);
        nvs_close(wifi_handle);
        if(restart){   
            esp_restart();
        } else{
            stop_wifi();
            start_wifi(0);
        }
    }
}

void startBleProcess(void)
{
    int rc;
    // ESP_LOGI(TAG, "[APP] Start of Main Free memory: %d bytes", esp_get_free_heap_size());

    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
    // ESP_LOGI(TAG, "[APP] Mid of Main Free memory: %d bytes", esp_get_free_heap_size());

    nimble_port_init();
    /* Initialize the NimBLE host configuration. */
    ble_hs_cfg.reset_cb = bleprph_on_reset;
    ble_hs_cfg.sync_cb = bleprph_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    rc = gatt_svr_init();
    assert(rc == 0);

    if (pthread_mutex_init(&bleMutex, NULL) != 0)
    {
        ESP_LOGI(TAG, "Failed to initialize mutex for ble");
    }

    /* Set the default device name. */
    if ((provFlag == 1) && (devLinkFlag == 1))
    {
        strcpy(bleName, "atomberg_"); // Device is linked
    }
    else
    {
        strcpy(bleName, "Atomberg_"); // Device is unlinked
    }
    strcat(bleName, "L1");
    strcat(bleName, "_");
    strcat(bleName, macId);

    printf("BLE name:%s\n", bleName);
    rc = ble_svc_gap_device_name_set(bleName);
    assert(rc == 0);

    /* XXX Need to have template for store */
    ble_store_config_init();

    nimble_port_freertos_init(bleprph_host_task);
    //ESP_LOGI(TAG, "[APP] End of Main Free memory: %d bytes", esp_get_free_heap_size());
};