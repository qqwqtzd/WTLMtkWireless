// <wtl> project: mtkwlan | file: tests/mtk_wmm_test.c | author: watchthelight </wtl>
// SPDX-License-Identifier: ISC
// Host test for the WMM TID -> AC -> LMAC queue mapping (IEEE 802.11-2020
// 10.2.3.2 vs MediaTek's MT_LMAC_AC order).
#include <stdio.h>
#include <stdint.h>

static int fails = 0;
#define CHECK(c, name) do { if (!(c)) { printf("FAIL: %s\n", name); fails = 1; } } while (0)

int mtk_tid_to_ac(uint8_t tid);
int mtk_ac_to_lmac(int ac);
int mtk_tid_to_lmac(uint8_t tid);
uint8_t mtk_eth_tid(const uint8_t *eh, uint32_t len);
uint8_t mtk_dscp_tid(uint8_t dscp);

int main(void) {
    // TID -> AC (mac80211 order): 0-1 BE(2), 2-3 BK(3), 4-5 VI(1), 6-7 VO(0), 8..15 -> VO/BE
    CHECK(mtk_tid_to_ac(0) == 2, "tid0 BE");
    CHECK(mtk_tid_to_ac(1) == 2, "tid1 BE");
    CHECK(mtk_tid_to_ac(2) == 3, "tid2 BK");
    CHECK(mtk_tid_to_ac(3) == 3, "tid3 BK");
    CHECK(mtk_tid_to_ac(4) == 1, "tid4 VI");
    CHECK(mtk_tid_to_ac(5) == 1, "tid5 VI");
    CHECK(mtk_tid_to_ac(6) == 0, "tid6 VO");
    CHECK(mtk_tid_to_ac(7) == 0, "tid7 VO");
    CHECK(mtk_tid_to_ac(8) == 0, "tid8 VO (downgrade)");
    CHECK(mtk_tid_to_ac(15) == 0, "tid15 VO parity");

    // AC -> LMAC: 3 - ac  (BE=2 -> LMAC 1, BK=3 -> 0, VI=1 -> 2, VO=0 -> 3)
    CHECK(mtk_ac_to_lmac(0) == 3, "VO -> lmac3");
    CHECK(mtk_ac_to_lmac(1) == 2, "VI -> lmac2");
    CHECK(mtk_ac_to_lmac(2) == 1, "BE -> lmac1");
    CHECK(mtk_ac_to_lmac(3) == 0, "BK -> lmac0");
    CHECK(mtk_ac_to_lmac(5) == 1, "bad ac clamped to BE");

    // TID -> LMAC composite: tid0 -> BE -> lmac1; tid6 -> VO -> lmac3; tid2 -> BK -> lmac0
    CHECK(mtk_tid_to_lmac(0) == 1, "tid0 lmac1");
    CHECK(mtk_tid_to_lmac(1) == 1, "tid1 lmac1");
    CHECK(mtk_tid_to_lmac(2) == 0, "tid2 lmac0");
    CHECK(mtk_tid_to_lmac(4) == 2, "tid4 lmac2");
    CHECK(mtk_tid_to_lmac(6) == 3, "tid6 lmac3");
    CHECK(mtk_tid_to_lmac(15) == 3, "tid15 lmac3");

    // DSCP -> TID (RFC 8325 reduced)
    CHECK(mtk_dscp_tid(46) == 6, "EF -> VO tid6");
    CHECK(mtk_dscp_tid(56) == 6, "CS7 -> VO tid6");
    CHECK(mtk_dscp_tid(34) == 4, "AF41 -> VI tid4");
    CHECK(mtk_dscp_tid(40) == 4, "CS5 -> VI tid4");
    CHECK(mtk_dscp_tid(10) == 2, "AF11 -> BK tid2");
    CHECK(mtk_dscp_tid(8) == 2, "CS1 -> BK tid2");
    CHECK(mtk_dscp_tid(0) == 0, "BE -> tid0");
    CHECK(mtk_dscp_tid(20) == 4, "AF22 -> VI tid4");

    // eth classifier: VLAN PCP 6 -> TID 6; IPv4 DSCP 46 (ToS 0xB8) -> TID 6; ARP -> 0
    {
        uint8_t vlan[18] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0x81, 0x00, 0xc0, 0x00, 0x08, 0x00};
        CHECK(mtk_eth_tid(vlan, sizeof(vlan)) == 6, "VLAN PCP6 -> tid6");
        uint8_t ip[20] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0x08, 0x00, 0xb8, 0x00, 0, 0, 0, 0};
        CHECK(mtk_eth_tid(ip, sizeof(ip)) == 6, "IPv4 EF -> tid6");
        uint8_t arp[14] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0, 0x08, 0x06};
        CHECK(mtk_eth_tid(arp, 14) == 0, "ARP -> tid0");
        CHECK(mtk_eth_tid(ip, 14) == 0, "short ipv4 -> tid0");
    }

    printf(fails ? "WMM mapping test FAILED\n" : "WMM mapping test OK\n");
    return fails;
}
