#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>
#include <zephyr.h>
#include <zephyr/types.h>
#include <sys/printk.h>
#include <sys/byteorder.h>
#include <services/ble_application.h>
#include <services/ble_uart_service.h>

static void ble_buffer(const uint8_t *buffer, size_t size) {
    service_transmit(buffer, size);
}

static void ble_stack(struct bt_conn *conn) {
    (void)conn;
    service_register(ble_buffer);
}

void main (void) {
    ble_application_start(ble_stack);
}