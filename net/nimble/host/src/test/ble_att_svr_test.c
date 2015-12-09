/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include "nimble/hci_common.h"
#include "host/ble_hs.h"
#include "host/ble_hs_test.h"
#include "host/ble_hs_uuid.h"
#include "testutil/testutil.h"
#include "ble_l2cap.h"
#include "ble_hs_test_util.h"
#include "ble_hs_conn.h"
#include "ble_att_priv.h"
#include "ble_att_cmd.h"

static uint8_t *ble_att_svr_test_attr_r_1;
static int ble_att_svr_test_attr_r_1_len;
static uint8_t *ble_att_svr_test_attr_r_2;
static int ble_att_svr_test_attr_r_2_len;

static uint8_t ble_att_svr_test_attr_w_1[1024];
static int ble_att_svr_test_attr_w_1_len;

static void
ble_att_svr_test_misc_init(struct ble_hs_conn **conn,
                           struct ble_l2cap_chan **att_chan)
{
    ble_hs_test_util_init();

    ble_hs_test_util_create_conn(2, ((uint8_t[]){2,3,4,5,6,7,8,9}));
    *conn = ble_hs_conn_find(2);
    TEST_ASSERT_FATAL(*conn != NULL);

    *att_chan = ble_hs_conn_chan_find(*conn, BLE_L2CAP_CID_ATT);
    TEST_ASSERT_FATAL(*att_chan != NULL);
}

static int
ble_att_svr_test_misc_attr_fn_r_1(struct ble_att_svr_entry *entry,
                                     uint8_t op,
                                     union ble_att_svr_handle_arg *arg)
{
    switch (op) {
    case BLE_ATT_OP_READ_REQ:
        arg->aha_read.attr_data = ble_att_svr_test_attr_r_1;
        arg->aha_read.attr_len = ble_att_svr_test_attr_r_1_len;
        return 0;

    default:
        return -1;
    }
}

static int
ble_att_svr_test_misc_attr_fn_r_2(struct ble_att_svr_entry *entry, uint8_t op,
                                  union ble_att_svr_handle_arg *arg)
{
    switch (op) {
    case BLE_ATT_OP_READ_REQ:
        arg->aha_read.attr_data = ble_att_svr_test_attr_r_2;
        arg->aha_read.attr_len = ble_att_svr_test_attr_r_2_len;
        return 0;

    default:
        return -1;
    }
}

static int
ble_att_svr_test_misc_attr_fn_r_group(struct ble_att_svr_entry *entry,
                                      uint8_t op,
                                      union ble_att_svr_handle_arg *arg)
{
    /* Service 0x1122 from 1 to 5 */
    /* Service 0x2233 from 6 to 10 */
    /* Garbage from 11 to 13 */
    /* Service 1,2,3...16 from 14 to 19 */
    /* Garbage from 20 to 22 */

    static uint8_t vals[23][16] = {
        [1] =   { 0x22, 0x11 },
        [2] =   { 0xdd, 0xdd },
        [3] =   { 0xdd, 0xdd },
        [4] =   { 0xdd, 0xdd },
        [5] =   { 0xdd, 0xdd },
        [6] =   { 0x33, 0x22 },
        [7] =   { 0xee, 0xee },
        [8] =   { 0xee, 0xee },
        [9] =   { 0xee, 0xee },
        [10] =  { 0xee, 0xee },
        [11] =  { 0xbe, 0xff },
        [12] =  { 0xbe, 0xff },
        [13] =  { 0xbe, 0xff },
        [14] =  { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16 },
        [15] =  { 0xdd, 0xdd },
        [16] =  { 0xdd, 0xdd },
        [17] =  { 0xdd, 0xdd },
        [18] =  { 0xdd, 0xdd },
        [19] =  { 0xdd, 0xdd },
        [20] =  { 0xbe, 0xff },
        [21] =  { 0xbe, 0xff },
        [22] =  { 0xbe, 0xff },
    };

    static uint8_t zeros[14];

    if (op != BLE_ATT_OP_READ_REQ) {
        return -1;
    }

    TEST_ASSERT_FATAL(entry->ha_handle_id >= 1 && entry->ha_handle_id <= 22);

    arg->aha_read.attr_data = vals + entry->ha_handle_id;
    if (memcmp(arg->aha_read.attr_data + 2, zeros, 14) == 0) {
        arg->aha_read.attr_len = 2;
    } else {
        arg->aha_read.attr_len = 16;
    }

    return 0;
}

static void
ble_att_svr_test_misc_register_uuid128(uint8_t *uuid128, uint8_t flags,
                                       uint16_t expected_handle,
                                       ble_att_svr_handle_func *fn)
{
    uint16_t handle;
    int rc;

    rc = ble_att_svr_register(uuid128, flags, &handle, fn);
    TEST_ASSERT_FATAL(rc == 0);
    TEST_ASSERT_FATAL(handle == expected_handle);
}

static void
ble_att_svr_test_misc_register_uuid16(uint16_t uuid16, uint8_t flags,
                                      uint16_t expected_handle,
                                      ble_att_svr_handle_func *fn)
{
    uint8_t uuid128[16];
    int rc;

    rc = ble_hs_uuid_from_16bit(uuid16, uuid128);
    TEST_ASSERT_FATAL(rc == 0);

    ble_att_svr_test_misc_register_uuid128(uuid128, flags, expected_handle,
                                           fn);
}

static void
ble_att_svr_test_misc_register_group_attrs(void)
{
    /* Service 0x1122 from 1 to 5 */
    /* Service 0x2233 from 6 to 10 */
    /* Garbage from 11 to 13 */
    /* Service 1,2,3...16 from 14 to 19 */
    /* Garbage from 20 to 22 */

    int i;

    /* Service 0x1122 from 1 to 5 */
    ble_att_svr_test_misc_register_uuid16(
        BLE_ATT_UUID_PRIMARY_SERVICE, 0, 1,
        ble_att_svr_test_misc_attr_fn_r_group);
    for (i = 2; i <= 5; i++) {
        ble_att_svr_test_misc_register_uuid16(
            BLE_ATT_UUID_CHARACTERISTIC, 0, i,
            ble_att_svr_test_misc_attr_fn_r_group);
    }

    /* Service 0x2233 from 6 to 10 */
    ble_att_svr_test_misc_register_uuid16(
        BLE_ATT_UUID_PRIMARY_SERVICE, 0, 6,
        ble_att_svr_test_misc_attr_fn_r_group);
    for (i = 7; i <= 10; i++) {
        ble_att_svr_test_misc_register_uuid16(
            BLE_ATT_UUID_INCLUDE, 0, i,
            ble_att_svr_test_misc_attr_fn_r_group);
    }

    /* Garbage from 11 to 13 */
    for (i = 11; i <= 13; i++) {
        ble_att_svr_test_misc_register_uuid16(
            0x8797, 0, i,
            ble_att_svr_test_misc_attr_fn_r_group);
    }

    /* Service 1,2,3...16 from 14 to 19 */
    ble_att_svr_test_misc_register_uuid16(
        BLE_ATT_UUID_PRIMARY_SERVICE, 0, 14,
        ble_att_svr_test_misc_attr_fn_r_group);
    for (i = 15; i <= 19; i++) {
        ble_att_svr_test_misc_register_uuid16(
            BLE_ATT_UUID_CHARACTERISTIC, 0, i,
            ble_att_svr_test_misc_attr_fn_r_group);
    }

    /* Garbage from 20 to 22 */
    for (i = 20; i <= 22; i++) {
        ble_att_svr_test_misc_register_uuid16(
            0xabab, 0, i,
            ble_att_svr_test_misc_attr_fn_r_group);
    }
}

static int
ble_att_svr_test_misc_attr_fn_w_1(struct ble_att_svr_entry *entry, uint8_t op,
                                  union ble_att_svr_handle_arg *arg)
{
    struct os_mbuf_pkthdr *omp;
    int rc;

    switch (op) {
    case BLE_ATT_OP_WRITE_REQ:
        omp = OS_MBUF_PKTHDR(arg->aha_write.om);
        rc = os_mbuf_copydata(arg->aha_write.om, 0, arg->aha_write.attr_len,
                              ble_att_svr_test_attr_w_1);
        TEST_ASSERT(rc == 0);
        ble_att_svr_test_attr_w_1_len = arg->aha_write.attr_len;
        return 0;

    default:
        return -1;
    }

    (void)omp;
}

static void
ble_att_svr_test_misc_verify_tx_err_rsp(struct ble_l2cap_chan *chan,
                                        uint8_t req_op, uint16_t handle,
                                        uint8_t error_code)
{
    struct ble_att_error_rsp rsp;
    uint8_t buf[BLE_ATT_ERROR_RSP_SZ];
    int rc;

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, 0, sizeof buf, buf);
    TEST_ASSERT(rc == 0);

    rc = ble_att_error_rsp_parse(buf, sizeof buf, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(rsp.baep_req_op == req_op);
    TEST_ASSERT(rsp.baep_handle == handle);
    TEST_ASSERT(rsp.baep_error_code == error_code);

    /* Remove the error response from the buffer. */
    os_mbuf_adj(ble_hs_test_util_prev_tx,
                BLE_ATT_ERROR_RSP_SZ);
}

static void
ble_att_svr_test_misc_verify_tx_read_rsp(struct ble_l2cap_chan *chan,
                                            uint8_t *attr_data, int attr_len)
{
    uint8_t u8;
    int rc;
    int i;

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, 0, 1, &u8);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(u8 == BLE_ATT_OP_READ_RSP);

    for (i = 0; i < attr_len; i++) {
        rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, i + 1, 1, &u8);
        TEST_ASSERT(rc == 0);
        TEST_ASSERT(u8 == attr_data[i]);
    }

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, i + 1, 1, &u8);
    TEST_ASSERT(rc != 0);

    /* Remove the read response from the buffer. */
    os_mbuf_adj(ble_hs_test_util_prev_tx, attr_len + 1);
}

static void
ble_att_svr_test_misc_verify_tx_write_rsp(struct ble_l2cap_chan *chan)
{
    uint8_t u8;
    int rc;

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, 0, 1, &u8);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(u8 == BLE_ATT_OP_WRITE_RSP);

    /* Remove the write response from the buffer. */
    os_mbuf_adj(ble_hs_test_util_prev_tx,
                BLE_ATT_WRITE_RSP_SZ);
}

static void
ble_att_svr_test_misc_verify_tx_mtu_rsp(struct ble_l2cap_chan *chan)
{
    struct ble_att_mtu_cmd rsp;
    uint8_t buf[BLE_ATT_MTU_CMD_SZ];
    int rc;

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, 0, sizeof buf, buf);
    TEST_ASSERT(rc == 0);

    rc = ble_att_mtu_cmd_parse(buf, sizeof buf, &rsp);
    TEST_ASSERT(rc == 0);

    TEST_ASSERT(rsp.bamc_mtu == chan->blc_my_mtu);

    /* Remove the write response from the buffer. */
    os_mbuf_adj(ble_hs_test_util_prev_tx,
                BLE_ATT_MTU_CMD_SZ);
}

struct ble_att_svr_test_info_entry {
    uint16_t handle;        /* 0 on last entry */
    uint16_t uuid16;        /* 0 if not present. */
    uint8_t uuid128[16];
};

static void
ble_att_svr_test_misc_verify_tx_find_info_rsp(
    struct ble_l2cap_chan *chan,
    struct ble_att_svr_test_info_entry *entries)
{
    struct ble_att_svr_test_info_entry *entry;
    struct ble_att_find_info_rsp rsp;
    uint16_t handle;
    uint16_t uuid16;
    uint8_t buf[BLE_ATT_FIND_INFO_RSP_BASE_SZ];
    uint8_t uuid128[16];
    int off;
    int rc;

    off = 0;

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, sizeof buf, buf);
    TEST_ASSERT(rc == 0);
    off += sizeof buf;

    rc = ble_att_find_info_rsp_parse(buf, sizeof buf, &rsp);
    TEST_ASSERT(rc == 0);

    for (entry = entries; entry->handle != 0; entry++) {
        rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, 2, &handle);
        TEST_ASSERT(rc == 0);
        off += 2;

        handle = le16toh((void *)&handle);
        TEST_ASSERT(handle == entry->handle);

        if (entry->uuid16 != 0) {
            TEST_ASSERT(rsp.bafp_format ==
                        BLE_ATT_FIND_INFO_RSP_FORMAT_16BIT);
            rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, 2, &uuid16);
            TEST_ASSERT(rc == 0);
            off += 2;

            uuid16 = le16toh((void *)&uuid16);
            TEST_ASSERT(uuid16 == entry->uuid16);
        } else {
            TEST_ASSERT(rsp.bafp_format ==
                        BLE_ATT_FIND_INFO_RSP_FORMAT_128BIT);
            rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, 16, uuid128);
            TEST_ASSERT(rc == 0);
            off += 16;

            TEST_ASSERT(memcmp(uuid128, entry->uuid128, 16) == 0);
        }
    }

    /* Ensure there is no extra data in the response. */
    TEST_ASSERT(off == OS_MBUF_PKTHDR(ble_hs_test_util_prev_tx)->omp_len);

    /* Remove the response from the buffer. */
    os_mbuf_adj(ble_hs_test_util_prev_tx, off);
}

struct ble_att_svr_test_type_value_entry {
    uint16_t first;        /* 0 on last entry */
    uint16_t last;
};

static void
ble_att_svr_test_misc_verify_tx_find_type_value_rsp(
    struct ble_l2cap_chan *chan,
    struct ble_att_svr_test_type_value_entry *entries)
{
    struct ble_att_svr_test_type_value_entry *entry;
    uint16_t u16;
    uint8_t op;
    int off;
    int rc;

    off = 0;

    rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, 1, &op);
    TEST_ASSERT(rc == 0);
    off += 1;

    TEST_ASSERT(op == BLE_ATT_OP_FIND_TYPE_VALUE_RSP);

    for (entry = entries; entry->first != 0; entry++) {
        rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, 2, &u16);
        TEST_ASSERT(rc == 0);
        htole16(&u16, u16);
        TEST_ASSERT(u16 == entry->first);
        off += 2;

        rc = os_mbuf_copydata(ble_hs_test_util_prev_tx, off, 2, &u16);
        TEST_ASSERT(rc == 0);
        htole16(&u16, u16);
        TEST_ASSERT(u16 == entry->last);
        off += 2;
    }

    /* Ensure there is no extra data in the response. */
    TEST_ASSERT(off == OS_MBUF_PKTHDR(ble_hs_test_util_prev_tx)->omp_len);

    /* Remove the response from the buffer. */
    os_mbuf_adj(ble_hs_test_util_prev_tx, off);
}

struct ble_att_svr_test_group_type_entry {
    uint16_t start_handle;  /* 0 on last entry */
    uint16_t end_handle;    /* 0 on last entry */
    uint16_t uuid16;        /* 0 if not present. */
    uint8_t uuid128[16];
};

/** Returns the number of entries successfully verified. */
static void
ble_att_svr_test_misc_verify_tx_read_group_type_rsp(
    struct ble_l2cap_chan *chan,
    struct ble_att_svr_test_group_type_entry *entries)
{
    struct ble_att_svr_test_group_type_entry *entry;
    struct ble_att_read_group_type_rsp rsp;
    struct os_mbuf *om;
    uint16_t u16;
    uint8_t uuid128[16];
    int off;
    int rc;

    om = os_mbuf_pullup(ble_hs_test_util_prev_tx,
                        BLE_ATT_READ_GROUP_TYPE_RSP_BASE_SZ);
    TEST_ASSERT_FATAL(om != NULL);

    rc = ble_att_read_group_type_rsp_parse(om->om_data, om->om_len, &rsp);
    TEST_ASSERT(rc == 0);

    off = BLE_ATT_READ_GROUP_TYPE_RSP_BASE_SZ;
    for (entry = entries; entry->start_handle != 0; entry++) {
        if (entry->uuid16 != 0) {
            TEST_ASSERT(rsp.bagp_length ==
                        BLE_ATT_READ_GROUP_TYPE_ADATA_SZ_16);
        } else {
            TEST_ASSERT(rsp.bagp_length ==
                        BLE_ATT_READ_GROUP_TYPE_ADATA_SZ_128);
        }

        rc = os_mbuf_copydata(om, off, 2, &u16);
        TEST_ASSERT(rc == 0);
        htole16(&u16, u16);
        TEST_ASSERT(u16 == entry->start_handle);
        off += 2;

        rc = os_mbuf_copydata(om, off, 2, &u16);
        TEST_ASSERT(rc == 0);
        htole16(&u16, u16);
        TEST_ASSERT(u16 == entry->end_handle);
        off += 2;

        if (entry->uuid16 != 0) {
            rc = os_mbuf_copydata(om, off, 2, &u16);
            TEST_ASSERT(rc == 0);
            htole16(&u16, u16);
            TEST_ASSERT(u16 == entry->uuid16);
            off += 2;
        } else {
            rc = os_mbuf_copydata(om, off, 16, uuid128);
            TEST_ASSERT(rc == 0);
            TEST_ASSERT(memcmp(uuid128, entry->uuid128, 16) == 0);
            off += 16;
        }
    }

    /* Ensure there is no extra data in the response. */
    TEST_ASSERT(off == OS_MBUF_PKTLEN(om));
}

static void
ble_att_svr_test_misc_mtu_exchange(uint16_t my_mtu, uint16_t peer_sent,
                                   uint16_t peer_actual, uint16_t chan_mtu)
{
    struct ble_att_mtu_cmd req;
    struct ble_l2cap_chan *chan;
    struct ble_hs_conn *conn;
    uint8_t buf[BLE_ATT_MTU_CMD_SZ];
    int rc;

    ble_att_svr_test_misc_init(&conn, &chan);

    chan->blc_my_mtu = my_mtu;

    req.bamc_mtu = peer_sent;
    rc = ble_att_mtu_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    TEST_ASSERT(chan->blc_peer_mtu == peer_actual);

    ble_att_svr_test_misc_verify_tx_mtu_rsp(chan);

    TEST_ASSERT(ble_l2cap_chan_mtu(chan) == chan_mtu);
}

TEST_CASE(ble_att_svr_test_mtu)
{
    /*** MTU too low; should pretend peer sent default value instead. */
    ble_att_svr_test_misc_mtu_exchange(BLE_ATT_MTU_DFLT, 5,
                                       BLE_ATT_MTU_DFLT, BLE_ATT_MTU_DFLT);

    /*** MTUs equal. */
    ble_att_svr_test_misc_mtu_exchange(50, 50, 50, 50);

    /*** Peer's higher than mine. */
    ble_att_svr_test_misc_mtu_exchange(50, 100, 100, 50);

    /*** Mine higher than peer's. */
    ble_att_svr_test_misc_mtu_exchange(100, 50, 50, 50);
}

TEST_CASE(ble_att_svr_test_read)
{
    struct ble_att_read_req req;
    struct ble_l2cap_chan *chan;
    struct ble_hs_conn *conn;
    uint8_t buf[BLE_ATT_READ_REQ_SZ];
    uint8_t uuid[16] = {0};
    int rc;

    ble_att_svr_test_misc_init(&conn, &chan);

    /*** Nonexistent attribute. */
    req.barq_handle = 0;
    rc = ble_att_read_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();
    ble_att_svr_test_misc_verify_tx_err_rsp(chan, BLE_ATT_OP_READ_REQ, 0,
                                               BLE_ATT_ERR_INVALID_HANDLE);

    /*** Successful read. */
    ble_att_svr_test_attr_r_1 = (uint8_t[]){0,1,2,3,4,5,6,7};
    ble_att_svr_test_attr_r_1_len = 8;
    rc = ble_att_svr_register(uuid, 0, &req.barq_handle,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    rc = ble_att_read_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_read_rsp(
        chan, ble_att_svr_test_attr_r_1, ble_att_svr_test_attr_r_1_len);

    /*** Partial read. */
    ble_att_svr_test_attr_r_1 =
        (uint8_t[]){0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,
                    22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39};
    ble_att_svr_test_attr_r_1_len = 40;

    rc = ble_att_read_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_read_rsp(chan,
                                                ble_att_svr_test_attr_r_1,
                                                BLE_ATT_MTU_DFLT - 1);
}

TEST_CASE(ble_att_svr_test_write)
{
    struct ble_att_write_req req;
    struct ble_l2cap_chan *chan;
    struct ble_hs_conn *conn;
    uint8_t buf[BLE_ATT_READ_REQ_SZ + 8];
    uint8_t uuid[16] = {0};
    int rc;

    ble_att_svr_test_misc_init(&conn, &chan);

    /*** Nonexistent attribute. */
    req.bawq_handle = 0;
    rc = ble_att_write_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    memcpy(buf + BLE_ATT_READ_REQ_SZ, ((uint8_t[]){0,1,2,3,4,5,6,7}), 8);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();
    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_WRITE_REQ, 0, BLE_ATT_ERR_INVALID_HANDLE);

    /*** Successful write. */
    rc = ble_att_svr_register(uuid, 0, &req.bawq_handle,
                                 ble_att_svr_test_misc_attr_fn_w_1);
    TEST_ASSERT(rc == 0);

    rc = ble_att_write_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    memcpy(buf + BLE_ATT_WRITE_REQ_MIN_SZ,
           ((uint8_t[]){0,1,2,3,4,5,6,7}), 8);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_write_rsp(chan);
}

TEST_CASE(ble_att_svr_test_find_info)
{
    struct ble_att_find_info_req req;
    struct ble_l2cap_chan *chan;
    struct ble_hs_conn *conn;
    uint16_t handle1;
    uint16_t handle2;
    uint16_t handle3;
    uint8_t buf[BLE_ATT_FIND_INFO_REQ_SZ];
    uint8_t uuid1[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    uint8_t uuid2[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    uint8_t uuid3[16] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00
    };
    int rc;

    ble_att_svr_test_misc_init(&conn, &chan);

    /* Increase the MTU to 128 bytes to allow testing of long responses. */
    chan->blc_my_mtu = 128;
    chan->blc_peer_mtu = 128;
    chan->blc_flags |= BLE_L2CAP_CHAN_F_TXED_MTU;

    /*** Start handle of 0. */
    req.bafq_start_handle = 0;
    req.bafq_end_handle = 0;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_INFO_REQ, 0, BLE_ATT_ERR_INVALID_HANDLE);

    /*** Start handle > end handle. */
    req.bafq_start_handle = 101;
    req.bafq_end_handle = 100;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_INFO_REQ, 101, BLE_ATT_ERR_INVALID_HANDLE);

    /*** No attributes. */
    req.bafq_start_handle = 200;
    req.bafq_end_handle = 300;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_INFO_REQ, 200, BLE_ATT_ERR_ATTR_NOT_FOUND);

    /*** Range too late. */
    rc = ble_att_svr_register(uuid1, 0, &handle1,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    req.bafq_start_handle = 200;
    req.bafq_end_handle = 300;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_INFO_REQ, 200, BLE_ATT_ERR_ATTR_NOT_FOUND);

    /*** One 128-bit entry. */
    req.bafq_start_handle = handle1;
    req.bafq_end_handle = handle1;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_info_rsp(chan,
        ((struct ble_att_svr_test_info_entry[]) { {
            .handle = handle1,
            .uuid128 = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        }, {
            .handle = 0,
        } }));

    /*** Two 128-bit entries. */
    rc = ble_att_svr_register(uuid2, 0,
                                 &handle2,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    req.bafq_start_handle = handle1;
    req.bafq_end_handle = handle2;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_info_rsp(chan,
        ((struct ble_att_svr_test_info_entry[]) { {
            .handle = handle1,
            .uuid128 = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        }, {
            .handle = handle2,
            .uuid128 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
        }, {
            .handle = 0,
        } }));

    /*** Two 128-bit entries; 16-bit entry doesn't get sent. */
    rc = ble_att_svr_register(uuid3, 0,
                                 &handle3,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    req.bafq_start_handle = handle1;
    req.bafq_end_handle = handle3;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_info_rsp(chan,
        ((struct ble_att_svr_test_info_entry[]) { {
            .handle = handle1,
            .uuid128 = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
        }, {
            .handle = handle2,
            .uuid128 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
        }, {
            .handle = 0,
        } }));

    /*** Remaining 16-bit entry requested. */
    req.bafq_start_handle = handle3;
    req.bafq_end_handle = handle3;

    rc = ble_att_find_info_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_info_rsp(chan,
        ((struct ble_att_svr_test_info_entry[]) { {
            .handle = handle3,
            .uuid16 = 0x000f,
        }, {
            .handle = 0,
        } }));
}

TEST_CASE(ble_att_svr_test_find_type_value)
{
    struct ble_att_find_type_value_req req;
    struct ble_l2cap_chan *chan;
    struct ble_hs_conn *conn;
    uint8_t buf[BLE_ATT_FIND_TYPE_VALUE_REQ_BASE_SZ + 2];
    uint16_t handle1;
    uint16_t handle2;
    uint16_t handle3;
    uint16_t handle4;
    uint16_t handle5;
    uint8_t uuid1[16] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
    };
    uint8_t uuid2[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    uint8_t uuid3[16] = {
        0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00
    };
    int rc;

    ble_att_svr_test_misc_init(&conn, &chan);

    /* Increase the MTU to 128 bytes to allow testing of long responses. */
    chan->blc_my_mtu = 128;
    chan->blc_peer_mtu = 128;
    chan->blc_flags |= BLE_L2CAP_CHAN_F_TXED_MTU;

    /* One-time write of the attribute value at the end of the request. */
    ble_att_svr_test_attr_r_1 = (uint8_t[]){0x99, 0x99};
    ble_att_svr_test_attr_r_1_len = 2;
    memcpy(buf + BLE_ATT_FIND_TYPE_VALUE_REQ_BASE_SZ,
           ble_att_svr_test_attr_r_1,
           ble_att_svr_test_attr_r_1_len);

    /*** Start handle of 0. */
    req.bavq_start_handle = 0;
    req.bavq_end_handle = 0;
    req.bavq_attr_type = 0x0001;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_TYPE_VALUE_REQ, 0,
        BLE_ATT_ERR_INVALID_HANDLE);

    /*** Start handle > end handle. */
    req.bavq_start_handle = 101;
    req.bavq_end_handle = 100;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_TYPE_VALUE_REQ, 101,
        BLE_ATT_ERR_INVALID_HANDLE);

    /*** No attributes. */
    req.bavq_start_handle = 200;
    req.bavq_end_handle = 300;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_TYPE_VALUE_REQ, 200,
        BLE_ATT_ERR_ATTR_NOT_FOUND);

    /*** Range too late. */
    rc = ble_att_svr_register(uuid1, 0, &handle1,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    req.bavq_start_handle = 200;
    req.bavq_end_handle = 300;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_FIND_TYPE_VALUE_REQ, 200,
        BLE_ATT_ERR_ATTR_NOT_FOUND);

    /*** One entry, one attribute. */
    req.bavq_start_handle = handle1;
    req.bavq_end_handle = handle1;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_type_value_rsp(chan,
        ((struct ble_att_svr_test_type_value_entry[]) { {
            .first = handle1,
            .last = handle1,
        }, {
            .first = 0,
        } }));

    /*** One entry, two attributes. */
    rc = ble_att_svr_register(uuid1, 0, &handle2,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    req.bavq_start_handle = handle1;
    req.bavq_end_handle = handle2;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_type_value_rsp(chan,
        ((struct ble_att_svr_test_type_value_entry[]) { {
            .first = handle1,
            .last = handle2,
        }, {
            .first = 0,
        } }));

    /*** Entry 1: two attributes; entry 2: one attribute. */
    rc = ble_att_svr_register(uuid2, 0, &handle3,
                                 ble_att_svr_test_misc_attr_fn_r_2);
    TEST_ASSERT(rc == 0);

    rc = ble_att_svr_register(uuid1, 0, &handle4,
                                 ble_att_svr_test_misc_attr_fn_r_1);
    TEST_ASSERT(rc == 0);

    req.bavq_start_handle = 0x0001;
    req.bavq_end_handle = 0xffff;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_type_value_rsp(chan,
        ((struct ble_att_svr_test_type_value_entry[]) { {
            .first = handle1,
            .last = handle2,
        }, {
            .first = handle4,
            .last = handle4,
        }, {
            .first = 0,
        } }));

    /*** Ensure attribute with wrong value is not included. */
    ble_att_svr_test_attr_r_2 = (uint8_t[]){0x00, 0x00};
    ble_att_svr_test_attr_r_2_len = 2;

    req.bavq_start_handle = 0x0001;
    req.bavq_end_handle = 0xffff;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_type_value_rsp(chan,
        ((struct ble_att_svr_test_type_value_entry[]) { {
            .first = handle1,
            .last = handle2,
        }, {
            .first = handle4,
            .last = handle4,
        }, {
            .first = 0,
        } }));

    /*** Ensure attribute with wrong type is not included. */
    rc = ble_att_svr_register(uuid3, 0, &handle5,
                              ble_att_svr_test_misc_attr_fn_r_1);

    req.bavq_start_handle = 0x0001;
    req.bavq_end_handle = 0xffff;

    rc = ble_att_find_type_value_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_find_type_value_rsp(chan,
        ((struct ble_att_svr_test_type_value_entry[]) { {
            .first = handle1,
            .last = handle2,
        }, {
            .first = handle4,
            .last = handle4,
        }, {
            .first = 0,
        } }));
}

TEST_CASE(ble_att_svr_test_read_group_type)
{
    struct ble_att_read_group_type_req req;
    struct ble_l2cap_chan *chan;
    struct ble_hs_conn *conn;
    uint8_t buf[BLE_ATT_READ_GROUP_TYPE_REQ_SZ_16];
    int rc;

    ble_att_svr_test_misc_init(&conn, &chan);

    /* Increase the MTU to 128 bytes to allow testing of long responses. */
    chan->blc_my_mtu = 128;
    chan->blc_peer_mtu = 128;
    chan->blc_flags |= BLE_L2CAP_CHAN_F_TXED_MTU;

    /*** Start handle of 0. */
    req.bagq_start_handle = 0;
    req.bagq_end_handle = 0;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat( conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_READ_GROUP_TYPE_REQ, 0,
        BLE_ATT_ERR_INVALID_HANDLE);

    /*** Start handle > end handle. */
    req.bagq_start_handle = 101;
    req.bagq_end_handle = 100;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_READ_GROUP_TYPE_REQ, 101,
        BLE_ATT_ERR_INVALID_HANDLE);

    /*** Invalid group UUID (0x1234). */
    req.bagq_start_handle = 110;
    req.bagq_end_handle = 150;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ, 0x1234);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_READ_GROUP_TYPE_REQ, 110,
        BLE_ATT_ERR_UNSUPPORTED_GROUP);

    /*** No attributes. */
    req.bagq_start_handle = 1;
    req.bagq_end_handle = 0xffff;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_READ_GROUP_TYPE_REQ, 1,
        BLE_ATT_ERR_ATTR_NOT_FOUND);

    /*** Range too late. */
    ble_att_svr_test_misc_register_group_attrs();
    req.bagq_start_handle = 200;
    req.bagq_end_handle = 300;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc != 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_err_rsp(
        chan, BLE_ATT_OP_READ_GROUP_TYPE_REQ, 200,
        BLE_ATT_ERR_ATTR_NOT_FOUND);

    /*** One 16-bit UUID service. */
    req.bagq_start_handle = 1;
    req.bagq_end_handle = 5;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_read_group_type_rsp(chan,
        ((struct ble_att_svr_test_group_type_entry[]) { {
            .start_handle = 1,
            .end_handle = 5,
            .uuid16 = 0x1122,
        }, {
            .start_handle = 0,
        } }));

    /*** Two 16-bit UUID services. */
    req.bagq_start_handle = 1;
    req.bagq_end_handle = 10;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_read_group_type_rsp(chan,
        ((struct ble_att_svr_test_group_type_entry[]) { {
            .start_handle = 1,
            .end_handle = 5,
            .uuid16 = 0x1122,
        }, {
            .start_handle = 6,
            .end_handle = 10,
            .uuid16 = 0x2233,
        }, {
            .start_handle = 0,
        } }));

    /*** Two 16-bit UUID services; ensure 128-bit service not returned. */
    req.bagq_start_handle = 1;
    req.bagq_end_handle = 100;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_read_group_type_rsp(chan,
        ((struct ble_att_svr_test_group_type_entry[]) { {
            .start_handle = 1,
            .end_handle = 5,
            .uuid16 = 0x1122,
        }, {
            .start_handle = 6,
            .end_handle = 10,
            .uuid16 = 0x2233,
        }, {
            .start_handle = 0,
        } }));

    /*** One 128-bit service. */
    req.bagq_start_handle = 11;
    req.bagq_end_handle = 100;

    rc = ble_att_read_group_type_req_write(buf, sizeof buf, &req);
    TEST_ASSERT(rc == 0);
    htole16(buf + BLE_ATT_READ_GROUP_TYPE_REQ_BASE_SZ,
            BLE_ATT_UUID_PRIMARY_SERVICE);

    rc = ble_hs_test_util_l2cap_rx_payload_flat(conn, chan, buf, sizeof buf);
    TEST_ASSERT(rc == 0);
    ble_hs_process_tx_data_queue();

    ble_att_svr_test_misc_verify_tx_read_group_type_rsp(chan,
        ((struct ble_att_svr_test_group_type_entry[]) { {
            .start_handle = 14,
            .end_handle = 19,
            .uuid128 = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
        }, {
            .start_handle = 0,
        } }));
}

TEST_SUITE(ble_att_svr_suite)
{
    ble_att_svr_test_mtu();
    ble_att_svr_test_read();
    ble_att_svr_test_write();
    ble_att_svr_test_find_info();
    ble_att_svr_test_find_type_value();
    ble_att_svr_test_read_group_type();
}

int
ble_att_svr_test_all(void)
{
    ble_att_svr_suite();

    return tu_any_failed;
}
