# AGENTS.md

Guidance for AI coding agents working in this fork.

> `CLAUDE.md` is a **symlink to this file** so Claude Code picks it up. Edit `AGENTS.md`, never `CLAUDE.md`.
> This file is fork-only context. **Drop it (and the `CLAUDE.md` symlink) from anything sent upstream.**

## What this is

Fork of [mnakada/atomcam_tools](https://github.com/mnakada/atomcam_tools) (custom firmware for ATOM Cam / **AtomSwing**).
Upstream has pan/tilt but **no ONVIF**. This fork adds a small ONVIF Device/Media/PTZ service **on the cam itself**,
so NVRs (we use Frigate, with autotracking) can drive the two outdoor Swings directly. It is the upstream-able
successor of an external Python shim that ran on the server and talked to each cam over HTTP (that shim, its repo and its container were deleted 2026-10-08). Same behaviour, ported to Go and run on the cam.

- Fork: `origin` = https://github.com/specializedken/atomcam_tools, `upstream` = mnakada/atomcam_tools
- Branch: **`onvif-ptz`** (pushed to origin). Feature commit `35fe9a2`, on top of upstream `313048b` (Ver.2.5.19). This is the **Go daemon** line (tag `onvif-go-v1` = state of 2026-10-11); it is what kitchen runs.
- **Direction since 2026-10-11:** upstream PR [mnakada/atomcam_tools#118](https://github.com/mnakada/atomcam_tools/pull/118) (open, by `bi119aTe5hXk`) already adds ONVIF via
  roleoroleo's `onvif_simple_server`. The user decided to **contribute our PTZ work on top of #118 instead of a second ONVIF server** - see "Merging into PR #118" below.
  Branches: `onvif-on-118` (= PR head + our 2 commits), `onvif-on-118-presets` (+ presets/home, FOV, speed/status/continuous, README = **the PR branch**, 8 commits on top of #118's 8). `pr118` = local fetch of the PR head (`git fetch upstream pull/118/head:pr118`).
- **PR opened 2026-10-12: [mnakada/atomcam_tools#146](https://github.com/mnakada/atomcam_tools/pull/146)** (`specializedken:onvif-ptz-118` -> `main`; #118's 8 commits + ours, authorship kept). Pushed as `origin/onvif-ptz-118` (= `onvif-on-118-presets` `1052b55`).
  Chosen over a PR against `bi119aTe5hXk:onvif` because that author said on 2025-05-01 they could not finish and were closing #118 (it is still open). A comment on #118 links #146.
  **Do not push other branches or force-push `onvif-ptz-118` without being asked; answer review comments on #146 rather than reopening the question.** `AGENTS.md`/`CLAUDE.md` are not on that branch and must stay off it.
  Fork-only line (`onvif-ptz`, Go daemon) is unchanged and still not what runs.

## What the branch adds

| Path | What |
|---|---|
| `custompackages/package/onvif/src/` | the daemon: `main.go` (HTTP/SOAP server, flags), `soap.go` (XML parse + all ONVIF ops), `cam.go` (command-socket transport, moves, presets), `main_test.go` |
| `custompackages/package/onvif/{Config.in,onvif.mk}` | buildroot package; builds with the image's Go (`GOARCH=mipsle CGO_ENABLED=0`), **no UPX** |
| `custompackages/package/Config.in`, `configs/atomcam_defconfig` | registers the package; `BR2_PACKAGE_ONVIF=y`; `onvif` added to `BR2_STRIP_EXCLUDE_FILES` |
| `overlay_rootfs/etc/init.d/S76onvif`, `overlay_rootfs/scripts/onvif.sh` | start/stop. Does nothing unless `ONVIF_ENABLE=on` in hack.ini; every failure path `exit 0` so it can never block boot |
| `overlay_rootfs/scripts/webcmd.sh` | `onvif` command (stop + start) so the UI applies changes without reboot |
| `web/source/vue/Setting.vue`, `i18n-en.yaml`, `i18n-ja.yaml` | ONVIF switch + max speed (AtomSwing only, `v-if="isSwing"`) |
| `README.md` | feature bullet + settings section (Japanese, like the rest of that file) |

Design facts:
- Talks to the cam's command socket **TCP `localhost:4000`**: `move` -> `pan tilt hflip vflip idle`;
  `move <pan> <tilt> <speed 1-9> <pri 0-3>` blocks until done (reply is NUL-terminated). Lower pri number cancels an in-flight move.
  Source of truth: `libcallback/command.c`, `libcallback/motor.c`.
- hack.ini keys: `ONVIF_ENABLE` (default off), `ONVIF_PORT` (default 8000), `ONVIF_MAX_SPEED` (1-9, default 9).
  Presets persist in `/media/mmc/onvif_presets.json`.
- Moves are absolute only (pan 0-355, tilt 0-180). `ContinuousMove` = head for the mechanical limit; `Stop` = pri-0 move to current position.
  `GetStatus` reports MOVING while any daemon-issued move is in flight. FOV space: +-1 = half FOV. The Go daemon's `-hfov 108 -vfov 54` defaults were an **unverified guess** (no provenance); measured 2026-10-11 on garage: ~20 px/deg near the image centre = **~96 x 54 deg** (see Open work 2b).
- pan/tilt direction assumed +1 (the cam already flip-corrects). **Verified only on the two h+v-flipped cams**, not on unflipped ones.
- No auth (WS-Security), no zoom, no ONVIF home position, no EFlip. **Unauthenticated on the LAN** - the UI tooltip and README say so;
  a maintainer may want auth before merging.

## Merging into PR #118 (decided 2026-10-11)

#118 = `onvif_simple_server` (C, GPLv3, pinned commit `45dbabf`, 4 local patches) behind lighttpd on **port 80** (`/onvif/device_service`), WS-Discovery, PullPoint events + motion alarms,
multi-profile RTSP, WS-Security (`ONVIF_USER`/`ONVIF_PASSWD`). Shell PTZ glue in `overlay_rootfs/scripts/onvif_ptz.sh`. Same base as ours (`313048b`), so no rebase vs upstream is needed.
It uses the **same file names as our package** (`S76onvif`, `onvif.sh`, the `webcmd.sh` `onvif` hook, `ONVIF_ENABLE`, `Setting.vue`, i18n, `Config.in`, defconfig): the two cannot coexist.

| Ours | Fate |
|---|---|
| `libcallback/{motor.c,command.c,libcallback_hook.md}` (`relmove`) | **contributed** (commit `6212966` on `onvif-on-118`) |
| `onvif_ptz.sh` rewrite: relative via `relmove`, real Stop, per-move moving flag | **contributed** (`e52f839`) |
| presets + home hooks in `onvif_ptz.sh`/`onvif.sh` (`/media/mmc/onvif_presets.txt`, `N=name pan tilt 0` lines, preset `home` = home position) | **follow-up**, stacked branch `onvif-on-118-presets` (`511a2fa`); needed on our cams because Frigate returns to `home` |
| Go daemon, our `S76onvif`/`onvif.sh`/`webcmd.sh`/UI/README/defconfig lines | **dropped from the PR line**; stay on `onvif-ptz` for kitchen |
| `AGENTS.md`, `CLAUDE.md`, `.gitignore` additions, `vendor-fw/` | never upstream |

Bugs found in #118's PTZ script (fixed by our commits): `relative` multiplied degrees by 30 as if they were speeds; `stop` only deleted a flag file and never stopped the motor.
Server contract (read from `ptz_service.c`): hooks run via `system()` and block; Generic space values arrive in steps (= degrees with max_step 355/180); FOV space is -100..100 (% of **half** FOV)
converted with a **hard-coded 63 x 37 deg FOV** (the Swing measures ~96 x 54 deg, so FOV moves undershoot ~1.5x/1.45x - patch `0005` fixes it, see Open work 2b); `get_presets` wants `N=name x y z` lines;
`set_preset <num|-1> <name>`; names must not contain spaces; `goto_home_position`/`set_home_position` are separate hooks.
Their `build_all` change (`make atomcam_defconfig` after copying packages) removes the "run build_all twice" quirk; it built first try with the prebuilt `Ver.2.5.5` builder image
(their Dockerfile ubuntu:22.04 change is not needed for that).

**Autotracking-quality work (2026-10-11, `onvif-on-118-presets` `5df387f` + fixes; first version running on garage, image `324023a1...2186a`):**
- *Why:* Frigate tight-loops `GetStatus` while a move is in flight and always sends `Speed=1`. Measured on garage (idle): `GetStatus` median 83 ms (min 62, p90 133; 5 concurrent = 436 ms, the single core serialises);
  our two shell hooks were ~50 ms of that (20x `get_position`+`is_moving` = 0.99 s on the cam), a bare `nc` round trip is ~14 ms. RAM is 87.7 MB total, ~3.8 MB free (25 MB cache): a resident Go daemon is the worse trade; per-request CGI is cheap.
- *`ptzstatus`* (libcallback): `pan,tilt,0,moving` in one line; `moving` = move/relmove/contmove in flight (so cruise/app moves count too). `get_position` hook is `echo ptzstatus | /usr/bin/nc localhost 4000` (no script, no awk).
  Server patch `0006` lets `get_position` return an optional 4th field `moving` and skips the second hook run.
- *Latest wins:* the cam dismisses a move issued at the same/lower priority while busy (arbitration is in our `motor.c`, not the SDK). `run_move` first cancels in-flight script moves (pri-0 move to current position)
  and retries up to 10 x 0.15 s on `dismiss`. Simulated only.
- *Speed:* `0006` passes `Speed/PanTilt` (max of x,y, -1 if absent) as a 4th arg to `jump_to_abs`/`jump_to_rel`; script maps `ceil(speed * ONVIF_MAX_SPEED)` (hack.ini key, default 5; both cams have 2, which is what Frigate's `movement_weights` were calibrated at).
  Frigate calls `RelativeMove` with `Speed=1` on every tracking move and polls status until `IDLE`.
- *Continuous:* vendor decompile (`fn_0044d860.c` ~L880-930, `FUN_0040f7f4`): `local_sdk_motor_move(h, v, speed, done, cancel, mode)` where h/v are **direction commands 0/1/2** (0 = none; the app swaps 1<->2 when the cam is flipped),
  `local_sdk_motor_stop(done, cancel, 1)` stops. `contmove <h> <v> [speed] [pri]` / `contstop` take the raw values; **which of 1/2 is which is unverified**, so `onvif_ptz.sh` calibrates once per boot with a 0.4 s blip at speed 1
  (skipped within 25 deg/330/155 of the ends; falls back to a 30 deg relmove nudge). 6 s dead-man timer stops a drive nobody stops. Patch `0006` also calls the move hook with 0.0 for a zero-velocity axis (it used to skip it).
  Unknown: whether the SDK soft-limits a continuous drive at the end of travel (the dead-man is the backstop) - watch the first real test.
- Sim: `vendor-fw/sim/` (fake `/scripts/cmd`, `run.sh`..`run3.sh`; see its README.txt).
- **Real-cam results on garage (2026-10-11, autotracking paused):** GetStatus median **41-51 ms (was 83)**, 5 concurrent 148-222 ms (was 436); generic +-5 exact; FOV x=10 -> +5 deg (was +3); newest request wins
  (a +40 pan cut short by a +6 tilt, both calls return); presets/GotoPreset home exact. **Speed has no measurable effect on move duration** (20 deg: 564 ms at 0.2 vs 555 ms at 1.0): the plumbing works, the SDK barely reacts to the speed argument.
- **Continuous on the real SDK:** calibration measured `1 -1` (raw command 1 = reported pan +/right, tilt -/down), ~**47 deg/s at cam speed 1**, Stop took 210 ms and the head coasted ~10 deg past it,
  first call per boot takes ~2.2 s (calibration blip). **Bug found:** the SDK calls the `done` callback right when a continuous command is accepted, so `ptzstatus` said IDLE during the drive (fixed: flag clears on contstop / new move /
  700 ms stall). End-of-travel behaviour of the SDK is still unknown, so the script's watchdog polls `ptzstatus` and stops 30 deg before either end (+ 6 s dead man). 255 deg was reached from 143 in ~3.5 s with no issue.
- **Testing lessons (2026-10-11):** (1) with autotracking ON a symmetric +-20 deg test ended +26 deg off - Frigate was following something; pause it first with MQTT:
  `docker exec frigate python3 -` + paho to `localhost:1883`, topic `frigate/garage/ptz_autotracker/set` payload `OFF`, read `.../state`, and **always send `ON` afterwards** (use a trap). (2) With `user=` empty the server still
  demands a WS-Security UsernameToken: a raw curl without one gets `Sender not Authorized`; clients must send an (empty) token, so Frigate needs `user: ""`. (3) Measuring server-side latency needs a real token
  (a faulting request only measures lighttpd+CGI: ~21 ms floor). Raw numbers: ping 2.3 ms, lighttpd-only ~10 ms, CGI no hook 21 ms, one script hook ~19 ms.
- **Garage runs v5 since 2026-10-11 21:24 (`bc590f5f...704efd8`, built from `onvif-on-118-presets` `10927d0`; image `vendor-fw/work/on118/rootfs_hack.squashfs`).** Verified on the cam: `MoveStatus` is MOVING during a continuous drive
  (6/6 samples) and IDLE after Stop (Stop 243 ms, ~10 deg coast); a drive with **no Stop** was ended by the watchdog at pan 340 (15 deg short of the 355 end, status IDLE); regression suite all PASS
  (GetStatus median 39 ms, min 30, p90 61; 5 concurrent 165 ms; FOV x=10 -> +5; newest request wins; presets). Only `libcallback.so` and `onvif_ptz.sh` differ from the previous image.
  Not exercised on the cam: the 6 s dead man (the limit watchdog fires first from home; sim only), tilt-axis continuous, diagonal drives, real Frigate autotrack of a person.
- **Speed (corrected 2026-10-11):** the earlier "speed has no effect" was wrong - it only compared cam speeds 1 vs 2. Measured 30 deg pan via `cmd relmove`: cam speeds **1,2,3 = ~0.69 s (a floor)**, 5 = 0.49 s, 7 = 0.35 s, 9 = 0.29 s.
  `ONVIF_MAX_SPEED=2` (set to match the old shim's calibration) therefore pinned Frigate's `Speed=1` to the slow plateau. `onvif_ptz.sh` (`7534732`) now maps ONVIF (0,1] onto cam speeds **4..MAX** (`SLOW_SPEED=3`),
  continuous drives stay capped at 3 (~47 deg/s; the limit watchdog margin was sized for that). Over ONVIF (incl. ~100 ms overhead): Speed 1.0 ~350-440 ms, 0.5 ~460-500, 0.1 ~690-750 (was ~550-620 for all).
- **Both cams are on v6 since 2026-10-11 ~21:50 (`fdddcd35...7792`, = v5 + the speed mapping) with `ONVIF_MAX_SPEED=9` in their SD hack.ini; the Go daemon line (`onvif-ptz`) is no longer running anywhere.**
  Kitchen verified after the swap: calibration `1 -1` (same as garage), MOVING during a drive, Stop 177 ms. Frigate: `calibrate_on_startup: true` on both (user asked for it to stay on), kitchen on `:80` with `user: ""`/`password: ""`.
  After the Frigate restart both calibrations completed without errors and **Frigate wrote the new `movement_weights` into config.yml by itself**: garage 0.176/0.191/0.185 (was 0.191/0.169/0.204), kitchen 0.135/0.162/0.197 (was 0.162/0.169/0.329).
  Config backups: `config.yml.pre-calibrate-speed`. Still unverified: a real person being tracked at speed 9 (faster may overshoot / blur; the knob is `ONVIF_MAX_SPEED`, 4-9 are all distinct, 1-3 identical).
- Rollback files on SD: garage `rootfs_hack.squashfs.bak-20261011` (Go daemon image), `.bak-118-v2-20261011` (`1b1deb1c`), `.bak-118-v3-20261011` (`324023a1`, status bug), `.bak-118-v5-20261011` (`bc590f5f`, v5);
  kitchen `.bak-20261011` (Go daemon image `4209e670`) and `.bak-20261008`. `hack.ini.bak-speed-20261011` on both.
  A flash is: `cat img | ssh ... 'cat > update/rootfs_hack.squashfs.part && mv ...'`, sha256 on the cam, then the **user reboots** (ssh reboot is a no-op); back in ~1 min.

**Image recipe for the #118 stack** (minimal diff again; do not flash the full build): official 2.5.19 rootfs + `etc/init.d/S76onvif`, `etc/lighttpd/lighttpd.conf`, `etc/{onvif_notify_server,wsd_simple_server}/`,
`scripts/{onvif,onvif_event,onvif_ptz,webcmd,webhook}.sh`, `usr/bin/{onvif_notify_server,wsd_simple_server}`, `usr/lib/libjson-c.so*` + `libmbedcrypto.so*` (**not in the official rootfs**, the new daemons need them),
`var/www/onvif/`, the new web bundle + `index.html`, and the rebuilt `lib/modules/libcallback.so` (132424 bytes; official 129252). Do NOT take `etc/shadow` or the other ~1200 timestamp-noise files.
Output `vendor-fw/work/on118/rootfs_hack.squashfs`, gzip/128K, `-noappend`; check `bytes_used` (u32 at offset 0x28) rounded up to 4096 == file size (init's check).
Pre-flight without a cam: `busybox sh -n` on every script, and the cam's own `lighttpd -t` under modern qemu (`qemu-user-static` in `debian:12-slim`, `-L <rootfs>`, run from `etc/lighttpd/`, `touch auth.conf` first - it is generated at runtime).

## Commands

There is **no Go on the host**; use Docker.

    # unit tests + vet + format check (fake command socket, no cam needed)
    cd custompackages/package/onvif/src
    docker run --rm -v "$PWD":/w -w /w golang:1.22 sh -c 'gofmt -l .; go vet ./... && go test -race -count=1 ./...'

    # cross-compile like the firmware does
    docker run --rm -v "$PWD":/w -w /w golang:1.22 sh -c 'GOARCH=mipsle GOOS=linux CGO_ENABLED=0 go build -ldflags "-s -w" -trimpath -o /tmp/onvif .'

Full firmware build (upstream flow, `make build`, done by hand with `docker compose`):

    docker pull atomtools/atomtools:Ver.2.5.5          # 13.6 GB, already pulled on this machine
    docker compose up -d
    docker compose exec -T builder /src/buildscripts/build_all > build.log 2>&1
    docker compose down                                 # when finished
    # outputs: target/rootfs_hack.squashfs, target/factory_t31_ZMC6tiIDQN, atomcam_tools.zip (all gitignored, root-owned)

Gotchas learned the hard way:
- **A brand-new package fails the first `build_all`** (`oldconfig` runs before `custompackages/` is copied in, so
  `BR2_PACKAGE_ONVIF` is "NEW" and `silentoldconfig` aborts). **Just run `build_all` a second time.** Upstream quirk, not our bug.
- The qemu-user in the builder image is 2.5 and **cannot run any Go binary** (`fatal error: sigaction failed`, go2rtc dies too).
  To run the mipsle binary use a modern qemu: `docker run --rm -v ... golang:1.22 sh -c 'apt-get install -y qemu-user-static ...; qemu-mipsel-static ./onvif ...'`.
- Web UI check without a firmware build: copy `web/{webpack.config.js,package*,source}` to a temp dir and run `npm install && webpack --mode production` in `node:18`.
- Launcher logic check: run `overlay_rootfs/scripts/onvif.sh` under `busybox sh` with stub `/scripts/cmd` and `/usr/bin/onvif`.

## Status of verification

Verified: Go tests (-race); mipsle cross-compile (5.6 MB); real `onvif-zeep` client against the Go binary (`client_check.py` from the shim repo);
the actual mipsle binary from the built image under qemu 7.2 (status IDLE->MOVING->IDLE, FOV move landed at pan 127 from 100,
preset saved, unknown op faults); launcher under busybox in 8 cases; web bundle builds; **full firmware build exit 0** and the squashfs contains
`/usr/bin/onvif`, `S76onvif`, `onvif.sh`, patched `webcmd.sh`, and the bundle with `ONVIF_ENABLE`.

**Real cam (2026-10-08): kitchen is flashed with the minimal-diff image and running our daemon.** Verified over ONVIF with onvif-zeep: tilt/pan relative moves
(tilt +y = up, confirmed by eye), MOVING/IDLE, ContinuousMove+Stop, presets set/goto/remove, faults, autostart across reboot (position returns to 164.0/77.0).
Frigate's kitchen `onvif:` points at `192.168.1.50:8000` (max speed 3 to match the old shim's calibration; `home` preset seeded by hand).
Not yet verified: a live Frigate autotrack, concurrent clients, the "Save home position" button (committed, not in any flashed image).

**Garage runs the #118 stack since 2026-10-11** (image `on118`, sha256 `1b1deb1c...e35b5` on 2026-10-11 morning, since replaced by `324023a1...` with FOV patch, speed/status/continuous; our Go daemon is gone from it). Verified over ONVIF with onvif-zeep (async API, run from inside the Frigate container):
generic RelativeMove pan/tilt +-5 exact out and back, FOV move works (but undershoots), MOVING seen mid-move, **Stop actually halts** (stopped at 161/179 on an 80 deg move), SetPreset/GetPresets/RemovePreset, GotoPreset `home` lands on 143.3/74.05.
Frigate garage `onvif:` = `192.168.1.129:80`, `user: ""` / `password: ""` (server has no `ONVIF_USER`; Frigate rejects *missing* creds with `NoneType` and rejects the old shim-era creds with "Sender not Authorized"),
validated and restarted, no autotrack error afterwards. Still unverified: a real person being tracked, concurrent clients, the 4.58/4.37 firmware matrix beyond ours (4.37.1.166-era).

## The cams and the update path (important)

- Cams: `garage` 192.168.1.129, `kitchen` 192.168.1.50 (DHCP). **Both were flashed with the fork's minimal-diff v2 rootfs on 2026-10-08** (stock 2.5.19 base + our files; `ATOMHACKVER` still reads 2.5.19), kernel unchanged: `#2 PREEMPT Sun May 11 07:01:36 UTC 2025`.
  They feed Frigate; both are outdoors. **The user has no easy physical access to them or their SD cards.**
- Update without the SD card: Web UI -> Maintenance -> "Custom update ZIP" + URL -> Update. The cam `curl`s the zip into `/media/mmc/update`
  and reboots; `initramfs_skeleton/init` unzips it, checks sizes, and `mv -f`s `rootfs_hack.squashfs` (and the kernel, if present) into place.
  A loose `rootfs_hack.squashfs` dropped in `update/` over Samba works too. **There is no backup of the old rootfs and no rollback**:
  if the new rootfs does not boot, the cam is stuck until someone reaches the SD card.
- **Do not flash our full build.** Diffed against the official 2.5.19 release (`gh release download Ver.2.5.19 -R mnakada/atomcam_tools`):
  file list matches except our 3 new files + the renamed web bundle, but **1229 existing files differ** (mostly embedded build timestamps; busybox is
  same size/version), 14 differ in size (incl. `usr/bin/videocapture`, `libfdk-aac`, `scripts/webcmd.sh` which is ours), `/etc/shadow` hashes differ,
  and our kernel differs by ~734 KB. Cannot prove those are harmless.
- **The minimal-diff approach is how both cams were updated** (see the image section below): unpack the *official* 2.5.19 `rootfs_hack.squashfs`, add only our files, repack
  (gzip, 128K blocks), ship only the rootfs (never the kernel). Kitchen: v1 via web UI Custom update, then v2 by streaming the squashfs over ssh to `/media/mmc/update/`. Garage: v2 via the web UI
  Custom update (its image also carried an `S21rootkeys` ssh-key patch because no key was trusted yet). Kitchen keeps a rollback copy `/media/mmc/rootfs_hack.squashfs.bak-20261008`.
  **2026-10-11:** both cams also have `rootfs_hack.squashfs.bak-20261011`, `hack.ini.bak-20261011`, `onvif_presets.json.bak-20261011` on the SD card, and host copies (sha256-verified) in `vendor-fw/backup/` (garage's backup is the Go-daemon image).
  Garage was then flashed with the #118 image by streaming it over ssh to `update/rootfs_hack.squashfs`. **`reboot` and `/scripts/reboot.sh` over ssh were silent no-ops on garage** (uptime unchanged); the user rebooted it by hand. Use the web UI reboot / ask the user.
  `id_atomcam` ssh works on both cams. Remote writes (flash, reboot, hack.ini) are blocked by the auto-mode classifier until the user says go in chat.
- **Never start an update/flash on a cam without the user's explicit go-ahead in that conversation.** Updating reboots a cam that Frigate depends on.

## Firmware exploration (2026-10-06 session; goal: relative moves, zoom, sturdier HEVC)

Goal: find out whether the stock firmware exposes more than we use. Nothing was run on a cam; no cam was touched.

- **Vendor firmware is NOT in the ATOM app APK** (`ATOM+-+...4.160.3_APKPure.xapk`, untracked in repo root; do not commit). The app fetches firmware from the vendor cloud, per device, with an account token.
  We stopped there on purpose: no replicating the app's request signing/auth against the vendor server.
  **UPDATE 2026-10-06:** public route found — the vendor (ATOM tech Inc.) lists its full firmware history on the official support page
  (`https://www.atomtech.co.jp/support/`, section "ATOM Camファームウェア", an embedded public Google Spreadsheet).
  Every release (Cam / Cam 2 / Swing, back to 2020) is a public Google Drive file (zip containing `demo.bin`) with the .bin SHA1 published in the sheet.
  No auth. AtomSwing: latest is 4.37.1.180 (2026/4/7); 4.37.1.166 (2025/3/10) is the 2.5.19-verified era and matches the cams' kernel date (2025-05-11).
  Downloaded + SHA1-verified both (180: `820633ce…e7c5`, 166: `2ea1d277…f2877`); saved under `/tmp/opencode/atomfw/` (volatile — move if wanted).
  Vendor forced-update path (their FAQ): copy `demo.bin` unrenamed to the SD root. The .bin is a signed `jz_fw` image (per-version signature header).
- **ssh to the cams works** (corrected 2026-10-08; an earlier note here claiming it was impossible was wrong). Stock sshd is key-only for root and reads the SD card's
  `/media/mmc/authorized_keys` (copied to `/root/.ssh` at boot by `S21rootkeys`; `S55sshd` only starts sshd if that file exists). Nothing needs baking into the rootfs.
  Both cams' SD files held the user's RSA key from install. On kitchen it has been replaced by the dedicated key `~/.ssh/id_atomcam` (ed25519, `atomcam-root`;
  backup of the old file: `/media/mmc/authorized_keys.bak-20261008`). Use `ssh -i ~/.ssh/id_atomcam root@192.168.1.50`. Garage still has the original RSA entry.
- **SMB** (guest, writable) only shares SD subfolders `record`, `time_lapse`, `alarm_record`, `update`; the SD root (where `rootfs_hack.squashfs` lives) is not shared, so SMB cannot read the firmware.
- **Stock 2.5.19 image** is unpacked in the session scratchpad (not persistent). It contains only the hack layer; the vendor stack (`iCamera_app`, `libimp.so`, `liblocalsdk.so`) lives in the cam's flash, so it is absent.
- **Vendor app partition decoded (Ghidra, 2026-10-06).** `demo.bin` = signed `jz_fw` image (signature header at byte 0) containing **two standard squashfs-tools 4.x images** (xz, compressed inodes+frags — the earlier "vendor superblock quirk"/header-patch theory was wrong; slice the original bytes and `unsquashfs` works):
  - **sqf1** = vendor **rootfs** (395 inodes, 1 MB blocks), magic `hsqs` at file offset **0x1F0040**: busybox, `init/`, `etc/`, drivers, `linuxrc`.
  - **sqf2** = vendor **app partition** (106 inodes, 512 KB blocks), magic at **0x5C0040**: `bin/iCamera_app`, `bin/assis`, `liblocalsdk*.so`, `libimp.so`, `init/{start.sh,app_init.sh,mnt.sh,factory.sh,wifi.sh}`. This is the vendor app stack the hack rootfs overlays on the real cam.
  - Analysis workhorse: `fw_166` (4.37.1.166, era-matched to the cams); `fw_180` extracted for comparison — its `iCamera_app` differs (different build) but the ONVIF code path + strings are **identical**. Ghidra 12.1.3 headless server (127.0.0.1:8089, `GHIDRA_MCP_ALLOW_SCRIPTS=1`) + `run_script_inline` (**body-only** — the bridge wraps it in a `GhidraScript` subclass) for decompilation; dumps under `/tmp/opencode/atomfw/dumps/` (volatile). Host objdump can't disassemble MIPS — use Ghidra. `iCamera_app` = MIPS o32, base 0x400000, **VA = file offset + 0x400000**; 3612 fns; the cloud-command handler is `iot_msg_process_handler` @ 0x44d860 (dump `fn_0044d860.c`).
- **Q1 — native relative moves: YES.** `iCamera_app` PTZ wrappers call the native SDK (`dumps/fn_0040f*`): `FUN_0040f844(H,V,sp)` -> `local_sdk_motor_move_rel_step(h,v,sp,cb,cb,2)` (**native relative step** = what `PTZ_relative_rotation` uses); `FUN_0040f894` -> `local_sdk_motor_move_abs_angle` (absolute — the only move our fork hooks); `FUN_0040f7f4(h,v,9)` -> `local_sdk_motor_move` (continuous primitive, `PTZ_continuous_rotation`, speed hardcoded 9); `FUN_0040f4bc()` -> `local_sdk_motor_stop`; `FUN_0040f450`/`0x46c` set/clear motor-state flags around moves. Our daemon's `RelativeMove` (read position + delta -> absolute, `soap.go`/`cam.go`/`TestRelativeMoveFov`) is behaviourally equivalent but could use the native step call if we ever hook more of the SDK.
- **Q2 — zoom: not a fixed-lens limitation.** `libimp.so` (Ingenic T31 IMP) exposes `IMP_FrameSource_SetFrameOffset` (digital crop/scale), `IMP_ISP_Tuning_SetFrontCrop`, `SetAutoZoom` + scaler/defog/DRC knobs. Nothing in `iCamera_app` uses them and the hooked SDK has no zoom symbol — **unhooked, not impossible**. Digital crop lowers effective resolution (poor fit for Frigate autotracking), consistent with the repo-source read.
- **Q3 — HEVC encoder: rich knobs in `libimp.so`.** `IMP_Encoder_SetChnAttrRcMode`/`..._Qp`/`..._QpBounds`/`..._BitRate`/`..._GopAttr`, `IMP_Encoder_GetChnAttr`, `IMP_Encoder_RequestIDR`, an `AL_*` software encoder (AVC+HEVC), and `local_sdk_video_set_parameters/fps/kbps`. `iCamera_app` only sets bitrate/fps/GOP, so RC-mode/QP/profile (in the attr struct to `IMP_Encoder_CreateChn`) are unhooked. Confirms the repo read: GOP/fps at CreateChn are hard maxima, GOP divides fps.
- **Stock ONVIF (`xcamera`) path — fully decoded, and it is a dead feature in these builds.** The vendor has an in-camera ONVIF server binary, `xcamera`, driven **entirely by the vendor cloud** (AWS IoT shadow) — no LAN/ONVIF discovery. All in `iot_msg_process_handler`:
  - Cloud command vocabulary (mapper `FUN_0044a8c4`, name->ID): powerOn 0x4cf, powerOff 0x4d0, upgrade 0x4d1, delDev 0x4d2, getLog 0x4d3, alarmAction 0x4d4, motionAlarmOn/Off 0x4d5/0x4d6, devReboot 0x4d7, sensorToken 0x4d8, setGmtOffset 0x4d9, setProperty 0x4da, setSubProperty 0x4db, setPropertyList 0x4e4, rtmpStart/End 0x4dd/0x4df, motorActive 0x4de, **onvifOn 0x4e0**, **onvifOff 0x4e1**, sirenOn/Off 0x4e2/0x4e3, webrtcChannel 0x4e5, resetService 0x4e8, custom_action 0x4e9 (PTZ verbs).
  - **onvifOn {url, md5}**: gate `FUN_00433b10()!=1`; parse `url`+`md5` from the cloud JSON; libcurl-GET `url` -> `/tmp/onvif/xcamera` (`downLoad`, `downloadfiles.c:81`, optional CA cert); then `md5sum /tmp/Test/factoryTestProcess` and `strncmp` vs the cloud `md5`. **The md5 check is on the *factory-test* binary, NOT the downloaded xcamera** (verified from raw bytes at VA 0x540df0; a copy-paste quirk from the factory-update flow — the xcamera binary itself is never integrity-checked). On match: queue `touch /tmp/onvif/.onvif`, set flag `DAT_0066dbec=1`, send cloud report 0x3f6, log "start onvif success".
  - **onvifOff**: queue `killall -9 xcamera` + `rm /tmp/onvif -rf`, clear the flag, send 0x3f6.
  - **PTZ** via custom_action 0x4e9: `PTZ_relative_rotation` -> `move_rel_step` (H/VRotationSteps, flip-signs via settings); `PTZ_set_position`/`PTZ_center` -> `move_abs_angle` (CenterType 1=h-max, 2=v-max, 3=reset); `PTZ_continuous_rotation` -> `local_sdk_motor_move` (speed 9, both-0 = stop). Shell commands go through an "exec-iCame" **SysV msgqueue IPC** (command queue `DAT_0068abf4` -> response queue `DAT_0068abf0`, synchronous send-then-wait-for-output); the **executor is `assis`** (`exec-shell-pool`/`shell_popen` — imports `fork/popen/system/opendir/access/stat` + the msgqueue primitives, no `execve`, no onvif awareness — it just runs the exact command string it's sent).
  - **Nothing ever execs `/tmp/onvif/xcamera`.** `iCamera_app` has **no `fork`/`execve` import** (only `system`, 22 call sites, none launch xcamera); `assis` has no `execve` either; the `.onvif` flag file is created but never read by any process; the onvif flag global is written by the two handlers and read only by the cloud property reporter (`FUN_0044ab2c`, `snprintf("%d", DAT_0066dbec)`); no binary in either image contains a launch reference to xcamera (exactly 3 `/tmp/onvif` literals: download dest, `touch`, `rm`). **Conclusion: in 4.37.1.166 (and 180, identical) the xcamera ONVIF server is downloaded and staged but never started — a half-wired / unshipped feature.** The `killall`/`rm` on OFF implies the vendor intended it to run, but the launch path is absent from these builds. This is why **our own Go ONVIF daemon on the cam is the right path** — you can't just "flip the vendor's ONVIF on".
- **Next candidate:** ssh into a cam (works, see above) -> dump `/system` and `nm -D`/`strings` for motor/ptz/crop/encoder attr.

## Minimal-diff image (v1 then v2 flashed on kitchen 2026-10-08; garage still stock)

`vendor-fw/` (gitignored) holds the vendor dumps + the build. Official 2.5.19 rootfs repacked (gzip, 128K blocks, size matches init's padded-size check)
with only: `usr/bin/onvif`, `etc/init.d/S76onvif`, `scripts/{onvif,webcmd}.sh`, new web bundle + `index.html` (v1 image also carried a `S21rootkeys` ssh-key patch,
dropped in v2 because the key now lives on the SD card). Output (v1): `vendor-fw/work/atomcam_tools.zip`; rootfs-only recipe = unsquashfs both, copy files, `mksquashfs -comp gzip -b 131072 -noappend`, as root in `debian:12-slim`.
(The #118 image is built the same way; recipe above.) Another way to update once ssh works: stream the squashfs to `/media/mmc/update/rootfs_hack.squashfs` and reboot (init moves it into place; keep a `.bak` of the old one on the SD first).
Diffed against official: exactly those files differ.

## Open work / next steps

1. ~~Build the minimal-diff image, update the cams, enable ONVIF, point Frigate at them~~ **DONE 2026-10-08** (both cams, Frigate `onvif:` -> `<cam>:8000`, max speed 3, `home` presets, shim removed). Still open: watch a real person being autotracked on each cam.
2. Auth: #118 supports `ONVIF_USER`/`ONVIF_PASSWD` (WS-Security); our cams run without (Frigate gets `user: ""`). Enabling = set both in hack.ini + same in Frigate (do not copy the user's creds onto a cam without asking).
   Port is now 80 (shared with lighttpd), not 8000.
2b. **FOV under-move:** the server hard-codes 63 x 37 deg. Patch `0005-ptz-fov-atomswing.patch` (on `onvif-on-118-presets`) uses **96 x 54**; image rebuild + garage reflash + retest still pending (expect FOV x=10 -> ~4.8 deg, was 3.15; stock reported 3.0 after rounding).
   Fixing it in `onvif_ptz.sh` is wrong: the script gets the same `jump_to_rel` call for generic and FOV space.
   **How 96 x 54 was measured (2026-10-11, garage, daylight):** pan/tilt in 10 deg ONVIF generic steps, always approaching from the same side (there is backlash/deadband: the first +5 deg tilt moved the image ~6 px),
   `get_jpeg.cgi` snapshots (1920x1080, no auth; RTSP grabs via ffmpeg were corrupt - HEVC joins mid-GOP), ORB matches -> median translation in the central region between consecutive frames.
   Pan 19.4-20.8 px/deg (one 15.6 outlier step), tilt 18-23 px/deg (noisy, few matches); ~20 px/deg both axes => 1920/20 = 96, 1080/20 = 54. **One unit, one position, ~10 samples per axis: treat as +-10%.**
   What Frigate actually needs is deg-per-frame-width near the centre (linear), not the rectilinear lens FOV (which would be ~80 x 50). Re-measure if a cam shows over/undershoot in real tracking.
   Scripts (scratchpad, not committed): `fov2.py`, `ptzcheck.py` - they run inside the Frigate container (`docker cp` + `docker exec frigate python3`; it has `cv2`, `onvif-zeep-async`).
   **Never query go2rtc's `/api/streams` on port 1984: it prints the Nest OAuth secret/refresh token from the Frigate config.**
2d. **Next real-cam test (needs a flash + the user's go):** image must carry the new `libcallback.so` (contmove/ptzstatus), `onvif_simple_server` (0005+0006), `onvif.sh`, `onvif_ptz.sh`. Then: GetStatus latency (expect ~83 -> ~40 ms),
   FOV x=10 -> ~4.7 deg, Speed 0.5 vs 1, preempt a long move with a short one, ContinuousMove + Stop (check `/tmp/onvif_ptz_cal`, direction by eye, small blip first), stop at 6 s without a Stop.
2c. Kitchen is still on the Go daemon; move it to the #118 image after garage has autotracked for a while (seed `1=home 163.500000 77.012657 0` in `/media/mmc/onvif_presets.txt`, Frigate port 8000 -> 80, empty creds).
3. ~~Open the PR~~ **DONE 2026-10-12: #146** (see top). Open: review feedback from mnakada, and a possible UI field for `ONVIF_MAX_SPEED` (hack.ini only today, documented in the README ONVIF section).
4. **Native relative move: DONE and verified on kitchen (2026-10-08).** `relmove <dpan> <dtilt> [speed] [pri]` in `libcallback/motor.c` (`local_sdk_motor_move_rel_angle`: degrees -> steps -> `move_rel_step`,
   clamped +-2130/+-1580 steps; hflip/vflip signs negated; done-callback re-reads the position like the vendor app) and the daemon's `RelativeMove` (FOV + generic space) is now one `relmove` command, no read-then-move.
   Image `vendor-fw/work/rootfs_hack.v2.squashfs` (new `libcallback.so` built with the firmware's own toolchain: the unmodified source gives exactly the official 129252 bytes) is running on kitchen.
   Verified on the cam: +-5 pan/tilt exact (tilt quantised to ~0.1 deg), out-and-back returns to the exact start (old read+abs path drifted 0.1-0.3 deg per round trip), same sign as abs `move`,
   SDK clamps at the mechanical limit (tilt 188 -> 180.0, no grinding), same/lower pri while busy -> `error : dismiss request.`, pri 0 preempts (victim gets `error : multiple request.`),
   full onvif-zeep suite passes. Caveats: cancelling a *relmove* with a pri-0 move ends 1-2 deg off target (abs-vs-abs cancel is exact; next abs move fixes it, not cumulative, only affects `Stop`);
   one unreproduced overlapping-test reading (184 vs 169); the speed argument seems to change little (10 deg at speed 1 took ~0.3 s). Test scripts must not wipe presets (kitchen's `home` was deleted once and restored).
   Leftover ideas: native `move_track`/`cruise`/`goback`, `local_sdk_motor_move` for true ContinuousMove (needs a hook too).
5. Docs outside this repo: `/media/Tac/kevin/dev/os-management` (`services/home-automation.md`, `services/docker.md`) describe the native ONVIF setup (updated 2026-10-08). The Frigate config lives outside any repo
   (`/home/kevin/frigate/config.yml`, contains secrets - never copy into a repo; always `docker exec frigate python3 -m frigate --validate-config` before restarting Frigate; backups `config.yml.pre-kitchen-onvif`, `config.yml.pre-garage-onvif`, `config.yml.pre-garage-118`).
   These docs still describe the Go daemon / `:8000` and need updating once kitchen has moved.
6. **Testing lesson (2026-10-08):** Frigate autotracking moves the cams by itself. With the old shim still steering garage, back-to-back test moves failed ~25% of the time; with the shim stopped, 24/24 passed.
   Stop Frigate (or disable `autotracking`) before motor tests, and never wipe presets in test scripts (`home` was deleted once and re-created).
   Timing lesson (2026-10-11): a 25 deg move at speed 5 finishes in <0.6 s - use >=80 deg and poll at 0.15-0.3 s to test Stop/MOVING.
7. Ideas not done: auth (WS-Security); native `move_track`/`cruise`/`goback`; true `ContinuousMove` through `local_sdk_motor_move` (needs a hook); the open question of the 1-2 deg miss after cancelling a relmove.

## Rules of the road

- Additive and off by default. Don't touch boot-critical scripts beyond the guarded init script; every new boot path must `exit 0` on failure.
- Keep the daemon standard-library-only Go, single `package main`, terse comments that explain *why*. Add/adjust a test in `main_test.go` for any behaviour change; run vet + `-race`.
  (Go daemon line only. On the #118 line the PTZ glue is POSIX/busybox `sh`: test with a stub `/scripts/cmd` and `busybox sh -n`, no bashisms.)
- Moving cams is physical and they are outdoors: small, slow moves, and return a cam to where you found it (read-only position: `/scripts/cmd move` on the cam,
  or `curl -s -X POST "http://<cam>/cgi-bin/cmd.cgi?port=socket" -d '{"exec":"move"}'`). Homes: garage 143.3/74.05, kitchen 163.5/77.0.
- Don't commit build outputs (`target/`, `atomcam_tools.zip`, `*.log` are gitignored) or secrets.

---

# Encoder tuning (branch `encoder-tuning`, 2026-10-11/12)

Branch base: `onvif-on-118-presets` (`7534732`) + 14 commits. Everything below is real-cam tested on garage (192.168.1.129) and
kitchen (192.168.1.50) unless marked otherwise. The ONVIF/PTZ sections above are still the reference for PTZ; this section
supersedes their "which image runs where" lines.

## State of the cams (end of session)

| | image on SD | sha256 (first 16) | notes |
|---|---|---|---|
| garage | **v12** `rootfs_hack.v12-nogopinterpose.squashfs` | `c31689d41f427cc6` | HEVC only (`RTSP_VIDEO2=on`), `IQP_MAIN_HEVC=42` (daylight), `GOP_MAIN_HEVC=30`, temper/sinter 112 |
| kitchen | **v12** | `c31689d41f427cc6` | HEVC only, `IQP_MAIN_HEVC=30`, `GOP_MAIN_HEVC=30`, temper/sinter 112 |

Image series (host copies in `vendor-fw/work/on118/`, gitignored): v6 `fdddcd35…` (PTZ only) -> v7 `4e596460…` (+encdump/encrc/encgop)
-> v8 `42b3087b…` (+iqp/gop commands, hack.ini keys, UI) -> v9 `f1861dbe…` (+set_gop hook, useless) -> v10 `01f03d60…`
(+SetChnGopAttr interposer, **buggy, adds 20**) -> v11 `f8bc72af…` (+re-apply after set_fps) -> **v12 `c31689d4…`** (interposer removed).
Rollback copies on both cams' SD: `rootfs_hack.squashfs.bak-118-v7-20261011b`/`v6-…b` (garage/kitchen, pre-v8), `bak-118-v8-…`,
`bak-118-v9-…`, `bak-118-v10-…`, `bak-118-v11-…`; plus the older `.bak-118-v2/v3/v5/v6`, `.bak-20261008/-20261011`.
Config backups on SD: `hack.ini.bak-iqpgop-20261011`, `video_isp.conf.bak-iqpgop-20261011` (both cams), `hack.ini.bak-qp42-20261011` (garage).

## New cam commands (libcallback/video_control.c)

Channel numbering: **libimp channels are 0 = main AVC, 1 = sub HEVC, 2 = main HEVC; the hack's SDK numbering (`video bitrate`, `iqp`, `gop`) is 0, 1, 3**.

| command | what |
|---|---|
| `video encdump <ch0-3> attr\|rc\|gop [first]` | read-only dump of the encoder channel / rate-control / GOP structs, 16 words per call (hex) |
| `video encrc <ch> [<word> <value> ...]` | read-modify-write of the 9-word rate-control struct on the running encoder (libimp numbering) |
| `video encgop <ch> [len]` | read / set GOP length of a running channel (libimp numbering) |
| `video iqp <ch 0\|1\|3> [qp 10-51\|auto]` | I-frame QP = high 16 bits of rc word 3; re-applied after the app's `set_kbps`, `set_fps` and `CreateChn` |
| `video gop <ch 0\|1\|3> [frames 1-300\|auto]` | keyframe interval; same re-apply points |
| `video temper|sinter <0-255>` | ISP temporal / spatial noise reduction (pre-existing). No read-back (`error`) |

hack.ini keys (negative = off, value remembered, like `BITRATE_*`): `IQP_MAIN_AVC IQP_SUB_HEVC IQP_MAIN_HEVC GOP_MAIN_AVC GOP_SUB_HEVC GOP_MAIN_HEVC`.
Applied at boot by `set_icamera_config.sh`; UI under "video config" (`Setting.vue`, i18n en/ja); UI defaults: AVC QP 32 / GOP 15, HEVC QP 30 / GOP 30,
sub off, ISP sinter/temper default 112. **A fixed QP is scene dependent - see "Keyframe size" below.**

## Encoder facts (libimp, T31)

* All three channels run rate-control **mode 8** (capped-VBR-like). Struct words: 0 mode, 1 target kbps, 2 max kbps (`video bitrate N` sets target = 0.75 x max),
  **3 = [MIN QP << 16 | initQP(-1)]** (AVC stock 23, HEVC stock 32; `video iqp` sets this, it is a floor, NOT a keyframe QP), 4 = [I/P delta(-1) << 16 | maxQP 51], 5 low = P/B delta(-1), 6 options, **7 = max picture size in kbit (1920 = 240 kB)**, 8 low = max PSNR in dB (48). Decoded from libimp.so 2026-10-12, see the "rc struct" section of `tools/encbench/README.md`.
* Stock AVC is capped at 960 kbps because `BITRATE_MAIN_AVC=-1800` is negative and `set_icamera_config.sh` skips non-positive values.
* GOP: the app derives its own interval (2.5 x fps) inside `local_sdk_video_set_fps`; the user value is re-applied after that call.
  **Read-back quirk: at the encoder's 20 fps a requested GOP reads back as requested + 20 (30 -> 50, 45 -> 65); at 15 fps it is exact.** Cause unknown.
* QP words move bitrate hard: HEVC I-QP 28/30/32/34/36 = ~1850/1100/1340/890/660 kbps (night); the target/max words only matter in busy scenes.
* ISP noise reduction is the biggest quality/bitrate lever: temper+sinter 200 halves the bitrate and rewards SSIM (check by eye); below ~64 the
  noise is coded every frame and P frames balloon (13-30 kB) - at 112/112 P frames are ~5 kB at night.

## Keyframe size is the limit (the important finding)

The cam's RTSP path stalls/loses references when a keyframe is bigger than **~70-80 kB**, whatever the codec:

* AVC at night: I-QP 28 (130 kB) made the clock stick and jump 3 s; 30 (103 kB) and 32 (80 kB) were smooth.
* HEVC garage, daylight: I-QP 30 = 227 kB (stalls ~2-3 s, VLC "Could not find ref with POC"), 36 = 123 kB, 40 = 97 kB (gaps), **42 = 72 kB (clean)**, 44 = 51 kB.
* The same QP is 80 kB at night and 227 kB by day on garage (outdoor, busy); kitchen (indoor, calm) is fine at QP 30.
* AVC + HEVC at the same time stutters (about 3 MB free RAM with two encoders + two RTSP sessions); with HEVC alone ~12-18 MB are free.
* Likely mechanism (source read, `v4l2rtspserver ce80891`): `splitFrames` queues every NAL as its own entry in a 5-entry queue (`-Q 5`);
  when full, the **oldest entry is deleted**, i.e. the start of the keyframe that is still being sent. See "Open questions".

## Tools (`tools/encbench/`, README has the tables)

* `bench.py <ip> <rtsp-path> [label]` - bitrate / SSIM vs averaged snapshots / detail / temporal noise (needs a static scene; night scenes are noisy).
* `gopdrift.py <ip> <path> [seconds]` - per-frame size / SSIM / sharpness grouped by frames since the last I frame (shows pumping and keyframe size).
* `arrival.py <rtsp-url> [seconds]` - frame arrival gaps (`RTSP_TRANSPORT=udp|tcp`). Use a venv with `numpy opencv-python-headless`; set `FFMPEG=/usr/bin/ffmpeg`.
* Run one test client at a time; Frigate counts as a client. A second client joining makes the first stall for a moment.

## Build / flash recipes that worked

* **libcallback only (about 1 minute)**, no full `build_all`: in `atomtools/atomtools:Ver.2.5.5` use `/atomtools/build/cross/mips-uclibc/bin/mipsel-ingenic-linux-uclibc-gcc`
  (the host `mipsel-linux-gcc` is glibc and wrong), sources = `CC_SRCS` of `libcallback/Makefile`, `-DV4L2_PIX_FMT_HEVC=0x43564548`,
  `-I<tinyalsa include>` (clone `tinyalsa/tinyalsa` at the commit in `libtinyalsa.mk`), `-L<rootfs>/lib/modules -ldl -l:libtinyalsa.so.2 -lm`.
  Output matches the official build within ~100 bytes and `NEEDED` = `libtinyalsa.so.2`, `libc.so.0`.
* Full build: `docker compose up -d; docker compose exec -T builder /src/buildscripts/build_all > buildN.log` - it rebuilds everything (about an hour).
* Image: base tree `scratchpad/img/final*` (official 2.5.19 rootfs + our files), replace `lib/modules/libcallback.so`, `scripts/set_icamera_config.sh`, the web bundle
  (`var/www/bundle_*.js.gz` + `index.html`), `lighttpd -t` under `qemu-mipsel-static`, `mksquashfs -comp gzip -b 131072 -noappend`, check `bytes_used` padded == size.
* Flash: stream to `/media/mmc/update/rootfs_hack.squashfs.part`, sha256 on the cam, `mv` to `rootfs_hack.squashfs`, **user reboots by hand**.
  The cam is back in ~1 minute; `uptime` says `up 0 min` while `iCamera_app` is still starting.

## Lessons / mistakes (read before touching the cams)

* **Never flip `video fps` on a live cam**: each change restarts the encoder channels and left garage's RTSP server in a state that stalled
  until a reboot. Frame rate also flips by itself with daylight (15 fps at night, 20 by day).
* `rtspserver.sh restart` started from an ssh session dies with the session; bind often fails right after a kill ("Address family not supported").
  `killall v4l2rtspserver` and let the cron watchdog (`rtspserver.sh watchdog`, every minute) relaunch it, or `setsid nohup` it and retry.
* Busybox on the cam: `cp` has no `-n`, `timeout` is `timeout -t N cmd`. `cat /proc/<pid>/fd/2` blocks forever (stderr is the console).
* `dlsym(libimp, "IMP_Encoder_SetChnGopAttr")` used as a "real" function behind a preload interposer returned lengths +20; calling the symbol directly is correct.
* A first claim "ffmpeg decoded 120 s over TCP with zero warnings" was wrong (the check had been killed by its timeout and printed nothing).
* After kitchen was flashed with v8 its HEVC capture (`video 2`) went `off` ~2 minutes after Frigate connected: the RTSP stream then had an audio track only and
  Frigate's ffmpeg crashed in a loop. Cause **not found**; re-running the same `iqp`/`gop` commands did not reproduce it. `video 2 on` + `killall v4l2rtspserver` fixed it.
* VLC defaults to RTP over UDP: a lost packet in a keyframe shows as "missing ref" until the next keyframe; use `--rtsp-tcp`.

## ROOT CAUSE FOUND (2026-10-12): live555's 50 KB client send buffer

The daylight stalls (and the AVC "sticking") are **not** the encoder, the loopback, the queue, the CPU or the network:

* `video stats` (v14) on garage at QP 30 by day: ch2 delivers every frame on time (1350 frames / 70 s, largest 222 kB, write+callback <= 6 ms,
  0 stalls, longest inter-frame gap 56 ms). So the encoder and our callback are healthy.
* The loss is in the RTSP server: live555 does `setsockopt(SO_SNDBUF, 51200)` on every RTSP client socket (shim log: `sndbuf fd3: 51200 -> ...`).
  A 200+ kB keyframe does not fit in a ~100 kB kernel buffer, the non-blocking send fails and RTP packets are dropped; the decoder then waits for the next IDR
  (the 2-3 s gaps = GOP). UDP behaved the same (bursts overflow the same way), which is why transport changes, `-Q`, `max_buffers` etc. did nothing.
* Fix: `LD_PRELOAD` shim `tools/encbench/shim/sndbuf.c` (raises any `SO_SNDBUF` request < 512 KB to 512 KB) + `net.core.wmem_max=1 MB`.
  **Must be built with the glibc toolchain** (`/atomtools/build/buildroot-2016.02/output/host/usr/bin/mipsel-linux-gcc`): the rootfs is glibc 2.21
  (`/lib/libc-2.21.so`); only the vendor stack under `/atom` is uClibc. Installed as `/usr/lib/sndbuf.so`, loaded by `rtspserver.sh`.
* Result, garage, daylight, same stream: I-QP 30 went from 6-12 fps with 3 s stalls and 8-9 decode warnings per 40 s to **18.5-20.9 fps, max gap 0.3-0.8 s, 1 warning**;
  I-QP 26: 18.5 fps, gaps <= 0.32 s, SSIM vs reference 0.89 (QP 42 was 0.65); QP 22 hits the 2 Mbps cap (1977 kbps) with ~1 s gaps.
  The encoder caps keyframes at ~227 kB whatever the QP, so **28-30 is the useful range**; garage's `IQP_MAIN_HEVC` goes back from 42 to 30.
* Other v14 results: libimp stream buffer defaults are 949 kB (AVC), 129 kB (sub), `streamcnt` 2 on all channels; they are pre-CreateChn settings
  (`video streambuf|streamcnt`, hack.ini `STREAMBUF_*`/`STREAMCNT_*`), not needed for this fix. `video stats [reset]` prints per channel counters.
* A proper upstream fix would patch live555 (`increaseSendBufferTo(..., 50*1024)` in `RTSPServer`/`GenericMediaServer`, package `custompackages/package/live555`).

Image series continued: v13 `cf41d83c…` (loopback 640x360 / max_buffers=8) -> v14 `446436da…` (+streambuf/streamcnt/stats) -> **v15 `fa1525d7…`** (+sndbuf.so, rtspserver.sh).

**v15/v16 status (2026-10-12):** v15 (`fa1525d7…`, shim) runs on garage and is verified with the *supervised* server: daylight I-QP 30 = 18.8-19.6 fps,
max gap 0.33-0.56 s, 1 decode warning / 40 s, SSIM 0.88. Kitchen is still on v13 (not given v15 yet). **v16 (`c6dddc4f…`) = v15 + watchdog self-heal**, staged on
garage's SD, not booted: `rtspserver.sh watchdog` restarts the server when an enabled channel's capture (`/scripts/cmd video N`) is not `on`.
Why: after the v15 reboot garage came up with `video 0/1/2` all `off` while the server ran (audio-only RTSP stream, Frigate camera_fps 0) - the same thing kitchen did after v8.
Only `video N on|off` commands change that flag (`rtspserver.sh off|restart`, the UI's RTSP save); none was found in the logs, so the trigger at boot is **still unknown**.
Frigate side note: both cams show `skipped_fps` ~3-4 and `process_fps` ~1.3 in daylight with the Coral saturated (`detection_fps` 9-18, 36 ms/inference); that is detector load from
motion regions, not the stream (cam-side stats are clean) - check at night / compare with the old QP 42 before blaming the sharper picture.

**v16 verified on both cams (2026-10-12 08:20 JST):** garage and kitchen boot with the shim loaded in the supervised server (`/proc/<pid>/maps`), `wmem_max` 1 MB,
only channel 2 capturing, `RTSP_AUDIO2=off` (video-only stream), `IQP_MAIN_HEVC=30`, `GOP_MAIN_HEVC=30`. Watchdog self-heal proven on garage: `video 2 off` was
detected at the next cron minute (`capture was 'off', restarting v4l2rtspserver` in `atomhack.log`), stream back with video in ~20 s.
Daylight idle numbers: garage 17.5-19.5 fps, gaps <= 0.31 s; kitchen 19.2-19.9 fps, gaps <= 0.21 s, `video stats` 0 stalls (keyframes 172 kB). Frigate config switched to
VAAPI decode, no audio role, OpenVINO iGPU detector (see os-management `services/home-automation.md`); Frigate restart is the user's.

## Open questions

1. What exactly breaks the RTSP path for big keyframes. Ruled out: server-side truncation, `-Q 20 -c` alone. Open: live555 sink time for a 200 kB NAL,
   wifi burst loss + TCP head-of-line, the 50 KB `SO_SNDBUF`. Cap ideas: `IMP_Encoder_SetChnQpIPDelta`, `SetChnQpBounds`, or a software loop in
   `video_callback.c` that measures each IDR and nudges the I-QP. See `tools/encbench/README.md` ("Why does a ~100 kB keyframe break...").
2. Why the +20 GOP offset at 20 fps, and why kitchen's capture went `off` once.
3. Whether `IQP_MAIN_HEVC` should be per time of day (night 30 / day 42) or replaced by a keyframe-size cap.

---

# PR #146 and PTZ traffic analysis (2026-10-12)

README now documents ONVIF/PTZ (Japanese, section `#### ONVIF` under 配信設定, plus a feature bullet); `libcallback_hook.md` already covered relmove/contmove/ptzstatus.
Only Frigate is claimed as verified in the docs; do not add other NVRs without testing.

**Passive capture** (`sudo tcpdump -i enp2s0 ... '(host 192.168.1.129 or host 192.168.1.50) and tcp port 80'`; needs the user to run it, sudo has a password; do not use a docker container to get around that), 31 min, 2 cams:
- Only Frigate (192.168.1.162) talks PTZ on :80. Calls: `RelativeMove` (always FOV space, `Speed` 1/1), exactly one `GetStatus` after each move (always IDLE, the move call blocks), `GotoPreset` home after each burst (~10 moves per burst).
- Move sizes: kitchen pan median 21 / p90 37 / max 48 deg, tilt 7 / 13 / 16; garage pan 28 / 38, tilt 5 / 7. Kitchen re-fires fast (15 of 41 gaps < 1 s, median 2.1 s).
- Latency: `GetStatus` median 60-80 ms, `RelativeMove` 300-360 ms, `GotoPreset` 0.5-0.6 s (max 1.0 s).
- Accuracy vs the 96x54 FOV assumption: reported position matches commanded delta, mean abs error 0.3-0.4 deg, actual/commanded 0.99-1.00 (status is integer-rounded). Return-to-home accuracy cannot be read from the capture.
- Parser: scratchpad `parse2.py` (not committed). Needs tcpdump `-A` stream reassembly; strip the first 52 chars (IP+TCP header with timestamps) of each packet, and pure ACK lines have no `seq`.
- There is **no on-cam move log** (`onvif_simple_server.log` stays empty, lighttpd has no access log) and Frigate logs tracking moves only at debug. Idea if needed: log args/before/after/duration in `onvif_ptz.sh` (needs flash + reboot).

**Autotrack sanity from Frigate's DB** (`/home/kevin/frigate/frigate.db`, copy it before reading): 229 person events / 24 h; tracked people drift to frame centre (all 7 events that started > 0.2 frame off-centre ended within 0.07). `movement_weights` vary up to ~1.7x between calibrations on the same hardware (third value); `calibrate_on_startup: true` re-rolls them each Frigate start.
Detect pipeline since the VAAPI switch: 0 kernel GPU hangs, but ffmpeg detect crash-loops (~40/h garage, ~64/h kitchen: `Could not find ref with POC` -> `Failed to sync surface`), see the encoder section; it can interrupt tracking.

---

# v17: built-in send buffer + keyframe gate (2026-10-12, built, NOT flashed)

- `custompackages/package/live555/20_client_sndbuf.patch`: RTSP client sockets ask for 512 KB (was 50 KB), replaces the LD_PRELOAD `sndbuf.so` (still shipped/preloaded if present, harmless).
- `custompackages/package/v4l2rtspserver/0006-wait-for-irap-per-client.patch`: `WaitForIRAPFilter` per unicast client; parameter sets pass, coded slices are dropped until the first IRAP
  (logs `RTSP client starts on a keyframe after dropping N frame(s)` in `/tmp/log/rtspserver.log`). Costs up to one GOP of start-up delay. `isKeyFrame()` in the stock source is defined but never called.
- Why: Frigate's ffmpeg crash loop. Intervals between crashes have a floor (35-45 s; kitchen 67% of 482 crashes in the 30-50 s bins, garage 38% of 276): restart -> join mid-GOP -> VAAPI
  `Failed to sync surface` -> exit -> restart. Cam encoder (14 gaps / 9.5 h), TCP retransmits (0.003-0.03%) and wifi are clean; a 5-min software-decode client saw only the join error.
  Garage's long random tail is probably the large-daylight-keyframe mechanism, not covered by the gate. Cheap A/B: `hwaccel_args` off on one cam in Frigate (user restarts Frigate).
- Image: `vendor-fw/work/on118/rootfs_hack.v17-irapgate.squashfs`, sha256 `2ee7d684703f8377...`, = v16 + `usr/bin/v4l2rtspserver`, `usr/lib/libliveMedia.so.112.1.2`, new web bundle `bundle_272e04f7...` + `index.html`
  (grouped video config UI). libcallback.so deliberately kept from v16 (source unchanged since f53639d). `bytes_used` padded == size, binary loads under qemu 7.2.
- Not verified on a cam: the gate itself (needs v17 flashed), start-up delay, effect on crash rate. Verify with: `grep "starts on a keyframe" /tmp/log/rtspserver.log` on the cam, and Frigate crashes/hour
  (`docker logs frigate | grep "Ffmpeg process crashed unexpectedly"`; baseline 8 h: kitchen 482, garage 276).
- Build gotcha: `docker compose exec ... build_all > buildN.log` printed nothing for ~10 min (cleanup phase); wait for the `exit=` marker, the log is not live. The safety check blocks `rm -rf` inside `docker run sh -c`; use fresh directories instead.

## v17 on kitchen: first results (2026-10-12 ~18:30-19:45 JST, evening, garage still v16 as control)

- Flashed kitchen only (backup `rootfs_hack.squashfs.bak-118-v16-20261012`); garage stays on v16 on purpose, as the A/B control for a daylight comparison.
- **Gate works:** 5 of 5 software-decode joins had zero `Could not find ref` / `RPS` (before: one per join, always); ~1 s start-up cost. The `starts on a keyframe` log line never shows
  (stdout of v4l2rtspserver is fully buffered when redirected to a file); use the join test (`ffmpeg -rtsp_transport tcp -i rtsp://<cam>:8554/video2_unicast -t 4 -f null -`) instead.
- **Signature changed, rate not proven lower:** all kitchen v17 crashes (7 in ~75 min incl. garage's) have no ref errors; they show `[segment] Timestamps are unset in a packet` / `Non-monotonic DTS 0 -> 0`
  then VAAPI `Failed to sync surface`. Garage (v16) crashed once in the same window with the old `POC`/`RPS` chain. Evening/night rates are low for both, so a daylight A/B is still needed
  (v16 garage ~40/h by day vs v17 kitchen).
- **ntpd theory refuted:** a static `CLOCK_REALTIME - CLOCK_MONOTONIC` probe (500 ms, 15 min, both cams; source in the session scratchpad, build with the builder's `mipsel-ingenic-linux-gnu-gcc -static`)
  showed no steps, no slews, 0.00 ms drift, < 0.15 ms jitter, while 3 crashes happened in the window. Kitchen's crash intervals (681, 687, 719, 868, 519 s) are not really periodic either.
- Next candidates: a long-running `-c copy -f segment` client (Frigate's record output) on kitchen to see whether `Timestamps are unset` reproduces without VAAPI; software decode on one cam in Frigate.


## Software-decode A/B on kitchen (2026-10-12, 11:17-13:15 UTC, ~118 min, evening JST)

- Frigate config (outside the repo): `hwaccel_args: []` under `cameras.kitchen.ffmpeg` (garage keeps global `preset-vaapi`). Both cams on the same Frigate; kitchen v17, garage v16 (control).
- **Result: kitchen 0 crashes (baseline ~4.8/h, ~9 expected), garage 4 (~2/h).** Garage's crashes still show the `Could not find ref with POC` -> `Error constructing the frame RPS`
  -> `Failed to sync surface` -> `hwdownload -5` chain: VAAPI dies hard on a missing reference where software decode conceals it.
- Conclusion: the ffmpeg crash loop lives in Frigate's VAAPI decode path, not the cam/network. Caveat: small sample, quiet evening regime; daylight not tested.
- Kitchen was left on software decode (about a third of a core). Revert = delete the `hwaccel_args: []` line (backup `config.yml.pre-kitchen-swdecode`); Frigate restart is the user's.
- Decided with the user: **garage stays on v16 as the control**; next is exploring VAAPI options (`-extra_hw_frames`, error-tolerant decode args, driver) on one cam.

## VAAPI crash mechanism, offline repro (2026-10-12, Frigate 0.18 / ffmpeg 8.0, N100 iGPU, iHD 25.2.6)

Method (no cam/Frigate impact): `-c copy` 60 s clip of kitchen's stream into the Frigate container, damage it with `-bsf:v noise=...`,
replay through Frigate's exact detect pipeline (`-hwaccel vaapi ... -vf fps=5,scale_vaapi,hwdownload,format=nv12`) with candidate flags. Scripts were scratch-only.
- **Missing references are NOT fatal under VAAPI**: 129 `Could not find ref with POC`/RPS errors on a clip with 9 dropped packets, rc=0, all 300 frames. Mid-GOP start and 35-frame gap likewise.
- **Corrupt slice data IS fatal, deterministically**: a few packets with ~40 random bytes changed -> `Failed to sync surface: 23 (internal decoding error)` (`VA_STATUS_ERROR_DECODING_ERROR`),
  ffmpeg turns it into -EIO (it only tolerates INVALIDDATA), `hwdownload`/decoder thread exits rc=251. Reproduces with ZERO ref errors, which is the sync-only crash seen on kitchen under v17.
  Software decode of the same clips: rc=0, 300/300 frames.
- No flag helps: `-extra_hw_frames 8`, `-err_detect ignore_err`, `-flags +output_corrupt`, decoding on GPU but downloading in the decoder (`scale` in software) all still die. `i965` is useless (Gen12).
- **QSV (`-hwaccel qsv`, Frigate's `preset-intel-qsv-h265`) survives all of them** (rc=0; drops to the next IDR after an error: 111/300 frames on the nastiest clip). Live on kitchen's RTSP for 159 s, 795 frames, no errors.
  Caveat: the GPU hang storm (~1800 on 2026-10-10/11) happened in the QSV era (before the VAAPI switch); it coincided with the cam's ref-loss storm, so unclear which caused it.
- Frigate runs ONE ffmpeg per cam doing detect + record segments, so a decode death also interrupts recording.
- Real fix would be upstream ffmpeg (map DECODING_ERROR to INVALIDDATA / drop the frame); a config-only fix is software decode or QSV.

## QSV A/B on kitchen (started 2026-10-12 13:33:47 UTC) and next test: where does the ~227 kB keyframe ceiling come from

- Kitchen's Frigate `hwaccel_args` is `preset-intel-qsv-h265` (backup `config.yml.pre-kitchen-qsv`; revert to software = `[]`, to VAAPI = delete the line). Garage stays VAAPI on v16 as the control.
  Compare crashes/h (baselines: VAAPI kitchen ~4.8/h, VAAPI garage ~2/h, software kitchen 0 in 118 min), the GPU hang count (the ~1800 hangs of 10/10-10/11 were in the QSV era, cause unproven), CPU, skipped_fps.
- Kitchen encoder state at 22:36 JST (night, read-only check): ch2 HEVC I-QP 30, GOP 30, 2000/1500 kbps, 15 fps, sharp 128 / sinter 112 / temper 112; measured 1605 kbps, I 81 kB, P 10.7 kB, 0 stalls.
- **Next test (not run): is the "keyframes capped at ~227 kB" a rate-control buffer limit?** The cap in the notes is an observation (QP 30 by day = 227 kB, QP 22 hit the 2000 kbps max), never root-caused;
  per-keyframe sizes at QP 26/22 were never logged. Hypothesis: capped-VBR (mode 8) buffer sized ~1 s of max bitrate (227 kB = 1.8 Mbit ~ 0.9 s of 2 Mbps).
  Test by day, ONE cam, after the QSV A/B: raise `BITRATE_MAIN_HEVC` (2000 -> 3000) and set a low I-QP (22); log max keyframe size (e.g. `video stats` / ffprobe packet flags over 60 s).
  Ceiling moves with max bitrate -> rate-control buffer; stays ~227 kB -> fixed encoder limit (then look at rc words 5-8: 65535, 1, 1920, 48 on kitchen, never decoded).
  Remember the RTSP path was only proven clean up to ~227 kB keyframes, and a bigger bitrate raises the Wi-Fi load; revert the knobs afterwards.

## Definition experiment on kitchen (2026-10-12 21:50-22:15 UTC, ~06:50-07:15 JST, daylight, 20 fps, live commands only)

**Correction:** my first write-up blamed "dawn light / wind / PTZ moves" for noisy numbers. The user confirmed light, framing and wind were steady, so the
stream-to-stream spread in P-frame size / Laplacian variance (e.g. 462 vs 657 at identical settings) is real encoder variance, not drift. Keyframe size was the stablest number.

Measured with 30-40 s `-c copy` clips pulled through Frigate's ffmpeg/ffprobe (packet sizes + Laplacian variance of decoded keyframes / every 7th P frame). Static scene (foliage).

| change | keyframe | P frame | bitrate | note |
|---|---|---|---|---|
| baseline QP 30 / 2000 kbps / sinter 112 / temper 112 | 140 kB | 2.2 kB | 925 kbps | **far below the 1500 target: rate control is not the limit** |
| `video bitrate 3 3000` | 141 kB | 2.2 kB | 935 kbps | no effect (words 1/2 became 2250/3000) |
| `video temper 80` (A/B/A/B, 30 s each) | 144-146 -> 128-129 kB | tiny | 640-850 kbps | keyframes -11%, reproduced twice; direction of the knob unclear (lower = smaller, not noisier); no visible difference by eye |
| I-QP 38 (temper 80, 3000) | 55 kB | 0.27 kB | 278 kbps | blocky, laplacian 433 |
| I-QP 30 | 122-123 kB | 1.6-2.5 kB | 780-915 kbps | laplacian ~540 (keyframes and P alike: no pumping) |
| I-QP 26 (three runs) | 123-131 kB | 3.3-9.0 kB | 1066 / 1650 / 1930 kbps | laplacian 549-633 on keyframes, P 585-645 (big spread between runs) |
| I-QP 20 | 128 kB | 16.3 kB | **2997 kbps = pinned at the 3000 cap** | P laplacian fell to 270 (starved by the cap) |

Findings:
- **The "I-QP" word (rc word 3 high half) does not size the keyframes in this scene**: QP 20, 26 and 30 all give a ~123-130 kB keyframe, only QP 38 shrinks it (55 kB).
  It spends bits on the P frames (x4 to x8). So "lower I-QP for a sharper picture" mostly buys bitrate, and the keyframe, which carries the detail of a static scene, is clamped by something else.
  That clamp is the same "~227 kB ceiling" seen on garage by day (there it was QP-dependent between 36 and 30, so it is scene/limit dependent, not a fixed number).
- Bitrate max/target (words 1/2) only matter when the scene needs more than the target; at 20 fps daylight it does not.
- Keyframe size at fixed QP is the metric to compare ISP changes; Laplacian variance and bitrate are too noisy for 30 s clips.
- Open: what clamps the keyframe (rc words 5-8: 65535, 1, 1920, 48 on kitchen; never decoded), see "rc struct" below if it has been filled in.

**Incident:** during this series the user's local VLC on the kitchen stream "stuttered big time" in one of the steps. Most likely the I-QP 20 run (a sustained ~3 Mbps pinned at the cap, 16 kB P frames),
possibly the extra RTSP client of each 30 s capture (a second client joining makes the first stall for a moment, see Tools). Not pinned down.
**Lessons:** every pulled clip is an extra RTSP client, so measure on the cam side (`video stats`, `encdump`) where possible; never push a live cam near its max bitrate while someone watches;
all values were set with live commands (not in hack.ini / video_isp.conf) and kitchen was reverted at 22:14 UTC to QP 30 / 2000 kbps / temper 112.


## rc struct decoded and wired (2026-10-12, v18 in source, NOT built into an image / flashed)

- Disassembled `IMP_Encoder_SetChnAttrRcMode` (+ its helper at 0x71210, the jump table and the dedicated QP setters) in the cam's own `libimp.so`
  with the builder image's `mipsel-ingenic-linux-uclibc-objdump` (no Ghidra needed). Full word table: `tools/encbench/README.md` ("rc struct").
- **Corrections to earlier notes:** the "I-frame QP" (`video iqp`, `IQP_*`) is the **min QP** (a floor for all frames); the "~227 kB keyframe ceiling" is word 7, max picture size 1920 kbit = 240 kB;
  word 4 high is the I/P delta, not part of the QP bounds.
- Runtime changes of words 7, 8 and the I/P delta have no effect on a running channel (measured, kitchen); they must be set at `CreateChn`.
- Code: `video rc` command + hack.ini keys `INITQP MAXQP IPDELTA PBDELTA RCOPT MAXPIC MAXPSNR` (suffix `_MAIN_AVC|_SUB_HEVC|_MAIN_HEVC`) applied from the `CreateChn` hook at attr byte offset 44.
  Absent keys change nothing, so a v18 image behaves like v17 until a key is set. Built with the firmware toolchain: `libcallback.so` 140180 bytes, NEEDED libtinyalsa.so.2 + libc.so.0, sha256 `83b09417...`
  (build gotcha: the Makefile's `CC_SRCS` line ends in a `# ...` comment; copying it into a shell script comments out every link flag and silently drops libtinyalsa from NEEDED).
- To test (needs a flash + reboot, user's go): set e.g. `MAXPIC_MAIN_HEVC=600` on one cam, reboot, read `video rc 3` (value, no `*` means the app's) and the keyframe size with `video stats`.
- **v18 image staged (not flashed):** `vendor-fw/work/on118/rootfs_hack.v18-rcwords.squashfs`, sha256 `9b674ec2543b4214...`, 46481408 bytes (`bytes_used` padded == size) = v17 + the new `libcallback.so`
  (same 69 exported symbols as v17's, text +5 kB, stripped). Nothing else differs from v17. Flash recipe as in "Build / flash recipes"; kitchen's rollback is `rootfs_hack.squashfs.bak-118-v16-20261012` or the v17 file.

## v18 on kitchen: rc words measured (2026-10-12 22:40-22:47 UTC)

- Kitchen flashed with v18 (backup `rootfs_hack.squashfs.bak-118-v17-20261012`, user rebooted twice). `video rc <ch>` works; hack.ini keys are applied from the CreateChn hook. hack.ini backup: `hack.ini.bak-rcwords-20261012`.
- **Measured** (table in `tools/encbench/README.md`): `IPDELTA` is the keyframe knob (AVC 3 -> 10 = 115 -> 193 kB); `MAXPIC` is honoured but floored by libimp (600 -> 1352 kbit at 2000 kbps max);
  `MAXPSNR` 30 did nothing measurable; `video iqp` = min QP only. The per-channel vendor values differ (see the README).
- Queued for the next boot: `IPDELTA_MAIN_HEVC=6` in kitchen's hack.ini (test keys removed). Expect keyframes ~+40% (capped near 169 kB by the max picture floor), P frames unchanged. Watch VLC/Frigate for stutter and `video stats` for stalls.
- **IPDELTA_MAIN_HEVC=6 applied on kitchen (boot 07:50 JST, `video rc 3` shows `ipdelta* 6`)**: ch2 keyframe max 170-172 kB, i.e. the SAME ~171 kB seen with ipdelta on auto right after the first v18 boot.
  So on the main HEVC channel keyframes are capped near 169 kB (1352 kbit, the number libimp produced as the max-picture floor) whatever the I/P delta; the AVC channel (ceiling not reached) did respond to the delta.
  Cross-run comparisons are weak (daylight changed between boots). Hypothesis, untested: that ceiling is derived from the max bitrate (about 0.68 s of 2000 kbps).
  Planned probe (needs a hack.ini write + reboot, awaiting the user's go): `BITRATE_MAIN_HEVC=3000`, `MAXPIC_MAIN_AVC=50`, `MAXPIC_SUB_HEVC=50`; the read-back `maxpic` of the unstreamed channels (bitrates 960 and 180)
  gives the floor formula, and ch2's keyframe max should move with the bitrate if the hypothesis holds.

### Floor probe and keyframe-ceiling tests (kitchen, 2026-10-12 22:56-23:01 UTC, boot with BITRATE_MAIN_HEVC=3000, IPDELTA_MAIN_HEVC=6, MAXPIC_*=50)

- **The max-picture floor depends on resolution, not bitrate:** requesting 50 kbit read back as **1352 on ch0 (1080p AVC, 960 kbps)**, **1352 on ch2 (1080p HEVC, 2000 kbps)** and **624 on ch1 (640x360, 180 kbps)**.
  So the bitrate-derived-buffer hypothesis is dead. The floor is the encoder's own minimum for the frame size (169 kB / 78 kB).
- **HEVC main keyframe max is flat at ~170-178 kB by day on kitchen** for: ipdelta auto / 6, max bitrate 2000 / 3000, and (runtime, 25 s windows) min QP 30 / 28 / 26 / 30 (all exactly 176 kB).
  The AVC channel did move with the delta (115 -> 193 kB, min QP 23), so the delta works at creation, but whatever pins the HEVC keyframe is not one of the words we can set (min QP, delta, max picture, max bitrate).
  Not understood. Candidates left: the HEVC encoder's own I-frame budget (chn+356 clamp in `SetChnAttrRcMode`), a profile/level limit, or the scene simply being 176 kB at the rate control's chosen QP.
- Nothing here made the picture measurably sharper; kitchen was restored: live bitrate 2000, min QP 30, hack.ini == `hack.ini.bak-rcwords-20261012` (original: no IPDELTA/MAXPIC keys, bitrate 2000).
  The running encoder keeps ipdelta 6 and the ch0/ch1 floors until the next reboot (harmless: ch0/ch1 are not streamed). v18 stays on kitchen; garage is untouched (v16).
- Practical result for "more definition": the encoder's quality knobs on this cam are exhausted at QP 30 / GOP 30 / 2000 kbps for HEVC; the remaining levers are ISP (sinter/temper/sharp, measured ~no effect on keyframes) and `video bitrate`/QP for P frames only.

## UI iqp/gop never reached the encoder; v19 staged (2026-10-12)

- User saved IQP 25 / GOP 40 from kitchen's UI: both landed in `hack.ini` (and `/tmp/hack.ini`) but the live encoder stayed at 30 / 30. Cause: the UI queues `iqp <ch> <qp>` / `gop <ch> <n>` but
  `overlay_rootfs/scripts/webcmd.sh` had no handler for them (only `bitrate`, `framerate`, ...), so they were dropped and only the boot script applied them. Fixed in `c84b9d5`.
  Applied by hand on kitchen at 23:10 UTC (`video iqp 3 25`, `video gop 3 40`, read back `user 25` / `user 40`, rc word 3 = 0x19FFFF, no stalls). Garage is unchanged (30 / 30).
- **v19 image staged, not flashed:** `vendor-fw/work/on118/rootfs_hack.v19-webcmd.squashfs`, sha256 `14e7c6e5d3d2e9d2...`, 46481408 bytes (`bytes_used` 46478128 padded == size)
  = v18 + `scripts/webcmd.sh` (diff vs v18: exactly the 5 added lines). Until it is flashed, UI saves of iqp/gop only take effect at the next boot.
- Gotcha for builds: UI-initiated commands live in `Setting.vue` (`execCmds`) and need a matching `webcmd.sh` branch; check both when adding a UI knob.

## QSV A/B verdict (2026-10-12 13:33:47 UTC -> 2026-10-13 06:34 UTC, 16 h 50 min, includes the whole daylight stretch from ~20:40 UTC)

| | decode | image | ffmpeg crashes | per hour | by day (21h-06h UTC) |
|---|---|---|---|---|---|
| kitchen | **QSV** (`preset-intel-qsv-h265`) | v17 -> v18 | **0** outside my 3 reboot windows (117 inside them: Frigate retrying a downed RTSP server, 22:40-23:00 UTC) | **0** | 0 |
| garage | VAAPI (global preset) | v16 | **226** | 13.4 | 15-31 / h (peak 31 at 04h UTC) |

- Earlier baselines for the SAME cam and image family: kitchen VAAPI ~4.8/h, software decode 0 (118 min). So the crash loop is Frigate's VAAPI decode path; QSV and software both avoid it.
- Garage's crashes keep the `Could not find ref with POC` -> RPS -> `Failed to sync surface` chain; kitchen under QSV logged **no** decode errors at all.
- GPU hangs: 0 in Frigate's logs for the whole window (the kernel log was not readable without sudo, so this is Frigate's view only). The ~1800 hangs of 10/10-10/11 were in the QSV era but coincided with the cam's ref-loss storm; not reproduced.
- Cost: kitchen's ffmpeg 21% of a core (QSV) vs 13% (garage, VAAPI) vs ~37% (software decode); host CPU ~41%, iGPU 13-20%, OpenVINO detector 16-17 ms. Both cams 5 fps, 0 skipped.
- Caveats: kitchen is on a newer cam image than garage (v17/v18 gate + sndbuf vs v16), but kitchen's own VAAPI-on-v17 baseline was 4.8/h, so the decode path is the variable that matters.
  Frigate's recording continuity was not checked (the query was wrong); kitchen's 117 "crashes" in the reboot windows are retries, not decode failures.
- Recommendation: move garage (and nest if it crashes) to `hwaccel_args: preset-intel-qsv-h265`; Frigate restart is the user's. Config backups: `config.yml.pre-kitchen-qsv`.
- **2026-10-13: garage staged for QSV** (`cameras.garage.ffmpeg.hwaccel_args: preset-intel-qsv-h265`, backup `config.yml.pre-garage-qsv`, `--validate-config` OK). Takes effect when the user restarts Frigate
  (which also re-runs PTZ calibration on both cams). Revert = delete the line. After the restart: check `ps` shows `-hwaccel qsv` for garage and count garage crashes/h against the 13.4/h baseline.

## Cleanup and PR plan for the encoder work (2026-10-13; nothing pushed, nothing opened)

Done in `2861712`: the UI/README/hook-doc wording now matches what was measured (QP field = **minimum QP**, keyframe ceiling, bitrate behaviour, the hack.ini rate-control keys, `video rc`).
hack.ini keys (`IQP_*`) and command names (`iqp`) are unchanged on purpose: existing hack.ini files keep working. The en/ja YAML parses; the web bundle was not rebuilt (strings and comments only).

`encoder-tuning` is 45 commits on top of `onvif-on-118-presets`, with many dead ends. Do not PR it as is. Suggested split, each a branch off `main` (or off the PR #146 branch where it needs `relmove`/PTZ):

| PR | Contents (source commits) | Notes |
|---|---|---|
| A. RTSP robustness | live555 512 KB send buffer (`5b1aa9c` patch only), per-client wait-for-IRAP (`22f1b74`), `rtspserver.sh watchdog` (`a36034e`) | the highest-value, smallest change; fixes real stalls and Frigate crash loops. Drop the `sndbuf.so` LD_PRELOAD shim (`d5b7b48`): the live555 patch replaces it |
| B. Encoder knobs | `video iqp` / `gop` / `rc` / `stats` / `streambuf` / `streamcnt` (`8433495`, `f53639d`, `ef4feec`), `webcmd.sh` handlers (`c84b9d5`), UI + i18n + README + hook docs (`5b1aa9c` UI part, `2861712`) | squash the iqp/gop interposer back-and-forth (`088a96b`, `2da8a57`, `12bb5c9`, `cb8f42a`) into the final behaviour |
| C. Investigation tooling | `tools/encbench/` (bench, gopdrift, arrival, README tables) | optional; upstream may not want it. Keep fork-only if so |
| fork-only, never upstream | `AGENTS.md`, `CLAUDE.md`, `vendor-fw/`, the `.xapk` | already untracked or excluded; strip `AGENTS.md` from any branch that goes out |

Left in the tree on purpose, candidates to remove before a PR (needs a rebuild + cam test, so not done now):
- `video encdump|encrc|encgop` are research tools; keep behind a comment or drop once `video rc` covers the use.
- `tools/encbench/shim/sndbuf.c` + the `LD_PRELOAD` line in `rtspserver.sh` (redundant since v17).
- The loopback `max_buffers=8` / 640x360 change (`2b62e11`): measured as not fixing the stalls, kept as "harmless, less RAM".
- `local_sdk_video_set_gop` hook (`2da8a57`): marked useless in the v9 notes; the `set_fps` re-apply is what works.
- The `set_icamera_config.sh` / UI key names still say `IQP`; renaming to `MINQP_*` would need a hack.ini migration, so it stays unless upstream asks.

Not yet flashed anywhere: v19 (UI iqp/gop live apply). Kitchen runs v18, garage v16.

## PR branches cut (2026-10-13, local only, nothing pushed)

`main` (= upstream Ver.2.5.19, `313048b`) is the base of all three; verified in throwaway worktrees that A and B apply to plain `main`, that #146 merges with A + B without conflicts, and that B compiles with the firmware toolchain.

| branch | base | content | size |
|---|---|---|---|
| `pr/rtsp-robustness` (A) | `main` | live555 512 KB send buffer patch + `wmem_max` in `rtspserver.sh`; per-client wait-for-IRAP patch; `rtspserver.sh` capture watchdog | 3 commits, 3 files, +148 |
| `pr/encoder-knobs` (B) | `main` | `video iqp/gop/rc/stats/streambuf/streamcnt/encdump/encrc/encgop` + hack.ini keys; per-stream video UI + `webcmd.sh` handlers; README/hook docs | 3 commits, 9 files, +776 |
| PR #146 (`onvif-ptz-118` / `onvif-on-118-presets`) | `main` | ONVIF + PTZ, unchanged | 16 commits |
| `integration` | `onvif-on-118-presets` | #146 + A + B + fork-only `AGENTS.md` / `tools/encbench` | what the cams should be built from from now on |
| `encoder-tuning` | `onvif-on-118-presets` (older, 7534732) | the original 46-commit working history | kept as the record; do not PR |

- A and B are independent of each other and of #146. Only the docs/UI files overlap with #146 (`README.md`, `Setting.vue`, both i18n files, `webcmd.sh`, `libcallback_hook.md`) and they auto-merge; whichever PR merges second needs a README look.
- Dropped on purpose from A/B/integration: the `LD_PRELOAD sndbuf.so` branch in `rtspserver.sh` (redundant since the live555 patch), the v4l2loopback `max_buffers=8` + 640x360 trick (measured: no effect on the stalls),
  the useless `local_sdk_video_set_gop` hook and the interposer commits (squashed into the final behaviour), `tools/encbench/__pycache__/*.pyc` (was committed by mistake).
  `integration` vs `encoder-tuning` differs only by those drops and by #146's README ONVIF section.
- Image builds: cut the next image (v20) from `integration`; it will not contain the shim or the loopback tweak (both measured as unnecessary, but not re-tested on a cam).
- **Gotcha:** `encoder-tuning` forks from #146's branch BEFORE its README docs commit, so a diff against `onvif-on-118-presets` shows the ONVIF README sections as removed; ignore those when cutting patches.

### Is a send buffer above 512 KB worth it? No (measured/derived on kitchen, 2026-10-13)

- `SO_SNDBUF` is clamped to `net.core.wmem_max` (1 MB set by `rtspserver.sh`) and the kernel then doubles it: a 512 KB request is an effective **~1 MB** send buffer per client.
- The largest frame the encoder will emit is bounded by the max picture size (1920 kbit = 240 kB); measured peaks 170-222 kB. So there is ~4x headroom; the 17 kB peak send queue seen in 100 samples (one client) is a floor, not a peak, because a keyframe burst drains in milliseconds.
- Costs of going higher: RAM is the scarce resource (MemFree ~5 MB, 32 MB cache) and a stalled client (Wi-Fi drop) can pin up to the full buffer per connection; at 2 Mbps a 1 MB queue is 4 s of stale video.
- If anything, the request could go DOWN (256 KB request = 512 KB effective, still >2x the largest keyframe) to leave RAM headroom; no evidence of a need, so left as is.

---

# v20 / v21 and the keyframe interval (2026-10-14)

- **v20** (`rootfs_hack.v20-integration.squashfs`, sha256 `5a127a5a…`) = built from `integration`: no `sndbuf.so` preload, no loopback tweak. Flashed on **kitchen** (rollback
  `rootfs_hack.squashfs.bak-118-v18-20261014`), verified: boots in ~1 min, `video rc` min QP 25 applied, 3 clean joins, wmem_max 1 MB. Staged (not rebooted) on **garage**
  (rollback `bak-118-v16-20261014`).
- **GOP findings (kitchen, 15 fps, ffprobe keyframe pts over 22 s):** the encoder makes keyframes only at whole seconds.
  set 15/30/45/60 -> read-back identical, 1/2/3/4 s. Non-multiples: 20 -> reads 35 -> **3 s**; 25 -> 39 -> **3 s**; 40 -> 55 -> **4 s** (ceil(len/fps) + 1 s; the read-back is len + ~fps).
  So "GOP 40" in the old UI meant 4 s at night, and the read-back (+20 at 20 fps) was this same rule. Upstream's own note says the GOP must be divisible by the fps.
  Boot: kitchen came up with 30 although hack.ini said 40 (cause not proven; the `video gop` from `set_icamera_config.sh` ran, but something later left 30).
- **Fix (`f52ff80` on `pr/encoder-knobs`, merged into `integration`):** the setting is seconds (`video gop <ch> 1-10|auto`, hack.ini `GOP_SEC_MAIN_AVC|SUB_HEVC|MAIN_HEVC`, UI 1-10 s);
  the frame count is `seconds * live encoder fps` on every apply (so the 15/20 fps flip stays exact). `IQP_*` and `GOP_SEC_*` are also read from hack.ini in the `CreateChn` hook,
  and a keeper thread re-checks both every 5 s (writes only on a difference). The UI shows "Running now" (read back from the cam). Old `GOP_*` keys are ignored (stale, harmless).
- **v21** (`rootfs_hack.v21-gopsec.squashfs`, sha256 `463f7fdc…`, 46481408 B) = v20 + that libcallback (142472 B), `set_icamera_config.sh`, new web bundle `891e5dcc…`, `sndbuf.so` removed.
  Built, **not flashed** (the flash write was denied by the auto-mode classifier: needs the user's go for v21).

## v22 on both cams, PR A opened (2026-10-15)

- **Both cams run v22** (`rootfs_hack.v22-nolive.squashfs`, sha256 `57b6b612…`, built from `integration` `df2c177`): v21 minus the "Running now" UI block (the doc lives in the gop tooltips; cam-side
  read-back is `video gop <ch>` / `video iqp <ch>`). Rollbacks on both SDs: `rootfs_hack.squashfs.bak-pre-v22-20261014` (garage = v16, kitchen = v21); kitchen also `bak-118-v20-20261014`, `bak-118-v18-20261014`.
- Verified after reboot: kitchen booted at hack.ini state (`GOP_SEC_MAIN_HEVC=1` -> 1 s / 15 frames, min QP 25), keyframes every 1.00 s, joins clean. Garage booted clean, min QP 30 applied; capture was `off`
  for ~1 min after boot and the watchdog healed it (01:01:00); keyframes every 1.00 s; no shim file; `wmem_max` 1 MB. Garage's old `GOP_MAIN_HEVC=30` is stale (ignored): set the interval in the UI.
- Known leftovers: `video stats` prints the first `gap` as an unsigned underflow (4294967262 ms); a keeper thread now enforces min QP / keyframe interval every 5 s.
- **PR A opened: [mnakada/atomcam_tools#147](https://github.com/mnakada/atomcam_tools/pull/147)** (`specializedken:pr/rtsp-robustness` -> `main`, 3 commits). `pr/encoder-knobs` (B) is not pushed; it now also carries
  the seconds-based keyframe interval (`f52ff80`, `cb9fe8b`). Do not push other branches or open B without being asked.
- **PR B opened: [mnakada/atomcam_tools#148](https://github.com/mnakada/atomcam_tools/pull/148)** (`specializedken:pr/encoder-knobs` -> `main`, 6 commits incl. seconds-based keyframe interval and the self-contained rc-struct doc;
  README/hook docs no longer point at `tools/encbench`). Offered in the body to drop `encdump|encrc|encgop` if the maintainer prefers.
- **PR B (#148) was closed by the user's request right after opening (opened by mistake).** Only A (#147) is meant to be upstream for now. `pr/encoder-knobs` still exists on origin; do not reopen or
  re-push B unless explicitly asked.

# Picks from heronshoes' upstream PRs (2026-10-25)

Branch `heronshoes-picks` = `integration` + 6 cherry-picks (authorship kept, applied clean, `busybox sh -n` ok). **Not built, not flashed, not pushed.**
Taken: #164 (v4l2loopback buffers 16.6 MB -> ~1 MB per 1080p stream, `libcallback/video_callback.c`; matters with ~3.8 MB free RAM), #160 (logrotate state file on `/tmp/log`),
#154 (re-copy `atom_root.squashfs` by md5, not size), #158 (ntpd waits for the network at boot + cron watchdog; blocks boot up to ~30 s, check on a cam first).
Skipped: #150 (webhook zombie, we use no webhook), #152 (timelapse sample), #156 (midnight-crossing recording schedules, we do not record to SD), #162 (`make test-device` suite, 1379 lines; handy for post-flash checks if ever wanted).
Image needs the new `libcallback.so` (#164), `S42ntpd`, `S20mountfs`, `scripts/set_crontab.sh`. Flash only with the user's go.

**v7 image built 2026-10-25, not flashed:** `vendor-fw/work/on118/rootfs_hack.v7-heronshoes.squashfs`, sha256 `a78e8374…d156f`, 46481408 B (bytes_used check ok).
= v22 + `libcallback.so` rebuilt from `heronshoes-picks` (142632 B stripped; v22's was 142472) + `S42ntpd`, `S20mountfs`, `scripts/set_crontab.sh`. `diff -r` against v22 shows only those files
(the code delta v22 -> branch is these picks plus comment-only edits). Library built in the builder image with `make` + the cross `strip` (unstripped it is 158916 B; v22's was stripped).

**Garage runs v7 since 2026-10-25 23:30** (flashed by the user's hand after the classifier blocked the ssh write; rollback `rootfs_hack.squashfs.bak-pre-v7-20261025` = v22 on its SD).
Checked after boot: ntpd synced at uptime 30 s (try 1), 0 zombies after 2 min, RTSP up, ptzstatus ok, Frigate quiet. Not yet seen: `/tmp/log/logrotate.status` (first cron run at :45), a measured #164 vmalloc saving
(`VmallocUsed` 7908 kB with the streams up), a real autotrack. **Kitchen stays on v22 until garage has run for a while (user's call, 2026-10-25).**

# Autotrack overshoot = stale frames; cam-side settle tail (2026-10-29)

- Garage event 07:20 UTC: Frigate's first RelativeMove centred the person, then ~0.15 s later it sent the SAME move again -> person on the far edge, ping-pong, 15 fragmented events.
- Cause: Frigate sets `stop_time = frame_time` (arrival-time stamps, `use_wallclock_as_timestamps`) on the first IDLE GetStatus and only gates moves on `frame_time > stop_time`.
  Detect frames lag the cam by **~0.8-0.9 s** (measured: cam OSD clock vs `latest.jpg` fetch time, 12 s), so frames shot before/during the move pass the gate. Frigate-side design flaw, unchanged in `dev`;
  no issue/PR/discussion found (their docs blame overshoot on calibration). GH Discussion to write later.
- Workaround: `ptzstatus [settle_ms]` keeps moving=1 that long after the head stops (`libcallback/motor.c`); `onvif.sh` passes hack.ini `ONVIF_SETTLE_MS` (default 0 = off) in the get_position hook.
  Script uses of `ptzstatus` (run_move, contmove watchdog) pass no arg, so they are unaffected. Host stub test: 7/7 cases.
- **v23** `vendor-fw/work/on118/rootfs_hack.v23-settle.squashfs`, sha256 `7d94bb09…c1dc`, = v7-heronshoes + `libcallback.so` (143080 B) + `scripts/onvif.sh`.
  **Garage runs v23 since 2026-10-29 17:01 JST** with `ONVIF_SETTLE_MS=1000` (rollback `rootfs_hack.squashfs.bak-pre-v23-20261029` = v7, `hack.ini.bak-settle-20261029`). Verified over ONVIF (autotracker paused):
  +-5 deg RelativeMove returns in 0.21 s, MoveStatus stays MOVING ~0.95-0.98 s after it (16-17 GetStatus polls), then IDLE; back exactly at 143/74. Kitchen stays on v22.
  **Real walk-bys OK (2026-10-30):** 16:58 JST walk = one 120 s event (was 15 fragments in 8 min on 10-29), person enters at x 0.03, first move lands at 0.58 with no second move, then x stays 0.46-0.66; 9 moves in 131 s, shortest gap between moves 1.9 s (frame-diff, `moves.py`, scratch), no double moves.
  libcallback recipe reproduces v7's file byte-identical from unmodified source (uclibc gcc, `-DV4L2_PIX_FMT_HEVC=0x43564548`, tinyalsa `1c5fb68` include, `-L<rootfs>/lib/modules -ldl -l:libtinyalsa.so.2 -lm`, cross strip).
- Keep `calibrate_on_startup: false`: a recalibration would fold the tail into `movement_weights`.
