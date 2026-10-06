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

**Not verified: anything on a real cam.** Nothing has been flashed or run on hardware.

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

## Open work / next steps

1. Build the minimal-diff image above; diff-verify it; get the user's OK; update one cam; enable ONVIF in the UI; point the Frigate `onvif:` host/port at the cam.
2. Decide before any upstream PR: auth story (WS-Security) vs. documented LAN-only; default port 8000 clash check; whether to keep `AGENTS.md` out of the PR.
3. Open the upstream PR **only when the user asks**. PR body must end with the Claude Code attribution line from the session system-reminder, and should mention the "run `build_all` twice for a new package" quirk.
4. Sibling repo `/media/Tac/kevin/dev/onvif-ptz` has **uncommitted** work: a `max_speed` cap in `onvif_ptz.py` + `config.toml` (`max_speed = 3`), container rebuilt and running,
   but no test, README or doc update yet. Per that repo's AGENTS.md also update `/media/Tac/kevin/dev/os-management` (`services/home-automation.md`, "PTZ via ONVIF shim...").
   The shim keeps running until the cams serve ONVIF themselves; Frigate config lives outside both repos (`/home/kevin/frigate/config.yml`, contains secrets - never copy into a repo;
   always `docker exec frigate python3 -m frigate --validate-config` before restarting Frigate).

## Rules of the road

- Additive and off by default. Don't touch boot-critical scripts beyond the guarded init script; every new boot path must `exit 0` on failure.
- Keep the daemon standard-library-only Go, single `package main`, terse comments that explain *why*. Add/adjust a test in `main_test.go` for any behaviour change; run vet + `-race`.
- Moving cams is physical and they are outdoors: small, slow moves, and return a cam to where you found it (read-only position: `/scripts/cmd move` on the cam,
  or `curl -s -X POST "http://<cam>/cgi-bin/cmd.cgi?port=socket" -d '{"exec":"move"}'`). Homes: garage 143.3/74.05, kitchen 163.5/77.0.
- Don't commit build outputs (`target/`, `atomcam_tools.zip`, `*.log` are gitignored) or secrets.
