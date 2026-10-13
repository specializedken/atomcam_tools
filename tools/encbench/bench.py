#!/usr/bin/env python3
"""Objective stream-quality bench for one AtomCam (run where cv2/numpy/ffmpeg exist, e.g. the Frigate container).

The camera must look at a STATIC scene. Reference = average of several JPEG snapshots (get_jpeg.cgi, noise
averaged out); each decoded RTSP frame is compared with it. Absolute numbers depend on the scene and light,
so compare configs run back to back (A/B/A), not across days.

  bench.py <cam-ip> [rtsp-path=video2_unicast] [label]
prints: bitrate (kbps, real), SSIM vs reference, detail (Laplacian variance, higher = more texture kept),
        frame-to-frame temporal noise (mean abs diff of consecutive frames, lower = steadier)
"""
import subprocess, sys, re, time, numpy as np, cv2

host = sys.argv[1]
path = sys.argv[2] if len(sys.argv) > 2 else "video2_unicast"
label = sys.argv[3] if len(sys.argv) > 3 else ""
import os
FF = os.environ.get("FFMPEG", "/usr/lib/ffmpeg/7.0/bin/ffmpeg")
W, H = 1920, 1080

def snapshot():
    p = subprocess.run(["curl", "-s", "-m", "20", f"http://{host}/cgi-bin/get_jpeg.cgi"], capture_output=True)
    return cv2.imdecode(np.frombuffer(p.stdout, np.uint8), cv2.IMREAD_GRAYSCALE)

def ssim(a, b):
    a = a.astype(np.float32); b = b.astype(np.float32)
    C1, C2 = (0.01 * 255) ** 2, (0.03 * 255) ** 2
    g = lambda x: cv2.GaussianBlur(x, (11, 11), 1.5)
    mu_a, mu_b = g(a), g(b)
    saa = g(a * a) - mu_a ** 2; sbb = g(b * b) - mu_b ** 2; sab = g(a * b) - mu_a * mu_b
    m = ((2 * mu_a * mu_b + C1) * (2 * sab + C2)) / ((mu_a ** 2 + mu_b ** 2 + C1) * (saa + sbb + C2))
    return float(m.mean())

def bitrate_kbps(sec=10):
    p = subprocess.run([FF, "-hide_banner", "-rtsp_transport", "tcp", "-t", str(sec), "-i", f"rtsp://{host}:8554/{path}",
                        "-c", "copy", "-an", "-f", "null", "-"], capture_output=True, text=True)
    m = re.findall(r"video:(\d+)(?:kB|KiB)", p.stderr)
    return int(m[-1]) * 8 / sec if m else float("nan")

def frames(n=8, skip=4, fps=2):
    cnt = n + skip
    p = subprocess.run([FF, "-loglevel", "error", "-rtsp_transport", "tcp", "-i", f"rtsp://{host}:8554/{path}",
                        "-vf", f"fps={fps},format=gray", "-frames:v", str(cnt), "-f", "rawvideo", "-pix_fmt", "gray", "-"],
                       capture_output=True, timeout=60)
    raw = np.frombuffer(p.stdout, np.uint8)
    k = len(raw) // (W * H)
    return [raw[i * W * H:(i + 1) * W * H].reshape(H, W) for i in range(skip, k)]

def main():
    snaps = [snapshot() for _ in range(5)]
    ref = np.mean([s.astype(np.float32) for s in snaps], axis=0)
    if ref.shape != (H, W): ref = cv2.resize(ref, (W, H))
    kbps = bitrate_kbps(10)
    fr = frames()
    if len(fr) < 4:
        print(f"{label}: not enough frames ({len(fr)})"); return
    ss = [ssim(f, ref) for f in fr]
    det = [float(cv2.Laplacian(f, cv2.CV_32F).var()) for f in fr]
    td = [float(np.mean(np.abs(fr[i].astype(np.int16) - fr[i - 1].astype(np.int16)))) for i in range(1, len(fr))]
    dref = float(cv2.Laplacian(ref.astype(np.float32), cv2.CV_32F).var())
    print(f"{label:34s} {kbps:6.0f} kbps | SSIM {np.mean(ss):.4f} (+-{np.std(ss):.4f}) | detail {np.mean(det):7.1f} (ref {dref:.1f}) | temporal noise {np.mean(td):.2f}")

main()
