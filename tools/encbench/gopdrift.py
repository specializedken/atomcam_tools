#!/usr/bin/env python3
"""Per-frame quality vs position in the GOP (shows keyframe 'pumping').

  gopdrift.py <cam-ip> <rtsp-path> [seconds=8]
Decodes every frame, SSIM vs an averaged snapshot reference, groups by frames since the last I frame.
Needs cv2/numpy; ffmpeg/ffprobe from PATH (override with FFMPEG / FFPROBE).
"""
import subprocess, sys, os, json, numpy as np, cv2
host, path = sys.argv[1], sys.argv[2]
sec = int(sys.argv[3]) if len(sys.argv) > 3 else 8
FF = os.environ.get("FFMPEG", "ffmpeg"); FP = os.environ.get("FFPROBE", "ffprobe")
W, H = 1920, 1080
url = f"rtsp://{host}:8554/{path}"

def snap():
    p = subprocess.run(["curl", "-s", "-m", "20", f"http://{host}/cgi-bin/get_jpeg.cgi"], capture_output=True)
    return cv2.imdecode(np.frombuffer(p.stdout, np.uint8), cv2.IMREAD_GRAYSCALE)

def ssim(a, b):
    a = a.astype(np.float32); b = b.astype(np.float32)
    C1, C2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    g = lambda x: cv2.GaussianBlur(x, (11, 11), 1.5)
    ma, mb = g(a), g(b)
    saa = g(a * a) - ma ** 2; sbb = g(b * b) - mb ** 2; sab = g(a * b) - ma * mb
    return float((((2 * ma * mb + C1) * (2 * sab + C2)) / ((ma ** 2 + mb ** 2 + C1) * (saa + sbb + C2))).mean())

snaps = []
for _ in range(6):
    s = snap()
    if s is not None: snaps.append(s.astype(np.float32))
ref = np.mean(snaps, axis=0)

# one connection: save the stream to a file, then probe + decode the file (keeps frame order identical)
tmp = f"/tmp/gopdrift_{os.getpid()}.mkv"
subprocess.run([FF, "-loglevel", "error", "-y", "-rtsp_transport", "tcp", "-t", str(sec), "-i", url, "-c", "copy", "-an", tmp], timeout=sec + 40)
pr = subprocess.run([FP, "-v", "error", "-select_streams", "v", "-show_entries", "frame=pict_type,pkt_size,key_frame",
                     "-of", "json", tmp], capture_output=True, text=True)
fr = json.loads(pr.stdout)["frames"]
raw = subprocess.run([FF, "-loglevel", "error", "-i", tmp, "-f", "rawvideo", "-pix_fmt", "gray", "-"], capture_output=True).stdout
os.unlink(tmp)
n = min(len(fr), len(raw) // (W * H))
rows = []; since = 0; prev = None
for i in range(n):
    if fr[i]["pict_type"] == "I": since = 0
    f = np.frombuffer(raw, np.uint8, W * H, i * W * H).reshape(H, W)
    d = float(np.mean(np.abs(f.astype(np.int16) - prev))) if prev is not None else float("nan")
    lap = float(cv2.Laplacian(f, cv2.CV_32F).var())
    rows.append((since, int(fr[i]["pkt_size"]), ssim(f, ref), fr[i]["pict_type"], d, lap))
    prev = f.astype(np.int16); since += 1
print(f"{path}: {n} frames, {sum(r[1] for r in rows) * 8 / sec / 1000:.0f} kbps, I-frames {sum(r[3]=='I' for r in rows)}")
print("since_I  n  avg_kB  avg_SSIM  diff_to_prev  detail")
for k in sorted(set(r[0] for r in rows)):
    g = [r for r in rows if r[0] == k]
    print(f"{k:5d} {len(g):3d} {np.mean([r[1] for r in g])/1000:7.1f}  {np.mean([r[2] for r in g]):.4f}   {np.nanmean([r[4] for r in g]):6.2f}   {np.mean([r[5] for r in g]):7.1f}")
