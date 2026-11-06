// <wtl> project: mtkwlan | file: tests/mtk_ba_test.c | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// Host test for the TX/RX block-ack window handling (MtkBa.cpp rules):
//   - clampBaWin: power of two, capped at MTK_BA_WIN, 0 -> max
//   - ADDBA parameter field: policy bit, TID, window at bits 6..15; a granted
//     window must decode back exactly what we asked (the AP echoes it).
#include <stdio.h>
#include <stdint.h>

#define MTK_BA_WIN 64
#define SN_MASK 0xfff

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s: ", __func__); printf(__VA_ARGS__); printf("\n"); fails = 1; } } while (0)

// must match MtkBa.cpp clampBaWin
static uint16_t clampBaWin(uint16_t win, uint16_t maxWin) {
    if (!win || win > maxWin) win = maxWin;
    while (win & (win - 1)) win &= (uint16_t)(win - 1);
    return win;
}

static void test_clamp(void) {
    CHECK(clampBaWin(0, MTK_BA_WIN) == MTK_BA_WIN, "0 -> %u", clampBaWin(0, MTK_BA_WIN));
    CHECK(clampBaWin(1, MTK_BA_WIN) == 1, "1 -> 1");
    CHECK(clampBaWin(8, MTK_BA_WIN) == 8, "8 -> 8");
    CHECK(clampBaWin(33, MTK_BA_WIN) == 32, "33 -> 32");
    CHECK(clampBaWin(48, MTK_BA_WIN) == 32, "48 -> 32");
    CHECK(clampBaWin(64, MTK_BA_WIN) == 64, "64 -> 64");
    CHECK(clampBaWin(100, MTK_BA_WIN) == 64, "100 -> 64");
    CHECK(clampBaWin(256, MTK_BA_WIN) == 64, "256 capped -> 64");
    CHECK(clampBaWin(0x8000, MTK_BA_WIN) == MTK_BA_WIN, "0x8000 capped -> max");
}

// ADDBA parameter set: bits 0 policy, 1..2 tid, 3..5 reserved, 6..15 buffer size
static void test_addba_param(void) {
    for (int w = 1; w <= 64; w++) {
        uint16_t param = (uint16_t)(0x2 | (0 << 2) | (clampBaWin((uint16_t)w, MTK_BA_WIN) << 6));
        CHECK((param & 0x2) == 0x2, "policy bit set for w=%d", w);
        CHECK(((param >> 2) & 0xf) == 0, "tid 0 for w=%d", w);
        uint16_t decoded = (uint16_t)(param >> 6);
        CHECK(decoded == clampBaWin((uint16_t)w, MTK_BA_WIN), "echo decode w=%d -> %u, got %u",
              w, clampBaWin((uint16_t)w, MTK_BA_WIN), decoded);
    }
}

int main(void) {
    test_clamp();
    test_addba_param();
    printf(fails ? "BA window test FAILED\n" : "BA window test OK\n");
    return fails;
}
