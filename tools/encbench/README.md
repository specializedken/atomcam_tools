# encbench (fork only, branch `encoder-tuning`)

Objective A/B for AtomSwing stream quality. See `bench.py` header.

Runtime knobs that need no code (all on the command socket, `/scripts/cmd video ...`):

    video bitrate 3 <10-3000|auto>   main HEVC (ch0 = main AVC, ch1 = sub HEVC)
    video temper <0-255>             ISP 3D (temporal) noise reduction, 128 = mid
    video sinter <0-255>             ISP 2D (spatial) noise reduction, 128 = mid
    video fps <1-30|auto>            restarts the channels (disruptive)

Always restore afterwards: bitrate = hack.ini BITRATE_MAIN_HEVC, temper/sinter = value in /media/mmc/video_isp.conf.

## First sweep (garage, 2026-10-11, daylight, static scene, 1080p HEVC `video2_unicast`)

target kbps is what `video bitrate 3` was set to; "real" is measured from the stream (ffmpeg -c copy, 10 s).

    config                         real kbps  SSIM   temporal noise
    A  2000k temper128 sinter128      778     0.781   3.10
    B  3000k t128 s128                694     0.793   3.03
    C  2000k t200 s128                774     0.789   2.64
    D  2000k t255 s128                602     0.801   3.06
    E  2000k t200 s200                374     0.899   1.69
    F  1200k t200 s200                366     0.906   1.26
    G  2000k t128 s128 (A again)      666     0.791   3.20

* The encoder does NOT spend the target: 2000k and 3000k both give ~700 kbps on a static scene. Rate control
  (probably a "smart"/low-bitrate mode) is holding QP high, so raising the target changes almost nothing.
* Noise reduction changes bits and fidelity far more than bitrate does. Caveat: SSIM is against an averaged
  (noise-free) reference that goes through the same ISP, so it rewards denoising; look at the picture too.
* Run-to-run spread is ~+-0.01 SSIM (A vs G), so only differences above that mean something.

## Encoder struct decode and second sweep (garage, v7 with `video encdump|encrc|encgop`)

libimp channel numbers are **0 = main AVC, 1 = sub HEVC, 2 = main HEVC** (the hack's `video bitrate 3` is IMP ch2).
Struct sizes from libimp's getters: ChnAttr 112 bytes, RcMode 36 (mode + 8 words), GopAttr 24.

    attr word 11 = rc mode (8 on all three channels), 12 = target kbps, 13 = max kbps,
    14..19 = QP fields packed as int16 pairs, 20/21 = fps num/den, 22.. = gop attr (mode 2, length, numB, sameScene)
    encrc words (rc struct): 0 mode, 1 target kbps, 2 max kbps, 3 [initQP=-1 | 0x20 on HEVC, 0x17 on AVC],
                             4 [maxQP=51 | minQP=-1 (HEVC) or 3 (AVC main)], 5..8 undecoded
    `video bitrate N` sets target = 0.75 * max (1500/2000 for BITRATE_MAIN_HEVC=2000)

What moved what (a calm scene gives ~700 kbps whatever you ask; a busy/dark scene follows the target):

    word 1 target 1000/1500/2000/3000   busy scene: 1126 / ~1700 / 2013 / 2863 kbps real, SSIM unchanged (noise-bound)
    word 3 QP field 20..36              no trend in bitrate or SSIM
    word 4 maxQP 51 -> 32               no effect (calm scene)
    gop length 15 / 30 / 60             793 / 427 / 214 kbps real at the SAME SSIM (0.81): keyframes dominate the bitrate
    ISP temper+sinter 200 (earlier)     ~370 kbps, SSIM 0.78 -> 0.90 (rewards denoising, check by eye)

Caveats: the scene changed between runs more than once (first rows of two sweeps), SSIM is +-0.01..0.03 run to
run, and the GOP read-back lags one change behind (it applies at the next keyframe).

## AVC (IMP ch0) and the keyframe "pumping" (garage, night, `gopdrift.py`)

Stock AVC is capped at 960 kbps (`BITRATE_MAIN_AVC=-1800` is negative, so `set_icamera_config.sh` skips it).
`gopdrift.py` groups per-frame size / SSIM / sharpness by frames since the last I frame.

* Both codecs put most bits into the I frame (AVC 176 kB vs ~2.5 kB P; HEVC 66 kB vs ~2 kB).
* AVC sharpness (Laplacian var) erodes through the GOP (558 -> 547) and snaps back at the next I frame; HEVC is flat
  (~493). That is the visible "drift".
* rc word 3 high half is the **I-frame QP** (AVC stock 23, HEVC 32). Word 5 (IP delta?) and minQP have no effect.

    AVC max 2500, word 3 high half = I-QP    kbps   I kB   SSIM    detail start->end of GOP
    23 (stock)                               1700   176    0.765   558 -> 547
    24                                       1805   174    0.765   534 -> 521
    28                                       1282   130    0.767   531 -> 522
    32                                        790    80    0.775   511 -> 503
    36                                        473    49    0.775   477 -> 470
    40                                        249    28    0.762   426 -> 421

  Value to write: `video encrc 0 3 $((QP*65536+65535))`. QP 32 halves the bitrate and the I frame, SSIM goes up.
* Some erosion remains at every QP (~1.5%); maxQP 40 or word 3 low half 30 shave a bit off it.
* `encgop 0 8` gave odd frame counts (GOP read-back lags / misreports); not trusted.

## AVC stutter at I-QP 28 (garage, user-observed via the on-screen clock)

I-QP 28 (I frame ~130 kB, ~1260 kbps): the clock sticks on one second, then jumps ~3 s. I-QP 32 (I frame ~80 kB,
~790 kbps): smooth. `arrival.py` from a wired host saw no gap >150 ms in either, so it is not the network; it only
shows when the viewer path (cam -> v4l2rtspserver queue -> wifi -> player) is pushed by the big keyframes.
Rule of thumb on this cam: keep AVC I frames <= ~80 kB (I-QP >= 32 at 1080p) and the bitrate under ~1 Mbps.

## HEVC I-QP sweep (garage, night, temper/sinter 112, HEVC alone)

    I-QP   kbps   I kB   P kB     SSIM    detail (flat over the GOP)
    32     1305-1450  97   5.5-6.8  0.727   516-531
    34      886       75   2.8      0.733   497
    36      663       57   2.0      0.745   470
    28     1854      104   10-11    0.728   540-554
    24     stream stalled
Value: `video encrc 2 3 $((QP*65536+65535))`. User picked 34. AVC + HEVC at the same time stutters (3 MB free RAM).

## HEVC candidate (garage, night, 2026-10-11)

I-QP 30, GOP 30, temper/sinter 112/112, target 1500 / max 2000: ~535 kbps real, ~81 kB I frame every 2 s, P ~2.4 kB,
flat detail, smooth clock. VLC over UDP shows an occasional "missing ref" (lost packets); over TCP ffmpeg saw none in 120 s.

## Daylight, garage (outdoor, busy scene): the keyframe size is the limit, not the QP

Everything above was tuned at night. By day the same HEVC I-QP 30 gives a 227 kB keyframe and the feed stalls ~2-3 s
every few seconds (decoder "Could not find ref"). Keyframe size vs smooth (one test client + Frigate):

    I-QP   I frame   kbps   arrival
    30     227 kB    910    6-12 fps, 5-7 gaps >300 ms per 20 s
    36     123 kB    710    8 fps, gaps
    40      97 kB    250    16 fps, 2 gaps
    42      72 kB    310    17-21 fps, ~clean   <- set on garage (IQP_MAIN_HEVC=42)
    44      51 kB    210    17-21 fps, clean

Same ~70-80 kB ceiling as the AVC stutter. A fixed QP is scene dependent: the same QP is 80 kB at night and 227 kB by day.
Kitchen (indoor, calm) is fine at QP 30. A real fix would cap the keyframe size, not set a QP.

GOP read-back: at the encoder's 20 fps the app returns requested + 20 (30 -> 50, 45 -> 65); at 15 fps it is exact.
Do not flip `video fps` on a live cam: every change restarts the channels and leaves the RTSP server in a bad state.

## Why does a ~100 kB keyframe break the RTSP path? (garage, 2026-10-12, `v4l2rtspserver -vv`)

Pipeline: libimp -> `video_callback.c` `write()` into `/dev/video2` (v4l2loopback) -> `v4l2rtspserver` (live555, `ce80891`) -> TCP.

Facts read from the server log and source:
* One HEVC keyframe reaches the server as **7 queue entries at once**: VPS, SPS, PPS (from the encoder), the same three again
  (`repeatConfig`, because `-c` is not passed) and a **single IDR NAL of 200-230 kB**. The default queue (`-Q 5`) is full by the 6th
  entry, so the first two (VPS, SPS) are deleted on every keyframe (harmless: they are repeated).
* The IDR is handed to live555 ~10 ms after capture, but the sink then needs ~100-160 ms to send it; the following P frames
  pile up in the queue (`queue:3..4` of 5 right after an IDR, `queue:9` once with `-Q 20`). Five entries at 20 fps is only 250 ms of slack.
* The server never truncates (`OutPacketBuffer::maxSize` follows the V4L2 buffer size; no "truncated" lines).
* Audio (1920-byte chunks every ~120 ms) overflows its own queue all the time ("Queue full" lines, not video).
* TCP on the cam during a 20 s test: +17 retransmits (all fast retransmits), 0 RTOs. Wifi is -31 dBm, ping 0% loss.

Experiments at I-QP 30 (227 kB keyframe, the stalling case): `-Q 20 -c` did **not** fix it (client still logs "Error constructing the frame RPS").
So the queue depth / duplicated config alone is not the cause. What is established is only the size dependence (>= ~97 kB gaps, <= 72 kB clean) and that
the sink needs >100 ms per such keyframe. Unproven candidates: live555 packetising a 200 kB NAL on one MIPS thread, burst loss on the
cam's wifi TX queue followed by TCP head-of-line blocking, and the 50 KB `SO_SNDBUF` that live555 sets on RTSP client sockets.

Other libimp encoder entry points that exist and are untested: `IMP_Encoder_SetChnQpIPDelta` (I vs P QP offset: would shrink keyframes without
coarsening P frames), `SetChnQpBounds`, `SetChnQp`, `SetChnGopLength` (may avoid the +20 of `SetChnGopAttr`), `SetStreamBufSize`.
A software cap is also possible in `video_callback.c`, which sees every frame: measure the last IDR size per channel and nudge the I-QP.

## v13: loopback buffers 2 -> 8 (garage, daylight, I-QP 30): did NOT fix it

v4l2loopback sizes each buffer as width*height*4 bytes = 8.3 MB at 1080p (2 buffers = 16.6 MB, seen in /proc/vmallocinfo), so only
`max_buffers=2` fit. v13 advertises 640x360 on the loopback format (0.9 MB buffers) and loads the module with `max_buffers=8`
(7.4 MB total, confirmed on both cams). Result at I-QP 30 by day: 9-12 fps, 5-7 gaps >150 ms and ~3.0 s stalls per 20 s, 9 decode
warnings in 40 s - the same as before (6-12 fps). So a 2-slot loopback ring is not the main cause; the changes stay (less RAM, harmless).
Open: where frames are lost (libimp stream buffer vs the consumer thread's write()+callback time, which grows with frame size).

## Root cause: live555's 50 KB SO_SNDBUF (garage, daylight, 2026-10-12)

`video stats` showed the encoder + callback delivering every frame on time, so the loss is in the RTSP server. live555 requests
SO_SNDBUF 51200 on each client socket; a 227 kB keyframe burst does not fit and its RTP packets are dropped. `shim/sndbuf.c` raises the
request to 512 KB (glibc build, `LD_PRELOAD`). I-QP 30 by day: 6-12 fps + 3 s stalls -> 18.5-20.9 fps, gaps <= 0.8 s, 1 decode warning / 40 s.
I-QP 26: SSIM 0.89, smooth; I-QP 22: 1977 kbps, ~1 s gaps (rate cap). Keyframes are capped at ~227 kB by the encoder.


## rc struct: what the nine words of `video encrc` really are (decoded from libimp.so, 2026-10-12)

`IMP_Encoder_GetChnAttrRcMode` is a plain 9-word copy from channel offset +196; `IMP_Encoder_SetChnAttrRcMode` converts the words into the
encoder's own rate-control parameters (helper at 0x71210 in the 4.37.1.166 `libimp.so`, jump table by mode) and stores the raw words.
Modes: 0 fixed QP, 1 CBR, 2 VBR, **4 and 8 share one mapping** (the capped modes; the app uses 8 on all three channels), 3/5/6/7 are rejected.
Mapping for modes 4 and 8 (cross-checked against `SetChnQpBounds` / `SetChnQpIPDelta`, which write the same fields):

| word | field | vendor value | notes |
|---|---|---|---|
| 0 | mode | 8 | |
| 1 | target bitrate, kbps | 0.75 x max | |
| 2 | max bitrate, kbps | | |
| 3 low | initial QP | -1 (auto) | |
| 3 high | **min QP** | 23 AVC / 32 HEVC | what `video iqp` / `IQP_*` set. A floor for every frame, not the keyframe QP |
| 4 low | max QP | 51 | |
| 4 high | I/P QP delta | -1 (auto) | |
| 5 low | P/B QP delta | -1 (auto) | high half unused |
| 6 | rate-control options (bit mask) | 1 | |
| 7 | **max picture size, kbit** | 1920 (1080p) / 640 (sub 640x360): the image width | x1000 bits = 240 kB, just above the ~227 kB keyframe ceiling seen on garage by day. libimp **raises it to a floor derived from the bitrate** (600 became 1352 = 169 kB on a 2000 kbps channel) |
| 8 low | max PSNR, dB | 48 | x100 in the encoder |

What was measured on kitchen (daylight, static scene, 20 fps):
- Words 7 (1920 -> 1000/600/300), 8 (48 -> 40/34/28) and word 4 high (IP delta 0/3/6) changed on the **running** channel moved nothing: keyframe max stayed 140-150 kB.
  The runtime path (`SetChnAttrRcMode` on a live channel) evidently only updates the dynamic fields (bitrate, QP bounds); the rest is fixed at `CreateChn`.
- Min QP 30 -> 26 -> 20 left the keyframe at 123-130 kB (the rate control already picks a higher QP for it) and made the P frames 4-8x bigger; QP 38 shrank keyframes to 55 kB.
- So on a calm scene the keyframe size is decided by the rate control's own budget, and garage's by-day ceiling is the max picture size (word 7). Both are creation-time.

Wired in `libcallback/video_control.c` (v18): `video rc <ch 0|1|3> [<field> [<value>|auto]]` (fields `initqp maxqp ipdelta pbdelta options maxpic maxpsnr`;
no field = print the live decoded values, `*` marks a user override) and the hack.ini keys `<FIELD>_<MAIN_AVC|SUB_HEVC|MAIN_HEVC>`
(`INITQP MAXQP IPDELTA PBDELTA RCOPT MAXPIC MAXPSNR`), read by the `CreateChn` hook, i.e. they take effect at boot / when the app (re)creates the channel.
Absent key = vendor value. Words 6 (options) and the exact unit of 8 are inferred from the field order, not yet proven by a measurement.

### Creation-time test on kitchen (v18, one boot, three channels, `video stats` max frame size, 25 s windows)

| key | vendor | set | frame max before -> after | result |
|---|---|---|---|---|
| `IPDELTA_MAIN_AVC` (ch0, not streamed) | 3 | 10 | 115 kB -> 192-194 kB | **+68%: the I/P delta is the keyframe-size knob** (applies at CreateChn only) |
| `MAXPIC_MAIN_HEVC` (ch2) | 1920 | 600 | ~150-171 -> 156 kB | read back **1352**: libimp floors max picture size (169 kB at 2000 kbps max); not a clamp to 75 kB |
| `MAXPSNR_SUB_HEVC` (ch1, not streamed) | 48 | 30 | 18 -> 15-18 kB | no measurable effect (inconclusive; unit/option dependence not understood) |

The vendor values also differ per channel: main AVC has min QP 23, I/P delta 3 and P/B delta 3; sub HEVC has max picture 640; main HEVC has I/P and P/B delta -1 (auto).
Windows right after a boot are contaminated (start-up IDR, a ~365 ms gap): wait ~1 minute before reading `video stats`.
