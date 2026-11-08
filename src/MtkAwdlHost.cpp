// MtkAwdlHost.cpp — AWDL host-side window probe (P1).
//
// Goal of P1: prove the firmware lets us park on an AWDL social channel (6/44/149)
// long enough to transmit a vendor Action frame while the STA link stays up.
// Everything here is bounded and one-shot (triggered ~30 s after hwInit) so a
// failure cannot disturb normal Wi-Fi; results are published as MtkAwdlProbe*.
//
// ROC format is the one already used by the join path (UNI_ROC 0x27, ACQUIRE).
#include "MtkWlan.hpp"

namespace {
constexpr uint8_t UNI_ROC = 0x27;
constexpr uint8_t BSS_IDX = 0;
inline void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
inline void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
constexpr uint8_t kAwdlChannel = 6;      // 2.4 GHz social channel
constexpr uint32_t kAwdlWindowMs = 200;  // bounded park time
constexpr uint32_t kAwdlSettleTicks = 30;   // give the firmware ~60 ms to switch
constexpr uint32_t kAwdlEndTicks = 400;     // abort the park after ~800 ms
} // namespace

// Independent ROC so the join path's token/channel state is untouched.
int MtkWlan::awdlRoc(uint8_t ch, uint32_t ms) {
    uint8_t req[4 + 24] = {};
    uint8_t *t = req + 4;
    awdlRocTok++;
    put16(t, 0);                       // UNI_ROC_ACQUIRE
    put16(t + 2, 24);
    t[4] = BSS_IDX;
    t[5] = awdlRocTok;
    t[6] = ch;
    t[8] = ch > 14 ? 2 : 1;            // band
    t[10] = ch;
    t[13] = ch;
    t[15] = 0;                         // reqtype
    put32(t + 16, ms);
    t[20] = 0xff;
    return uniCmd(UNI_ROC, req, sizeof(req), false, nullptr, 0, nullptr);
}

int MtkWlan::awdlRocAbort() {
    uint8_t req[4 + 12] = {};
    uint8_t *t = req + 4;
    put16(t, 1);                       // UNI_ROC_ABORT
    put16(t + 2, 12);
    t[4] = BSS_IDX;
    t[5] = awdlRocTok;
    t[6] = 0xff;
    return uniCmd(UNI_ROC, req, 4 + 12, false, nullptr, 0, nullptr);
}

// One-shot window probe: park on the AWDL channel, emit a broadcast vendor
// Action frame (Apple OUI 00:17:F2), then release the park.
void MtkWlan::awdlService() {
    if (awdlState == 0) {
        if (awdlProbeAt && ticks >= awdlProbeAt) {
            awdlProbeAt = 0;
            if (!linkUp) { setNum("MtkAwdlProbe", 0xffff); return; }
            int rc = awdlRoc(kAwdlChannel, kAwdlWindowMs);
            setNum("MtkAwdlProbeRoc", (uint32_t)rc);
            awdlState = 1;
            awdlDeadline = ticks + kAwdlSettleTicks;
        }
        return;
    }
    if (awdlState == 1 && ticks >= awdlDeadline) {
        // Broadcast vendor Action frame: minimal valid AWDL-style frame.
        uint8_t f[64] = {};
        uint32_t n = 0;
        f[n++] = 0xd0; f[n++] = 0x00;                 // Action, flags 0
        f[n++] = 0; f[n++] = 0;                        // duration
        for (int i = 0; i < 6; i++) f[n++] = 0xff;     // DA broadcast
        memcpy(f + n, mac, 6); n += 6;                 // SA
        for (int i = 0; i < 6; i++) f[n++] = 0xff;     // BSSID broadcast
        f[n++] = 0; f[n++] = 0;                        // seq
        f[n++] = 0x7f;                                 // vendor specific
        f[n++] = 0x00; f[n++] = 0x17; f[n++] = 0xf2;   // Apple OUI
        f[n++] = 0x00;                                 // AWDL type placeholder
        uint32_t pad = 16;
        while (pad-- && n < sizeof(f)) f[n++] = 0;
        bool ok = txMgmt(f, n, 13);                    // subtype 13 (action)
        setNum("MtkAwdlProbeTx", ok ? 1 : 0);
        awdlState = 2;
        awdlDeadline = ticks + kAwdlEndTicks;
        return;
    }
    if (awdlState == 2 && ticks >= awdlDeadline) {
        int rc = awdlRocAbort();
        setNum("MtkAwdlProbeAbort", (uint32_t)rc);
        setNum("MtkAwdlProbe", 1);
        awdlState = 3;
    }
}
