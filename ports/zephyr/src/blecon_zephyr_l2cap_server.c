/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
#include "stdlib.h"
#include "string.h"
#include "stdbool.h"
#include "assert.h"

#include "blecon_zephyr_l2cap_server.h"
#include "blecon_zephyr_l2cap_bearer.h"
#include "blecon_zephyr_bluetooth.h"
#include "blecon_zephyr_bluetooth_common.h"

#include "blecon/port/blecon_bluetooth.h"
#include "blecon/blecon_memory.h"
#include "blecon/blecon_buffer.h"
#include "blecon/blecon_error.h"

#include "zephyr/bluetooth/bluetooth.h"
#include "zephyr/bluetooth/conn.h"
#include "zephyr/bluetooth/l2cap.h"

#define BLECON_ZEPHYR_MAX_L2CAP_SERVERS 2

struct blecon_zephyr_l2cap_server_t;

struct blecon_zephyr_l2cap_server_t* blecon_zephyr_l2cap_server_accept_ctx[BLECON_ZEPHYR_MAX_L2CAP_SERVERS] = { NULL, NULL };

struct blecon_zephyr_l2cap_server_t { 
    struct blecon_bluetooth_l2cap_server_t l2cap_server;
    struct bt_l2cap_server z_l2cap_server;
    struct blecon_zephyr_l2cap_bearer_t l2cap_bearer;
};

static int blecon_zephyr_l2cap_server_accept_0(struct bt_conn* conn, struct bt_l2cap_chan** chan);
static int blecon_zephyr_l2cap_server_accept_1(struct bt_conn* conn, struct bt_l2cap_chan** chan);
static int blecon_zephyr_l2cap_server_accept(struct bt_conn* conn, struct bt_l2cap_chan** chan, struct blecon_zephyr_l2cap_server_t* zephyr_l2cap_server);

struct blecon_bluetooth_l2cap_server_t* blecon_zephyr_bluetooth_l2cap_server_new(struct blecon_bluetooth_t* bluetooth, uint8_t psm) {
    struct blecon_zephyr_l2cap_server_t* zephyr_l2cap_server = BLECON_ALLOC(sizeof(struct blecon_zephyr_l2cap_server_t));
    struct blecon_zephyr_bluetooth_t* zephyr_bluetooth = (struct blecon_zephyr_bluetooth_t*) bluetooth;

    if(zephyr_l2cap_server == NULL) {
        blecon_fatal_error();
    }
    
    blecon_zephyr_l2cap_bearer_init_server(&zephyr_l2cap_server->l2cap_bearer, zephyr_bluetooth->event_loop);

    zephyr_l2cap_server->z_l2cap_server.psm = psm;
    zephyr_l2cap_server->z_l2cap_server.sec_level = BT_SECURITY_L1;
    
    // Find a free accept callback slot
    if( blecon_zephyr_l2cap_server_accept_ctx[0] == NULL ) {
        blecon_zephyr_l2cap_server_accept_ctx[0] = zephyr_l2cap_server;
        zephyr_l2cap_server->z_l2cap_server.accept = blecon_zephyr_l2cap_server_accept_0;
    } else if( blecon_zephyr_l2cap_server_accept_ctx[1] == NULL ) {
        blecon_zephyr_l2cap_server_accept_ctx[1] = zephyr_l2cap_server;
        zephyr_l2cap_server->z_l2cap_server.accept = blecon_zephyr_l2cap_server_accept_1;
    } else {
        blecon_fatal_error();
    }
    
    int ret = bt_l2cap_server_register(&zephyr_l2cap_server->z_l2cap_server);
    blecon_assert(ret == 0);
    
    return &zephyr_l2cap_server->l2cap_server;
}

struct blecon_bearer_t* blecon_zephyr_bluetooth_l2cap_server_as_bearer(struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    struct blecon_zephyr_l2cap_server_t* zephyr_l2cap_server = (struct blecon_zephyr_l2cap_server_t*) l2cap_server;
    return blecon_zephyr_l2cap_bearer_as_bearer(&zephyr_l2cap_server->l2cap_bearer);
}

void blecon_zephyr_bluetooth_l2cap_server_free(struct blecon_bluetooth_l2cap_server_t* l2cap_server) {
    struct blecon_zephyr_l2cap_server_t* zephyr_l2cap_server = (struct blecon_zephyr_l2cap_server_t*) l2cap_server;

    // Release accept callback slot
    if( blecon_zephyr_l2cap_server_accept_ctx[0] == zephyr_l2cap_server ) {
        blecon_zephyr_l2cap_server_accept_ctx[0] = NULL;
    } else if( blecon_zephyr_l2cap_server_accept_ctx[1] == zephyr_l2cap_server ) {
        blecon_zephyr_l2cap_server_accept_ctx[1] = NULL;
    } else {
        blecon_fatal_error();
    }

    blecon_zephyr_l2cap_bearer_cleanup(&zephyr_l2cap_server->l2cap_bearer);
    BLECON_FREE(zephyr_l2cap_server);
}

int blecon_zephyr_l2cap_server_accept_0(struct bt_conn* conn, struct bt_l2cap_chan** chan) {
    return blecon_zephyr_l2cap_server_accept(conn, chan, blecon_zephyr_l2cap_server_accept_ctx[0]);
}

int blecon_zephyr_l2cap_server_accept_1(struct bt_conn* conn, struct bt_l2cap_chan** chan) {
    return blecon_zephyr_l2cap_server_accept(conn, chan, blecon_zephyr_l2cap_server_accept_ctx[1]);
}

int blecon_zephyr_l2cap_server_accept(struct bt_conn* conn, struct bt_l2cap_chan** chan, struct blecon_zephyr_l2cap_server_t* zephyr_l2cap_server) {
    if( blecon_zephyr_l2cap_bearer_is_connected(&zephyr_l2cap_server->l2cap_bearer) ) {
        return -ENOMEM;
    }

    blecon_zephyr_l2cap_bearer_server_accept(&zephyr_l2cap_server->l2cap_bearer, conn, chan);

    return 0;
}
