#include <zephyr/types.h>
#include <zephyr.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/att.h>
#include <bluetooth/hci.h>
#include <bluetooth/conn.h>
#include <bluetooth/uuid.h>
#include <bluetooth/gatt.h>

#include <sys/byteorder.h>
#include <sys/printk.h>
#include <sys/slist.h>

#include <console/console.h>
#include <stddef.h>
#include <errno.h>
#include <stdint.h>

#define CFLAG(flag) static atomic_t flag = (atomic_t)false
#define SFLAG(flag) (void)atomic_set(&flag, (atomic_t)true)
#define UFLAG(flag) (void)atomic_set(&flag, (atomic_t)false)
#define WFLAG(flag) \
	while (!(bool)atomic_get(&flag)) { \
		(void)k_sleep(K_MSEC(1)); \
	}

#define BT_ATT_FIRST_ATTRIBUTE_HANDLE           0x0001
#define BT_ATT_FIRST_ATTTRIBUTE_HANDLE __DEPRECATED_MACRO BT_ATT_FIRST_ATTRIBUTE_HANDLE
#define BT_ATT_LAST_ATTRIBUTE_HANDLE            0xffff
#define BT_ATT_LAST_ATTTRIBUTE_HANDLE __DEPRECATED_MACRO BT_ATT_LAST_ATTRIBUTE_HANDLE

uint8_t test_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *buf, uint16_t size);
static void test_subscribed(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params);
static uint8_t discover(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params);
static void start_scan(void);

static uint16_t chrc_h;
static uint16_t notify_h;
static volatile size_t n_not;
static struct bt_conn *default_conn;
static struct bt_uuid_128 ble_upper = BT_UUID_INIT_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x00, 0x00);
static struct bt_uuid_128 ble_receive = BT_UUID_INIT_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03,0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0x00);
static struct bt_uuid_128 ble_notify = BT_UUID_INIT_128(0x01, 0x23, 0x45, 0x67, 0x89, 0x01, 0x02, 0x03,0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0x11);
static struct bt_uuid *primary_uuid = &ble_upper.uuid;
static struct bt_gatt_discover_params disc_params;
static struct bt_gatt_subscribe_params sub_params = {
	.notify = test_notify,
	.write = test_subscribed,
	.ccc_handle = 0,
	.disc_params = &disc_params,
	.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE,
	.value = BT_GATT_CCC_NOTIFY,
};

CFLAG(discover_f);
CFLAG(subscribed_f);
CFLAG(cwrite_f);

static void test_subscribed(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	if (err) printk("Subscribe failed (err %d)\n", err);

	SFLAG(subscribed_f);

	if (!params) 
    {
		printk("params NULL\n");
		return;
	}

	if (params->handle == notify_h) printk("Subscribed to characteristic\n");
	else printk("Unknown handle %d\n", params->handle);
}

uint8_t test_notify(struct bt_conn *conn, struct bt_gatt_subscribe_params *params, const void *buff, uint16_t size)
{
	printk("\nReceived notification #%u with length %d\n", n_not++, size);
	uint8_t text[size+1];

    int i;

    for(i = 0; i < size; i++) text[i] = *((char*)buff+i);

    text[size] = '\0';
    printk("\nReceived data from peripheral: %s\n\n", text);
	buff = "";

	return BT_GATT_ITER_CONTINUE;
}

static void gatt_subscribe(void)
{
	int err;

	UFLAG(subscribed_f);

	sub_params.value_handle = chrc_h;
	err = bt_gatt_subscribe(default_conn, &sub_params);

	if (err < 0) printk("Failed to subscribe!\n");
	else printk("Subscribe request sent!\n");

	WFLAG(subscribed_f);
}

static void gatt_discover(void)
{
	int err;

	static struct bt_gatt_discover_params discover_params;

	discover_params.uuid = primary_uuid;
	discover_params.func = discover;
	discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	discover_params.type = BT_GATT_DISCOVER_PRIMARY;

	err = bt_gatt_discover(default_conn, &discover_params);
	if (err) printk("Discover failed(err %d)\n", err);
	
    WFLAG(discover_f);

	printk("Discover complete\n");
}


static uint8_t discover(struct bt_conn *conn, const struct bt_gatt_attr *attr, struct bt_gatt_discover_params *params)
{
	int err;
    
	if (attr == NULL) {
		if (chrc_h == 0) printk("Did not discover long_chrc (%x)", chrc_h);
		(void)memset(params, 0, sizeof(*params));

		SFLAG(discover_f);
		return BT_GATT_ITER_STOP;
	}

	printk("[ATTRIBUTE] handle %u\n", attr->handle);

	if (params->type == BT_GATT_DISCOVER_PRIMARY && !bt_uuid_cmp(params->uuid, &ble_upper.uuid)) 
    {
		printk("Service found successfully!\n");
		params->uuid = NULL;
		params->start_handle = attr->handle + 1;
		params->type = BT_GATT_DISCOVER_CHARACTERISTIC;

		err = bt_gatt_discover(conn, params);
		if (err) printk("Discover failed (err %d)\n", err);

		return BT_GATT_ITER_STOP;
	} 
    else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) 
    {
		struct bt_gatt_chrc *chrc;
        chrc = (struct bt_gatt_chrc *)attr->user_data;

		if (!bt_uuid_cmp(chrc->uuid, &ble_receive.uuid)) chrc_h = chrc->value_handle;
        else if (!bt_uuid_cmp(chrc->uuid, &ble_notify.uuid)) notify_h = chrc->value_handle;
	}

	return BT_GATT_ITER_CONTINUE;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];
	int err;

	if (type == BT_GAP_ADV_TYPE_ADV_IND || type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND || !default_conn) {
        bt_addr_le_to_str(addr, dev, sizeof(dev));
        printk("Device found: %s (RSSI %d)\n", dev, rssi);

        if (rssi < -70 || bt_le_scan_stop()) return;

        err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
        if (err) 
        {
            printk("Create conn failed!\n");
            start_scan();
        }
    }
}

static void gatt_write_func(struct bt_conn *conn, uint8_t err, struct bt_gatt_write_params *params)
{
	if (err != BT_ATT_ERR_SUCCESS) printk("Write error: 0x%02X\n", err);
	(void)memset(params, 0, sizeof(*params));
	SFLAG(cwrite_f);
}

static void gatt_write(uint16_t handle, char* chrc_buff)
{
	static struct bt_gatt_write_params write_params;

	int err;

	if (chrc_h == handle)
    {
		write_params.data = chrc_buff;
		write_params.length = strlen(chrc_buff);

        printk("Writing...\n");
	}

	write_params.func = gatt_write_func;
	write_params.handle = handle;

	UFLAG(cwrite_f);

	err = bt_gatt_write(default_conn, &write_params);
	if (err) printk("Write failed: %d\n", err);

	WFLAG(cwrite_f);
	printk("Write Finished!\n");
}

static void start_scan(void)
{
	int err;

    /* Use active scanning and disable duplicate filtering to handle any
	 * devices that might update their advertising data at runtime. */

	err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, device_found);
	if (err) {
		printk("Scanning failed to start (err %d)\n", err);
		return;
	}

	printk("Scanning successfully started\n");
}

static void gatt_call(){
    gatt_discover();
	gatt_subscribe();
}

static void input()
{
    console_getline_init();
	bool aux = true;
    char* text = NULL;
     
    while(true)
    {
        printk("Input:\n>");
        text = console_getline();

        if (text == NULL) {
            printk("Invalid input!\n");
        }

		if(aux){
			gatt_call();
		}

		gatt_write(chrc_h, text);
		aux = false;
    }
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
	char addr[BT_ADDR_LE_STR_LEN];
    //int err;

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (conn_err) {
		printk("Failed to connect to %s (%u)\n", addr, conn_err);

		bt_conn_unref(default_conn);
		default_conn = NULL;

		start_scan();
		return;
	}

	if (default_conn != conn) {
		return;
	}

	printk("Connected: %s\n", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

    if (default_conn != conn) {
		return;
	}

	bt_conn_unref(default_conn);
	default_conn = NULL;

	start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

void main(void)
{
	int err;
	err = bt_enable(NULL);

	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	start_scan();
    input();

    //return 0;
}