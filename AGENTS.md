# AGENTS.md

Guidance for AI coding agents working in this fork.

> `CLAUDE.md` is a **symlink to this file** so Claude Code picks it up. Edit `AGENTS.md`, never `CLAUDE.md`.
> This file is fork-only context. **Drop it (and the `CLAUDE.md` symlink) from anything sent upstream.**

## What this is

Fork of [mnakada/atomcam_tools](https://github.com/mnakada/atomcam_tools) (custom firmware for ATOM Cam / **AtomSwing**).
Upstream has pan/tilt but **no ONVIF**. This fork adds a small ONVIF Device/Media/PTZ service **on the cam itself**,
so NVRs (we use Frigate, with autotracking) can drive the two outdoor Swings directly. It is the upstream-able
successor of the external shim in the sibling repo `/media/Tac/kevin/dev/onvif-ptz` (`onvif_ptz.py`, Python, runs on
the server and talks to each cam over HTTP). Same behaviour, ported to Go and run on the cam.

- Fork: `origin` = https://github.com/specializedken/atomcam_tools, `upstream` = mnakada/atomcam_tools
- Branch: **`onvif-ptz`** (pushed to origin). Feature commit `35fe9a2`, on top of upstream `313048b` (Ver.2.5.19).
- **No PR has been opened, on purpose.** The user said not to. Do not open one (or push elsewhere) without being asked.

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
  `GetStatus` reports MOVING while any daemon-issued move is in flight. FOV ~108 x 54 deg (`TranslationSpaceFov`, +-1 = half FOV).
- pan/tilt direction assumed +1 (the cam already flip-corrects). **Verified only on the two h+v-flipped cams**, not on unflipped ones.
- No auth (WS-Security), no zoom, no ONVIF home position, no EFlip. **Unauthenticated on the LAN** - the UI tooltip and README say so;
  a maintainer may want auth before merging.

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
Frigate's kitchen `onvif:` now points at `192.168.1.50:8000` (max speed 3 to match the old shim's calibration; `home` preset seeded by hand). Garage is still on stock 2.5.19 + the shim.
Not yet verified: a live Frigate autotrack, concurrent clients, the new "Save home position" button (committed, not yet in any flashed image).

## The cams and the update path (important)

- Cams: `garage` 192.168.1.129, `kitchen` 192.168.1.50 (DHCP). Both run atomcam_tools **2.5.19**, kernel `#2 PREEMPT Sun May 11 07:01:36 UTC 2025`.
  They feed Frigate; both are outdoors. **The user has no easy physical access to them or their SD cards.**
- Update without the SD card: Web UI -> Maintenance -> "Custom update ZIP" + URL -> Update. The cam `curl`s the zip into `/media/mmc/update`
  and reboots; `initramfs_skeleton/init` unzips it, checks sizes, and `mv -f`s `rootfs_hack.squashfs` (and the kernel, if present) into place.
  A loose `rootfs_hack.squashfs` dropped in `update/` over Samba works too. **There is no backup of the old rootfs and no rollback**:
  if the new rootfs does not boot, the cam is stuck until someone reaches the SD card.
- **Do not flash our full build.** Diffed against the official 2.5.19 release (`gh release download Ver.2.5.19 -R mnakada/atomcam_tools`):
  file list matches except our 3 new files + the renamed web bundle, but **1229 existing files differ** (mostly embedded build timestamps; busybox is
  same size/version), 14 differ in size (incl. `usr/bin/videocapture`, `libfdk-aac`, `scripts/webcmd.sh` which is ours), `/etc/shadow` hashes differ,
  and our kernel differs by ~734 KB. Cannot prove those are harmless.
- **Plan (not built yet):** unpack the *official* 2.5.19 `rootfs_hack.squashfs` (as root, e.g. in the builder container, to keep device nodes/ownership;
  reuse its compression/block size from `unsquashfs -s`), add only: `usr/bin/onvif`, `etc/init.d/S76onvif`, `scripts/onvif.sh`, patched `scripts/webcmd.sh`,
  and the new web bundle + `index.html`; repack. Ship a zip containing **only** `rootfs_hack.squashfs` (never the kernel). Host it on the LAN from this
  machine (e.g. `python3 -m http.server`). Before updating, copy `/media/mmc/rootfs_hack.squashfs` off the cam over ssh as a manual-rollback copy.
  Try **one cam first**; leave the other untouched. Sanity-check the result with `unsquashfs -l` + a diff against the official tree (should show only our files).
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
Another way to update once ssh works: stream the squashfs to `/media/mmc/update/rootfs_hack.squashfs` and reboot (init moves it into place; keep a `.bak` of the old one on the SD first).
Diffed against official: exactly those files differ.

## Open work / next steps

1. Build the minimal-diff image above; diff-verify it; get the user's OK; update one cam; enable ONVIF in the UI; point the Frigate `onvif:` host/port at the cam.
2. Decide before any upstream PR: auth story (WS-Security) vs. documented LAN-only; default port 8000 clash check; whether to keep `AGENTS.md` out of the PR.
3. Open the upstream PR **only when the user asks**. PR body must end with the Claude Code attribution line from the session system-reminder, and should mention the "run `build_all` twice for a new package" quirk.
4. **Native relative move: DONE and verified on kitchen (2026-10-08).** `relmove <dpan> <dtilt> [speed] [pri]` in `libcallback/motor.c` (`local_sdk_motor_move_rel_angle`: degrees -> steps -> `move_rel_step`,
   clamped +-2130/+-1580 steps; hflip/vflip signs negated; done-callback re-reads the position like the vendor app) and the daemon's `RelativeMove` (FOV + generic space) is now one `relmove` command, no read-then-move.
   Image `vendor-fw/work/rootfs_hack.v2.squashfs` (new `libcallback.so` built with the firmware's own toolchain: the unmodified source gives exactly the official 129252 bytes) is running on kitchen.
   Verified on the cam: +-5 pan/tilt exact (tilt quantised to ~0.1 deg), out-and-back returns to the exact start (old read+abs path drifted 0.1-0.3 deg per round trip), same sign as abs `move`,
   SDK clamps at the mechanical limit (tilt 188 -> 180.0, no grinding), same/lower pri while busy -> `error : dismiss request.`, pri 0 preempts (victim gets `error : multiple request.`),
   full onvif-zeep suite passes. Caveats: cancelling a *relmove* with a pri-0 move ends 1-2 deg off target (abs-vs-abs cancel is exact; next abs move fixes it, not cumulative, only affects `Stop`);
   one unreproduced overlapping-test reading (184 vs 169); the speed argument seems to change little (10 deg at speed 1 took ~0.3 s). Test scripts must not wipe presets (kitchen's `home` was deleted once and restored).
   Leftover ideas: native `move_track`/`cruise`/`goback`, `local_sdk_motor_move` for true ContinuousMove (needs a hook too).
5. Sibling repo `/media/Tac/kevin/dev/onvif-ptz` has **uncommitted** work: a `max_speed` cap in `onvif_ptz.py` + `config.toml` (`max_speed = 3`), container rebuilt and running,
   but no test, README or doc update yet. Per that repo's AGENTS.md also update `/media/Tac/kevin/dev/os-management` (`services/home-automation.md`, "PTZ via ONVIF shim...").
   The shim keeps running until the cams serve ONVIF themselves; Frigate config lives outside both repos (`/home/kevin/frigate/config.yml`, contains secrets - never copy into a repo;
   always `docker exec frigate python3 -m frigate --validate-config` before restarting Frigate).

## Rules of the road

- Additive and off by default. Don't touch boot-critical scripts beyond the guarded init script; every new boot path must `exit 0` on failure.
- Keep the daemon standard-library-only Go, single `package main`, terse comments that explain *why*. Add/adjust a test in `main_test.go` for any behaviour change; run vet + `-race`.
- Moving cams is physical and they are outdoors: small, slow moves, and return a cam to where you found it (read-only position: `/scripts/cmd move` on the cam,
  or `curl -s -X POST "http://<cam>/cgi-bin/cmd.cgi?port=socket" -d '{"exec":"move"}'`). Homes: garage 143.3/74.05, kitchen 163.5/77.0.
- Don't commit build outputs (`target/`, `atomcam_tools.zip`, `*.log` are gitignored) or secrets.
