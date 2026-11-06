// <wtl> project: mtkwlan | file: tests/mtk_txs_test.c | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// Host test for the TXS (TX status) record layout on MT7927's RXQ_DATA2 ring, using
// raw packets captured on the real card (ioreg MtkTxsDump0/1, 2026-11-06).
// Verified layout: RXD header 16 bytes, then 48-byte (12 dword) records,
// TXS0 at record+0, matching mt7925_rx_check (rxd += 4; rxd += 12).
//   TXS0: TXS_FORMAT 24:23, BW 31:29, rate 13:0 (mode 9:6, nss 13:10, mcs 5:0)
//   TXS2: WCID 27:16, TX_DELAY 15:0; TXS3: PID 31:24, tx count 29:25
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define AP_WCID 1u
#define TXS_HDR 16u      // RXD header bytes before the first record
#define TXS_REC 48u      // 12 dwords per record (mt76_connac3_mac.h MT_TXS_SIZE)

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

// Field mapping of the driver's txStatus, kept here so layout changes are checked
// against the on-card bytes without a reboot.
static void fields(const uint8_t *rec, uint32_t *mode, uint32_t *idx, uint32_t *nss, uint32_t *bw,
                   uint32_t *wcid, uint32_t *format, uint32_t *delay, uint32_t *count, uint32_t *pid) {
    uint32_t txs0 = le32(rec + 0), txs2 = le32(rec + 8), txs3 = le32(rec + 12);
    *format = (txs0 >> 23) & 3;
    *bw = (txs0 >> 29) & 7;
    *wcid = (txs2 >> 16) & 0xfff;
    *delay = txs2 & 0xffff;
    *pid = txs3 >> 24;
    *count = (txs3 >> 25) & 0x1f;
    uint32_t rate = txs0 & 0x3fff;
    *mode = (rate >> 6) & 0xf;
    *nss = ((rate >> 10) & 0xf) & 3;
    *idx = rate & 0x3f;
}

static int fail = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s: ", __func__); printf(__VA_ARGS__); printf("\n"); fail = 1; } } while (0)

// MtkTxsDump0 (2026-11-06, ring 7): RXD 16B, one record, TXS0=0x0000804b
static const uint8_t dump0[128] = {
    0x40,0x00,0x01,0x00,0x00,0x00,0xbc,0x08, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x4b,0x80,0x00,0x00, 0x2a,0x36,0xcb,0x01, 0x09,0x00,0x01,0x10, 0x4f,0x00,0x00,0x03,
    0x02,0x0c,0xa5,0x00, 0x56,0x31,0x05,0x82, 0xff,0xff,0xff,0xff, 0x62,0x6c,0xff,0xff,
    0x00,0x00,0x00,0x00, 0xef,0xbe,0xad,0xde, 0xef,0xbe,0xad,0xde, 0xef,0xbe,0xad,0xde
};
// MtkTxsDump1 (another sample, TXS0=0x0000804b, TXS2 wcid 0x1001)
static const uint8_t dump1[128] = {
    0x40,0x00,0x01,0x00,0x00,0x00,0xc6,0x08, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x4b,0x80,0x00,0x00, 0x2a,0x3b,0xdb,0x01, 0x0d,0x00,0x01,0x10, 0x4f,0x00,0x00,0x03,
    0x17,0xa5,0x00,0xb5, 0x31,0x05,0x82,0xff, 0xff,0xff,0xff,0xff, 0x63,0x6b,0xff,0xff,
    0x00,0x00,0x00,0x00, 0xef,0xbe,0xad,0xde, 0xef,0xbe,0xad,0xde, 0xef,0xbe,0xad,0xde
};

int main(void) {
    uint32_t mode, idx, nss, bw, wcid, fmt, delay, count, pid;
    for (int d = 0; d < 2; d++) {
        const uint8_t *p = d ? dump1 : dump0;
        const uint8_t *rec = p + TXS_HDR;
        fields(rec, &mode, &idx, &nss, &bw, &wcid, &fmt, &delay, &count, &pid);
        CHECK(fmt  == 0, "dump%d format=%u want 0", d, fmt);
        CHECK(mode == 1, "dump%d mode=%u want 1 (OFDM)", d, mode);
        CHECK(idx  == 0x0b, "dump%d mcs idx=%u want 0xB", d, idx);
        CHECK(nss  == 0, "dump%d nss=%u want 0", d, nss);
        CHECK(bw   == 0, "dump%d bw=%u want 0", d, bw);
        CHECK(wcid == AP_WCID, "dump%d wcid=%u want %u", d, wcid, AP_WCID);
        CHECK(delay == (d ? 13 : 9), "dump%d delay=%u want %u", d, delay, d ? 13 : 9);
        CHECK(pid  == 0x03, "dump%d pid=%u want 3", d, pid);
        CHECK(count == 1, "dump%d count=%u want 1", d, count);
    }
    // real data-frame record (MtkW9TxsLast, ring 7): TXS0=0x40008607 (mode 8, mcs 7, nss 1,
    // bw 80MHz), TXS2=0x1001003b (wcid 1, delay 59), TXS3=0x03000008 (pid 3, count 1)
    {
        uint8_t he[48];
        memset(he, 0, sizeof(he));
        he[0]=0x07; he[1]=0x86; he[2]=0x00; he[3]=0x40;                   // TXS0
        he[8+0]=0x3b; he[8+1]=0x00; he[8+2]=0x01; he[8+3]=0x10;          // TXS2 wcid 1, delay 59
        he[12+0]=0x08; he[12+1]=0x00; he[12+2]=0x00; he[12+3]=0x03;      // TXS3 pid 3, count 1
        fields(he, &mode, &idx, &nss, &bw, &wcid, &fmt, &delay, &count, &pid);
        CHECK(mode == 8, "he mode=%u want 8 (HE/EHT data)", mode);
        CHECK(idx  == 7, "he idx=%u want 7", idx);
        CHECK(nss  == 1, "he nss=%u want 1", nss);
        CHECK(bw   == 2, "he bw=%u want 2 (80MHz)", bw);
        CHECK(wcid == AP_WCID, "he wcid=%u want 1", wcid);
        CHECK(delay == 59, "he delay=%u want 59", delay);
        CHECK(pid  == 3, "he pid=%u want 3", pid);
        CHECK(count == 1, "he count=%u want 1", count);
        printf("ok: real dumps parse; data record mode=%u mcs=%u nss=%u bw=%uMHz\n", mode, idx, nss+1, 20u<<bw);
    }
    printf(fail ? "TXS layout test FAILED\n" : "TXS layout test OK\n");
    return fail;
}
