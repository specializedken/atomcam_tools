package main

import (
	"encoding/json"
	"fmt"
	"io/ioutil"
	"log"
	"math"
	"net"
	"os"
	"sort"
	"strconv"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

const (
	panMax     = 355.0
	tiltMax    = 180.0
	maxPresets = 20
)

func clamp(v, lo, hi float64) float64 { return math.Max(lo, math.Min(hi, v)) }

type preset struct {
	Name string  `json:"name"`
	Pan  float64 `json:"pan"`
	Tilt float64 `json:"tilt"`
}

// Cam talks to atomcam_tools' command socket (localhost:4000) and keeps a preset store.
type Cam struct {
	addr        string
	presetsPath string
	hfov, vfov  float64
	maxSpeed    int
	inflight    int32
	mu          sync.Mutex
}

// exec sends one command; the cam answers with a NUL-terminated line and closes.
func (c *Cam) exec(cmd string, timeout time.Duration) (string, error) {
	conn, err := net.DialTimeout("tcp", c.addr, 3*time.Second)
	if err != nil {
		return "", err
	}
	defer conn.Close()
	conn.SetDeadline(time.Now().Add(timeout))
	if _, err := conn.Write([]byte(cmd)); err != nil {
		return "", err
	}
	var out []byte
	buf := make([]byte, 256)
	for {
		n, err := conn.Read(buf)
		out = append(out, buf[:n]...)
		if err != nil {
			break
		}
	}
	res := strings.TrimSpace(strings.Trim(string(out), "\x00"))
	if strings.HasPrefix(res, "error") {
		return res, fmt.Errorf("%s", res)
	}
	return res, nil
}

// position -> pan, tilt, idle (cam angles, already flip-corrected by the cam).
func (c *Cam) position() (float64, float64, bool, error) {
	res, err := c.exec("move", 5*time.Second)
	if err != nil {
		return 0, 0, false, err
	}
	f := strings.Fields(res)
	if len(f) < 5 {
		return 0, 0, false, fmt.Errorf("bad position reply %q", res)
	}
	pan, e1 := strconv.ParseFloat(f[0], 64)
	tilt, e2 := strconv.ParseFloat(f[1], 64)
	if e1 != nil || e2 != nil {
		return 0, 0, false, fmt.Errorf("bad position reply %q", res)
	}
	return pan, tilt, f[4] == "1", nil
}

// move fires an absolute move; the cam blocks until it ends, so run it in a goroutine.
func (c *Cam) move(pan, tilt float64, speed, pri int) {
	pan, tilt = clamp(pan, 0, panMax), clamp(tilt, 0, tiltMax)
	speed = int(clamp(float64(speed), 1, float64(c.maxSpeed)))
	cmd := fmt.Sprintf("move %.2f %.2f %d %d", pan, tilt, speed, pri)
	atomic.AddInt32(&c.inflight, 1) // before returning so GetStatus never races to IDLE
	go func() {
		defer atomic.AddInt32(&c.inflight, -1)
		if _, err := c.exec(cmd, 60*time.Second); err != nil {
			log.Printf("move failed: %v", err) // canceled by a higher priority move, or cam busy
		}
	}()
}

func (c *Cam) inflightNow() int32 { return atomic.LoadInt32(&c.inflight) }

func (c *Cam) toNorm(pan, tilt float64) (float64, float64) {
	return pan/panMax*2 - 1, tilt/tiltMax*2 - 1
}

func (c *Cam) fromNorm(x, y float64) (float64, float64) {
	return (x + 1) / 2 * panMax, (y + 1) / 2 * tiltMax
}

// speedToCam maps ONVIF speed 0..1 (nil = default) to cam speed 1..9.
func speedToCam(s *float64) int {
	if s == nil {
		return 9
	}
	return int(clamp(math.Ceil(math.Abs(*s)*9), 1, 9))
}

func (c *Cam) status() (x, y float64, idle bool, err error) {
	pan, tilt, idle, err := c.position()
	if err != nil {
		return
	}
	x, y = c.toNorm(pan, tilt)
	return x, y, idle && atomic.LoadInt32(&c.inflight) == 0, nil
}

func (c *Cam) absolute(x, y float64, speed *float64) {
	pan, tilt := c.fromNorm(clamp(x, -1, 1), clamp(y, -1, 1))
	c.move(pan, tilt, speedToCam(speed), 2)
}

func (c *Cam) relative(dx, dy float64, speed *float64) error {
	pan, tilt, _, err := c.position()
	if err != nil {
		return err
	}
	x, y := c.toNorm(pan, tilt)
	c.absolute(x+dx, y+dy, speed)
	return nil
}

// relativeFov: translation in FOV space, +-1 = half the field of view (frame edge).
func (c *Cam) relativeFov(dx, dy float64, speed *float64) error {
	pan, tilt, _, err := c.position()
	if err != nil {
		return err
	}
	c.move(pan+dx*c.hfov/2, tilt+dy*c.vfov/2, speedToCam(speed), 2)
	return nil
}

// continuous fakes velocity moves: head for the mechanical limit at a matching speed.
func (c *Cam) continuous(vx, vy float64) error {
	pan, tilt, _, err := c.position()
	if err != nil {
		return err
	}
	s := math.Max(math.Abs(vx), math.Abs(vy))
	if math.Abs(vx) > 1e-3 {
		pan = 0
		if vx > 0 {
			pan = panMax
		}
	}
	if math.Abs(vy) > 1e-3 {
		tilt = 0
		if vy > 0 {
			tilt = tiltMax
		}
	}
	c.move(pan, tilt, speedToCam(&s), 2)
	return nil
}

func (c *Cam) stop() error {
	pan, tilt, _, err := c.position()
	if err != nil {
		return err
	}
	// priority 0 cancels the in-flight move, then settles here
	c.move(pan, tilt, 9, 0)
	return nil
}

// -- presets ---------------------------------------------------------------
func (c *Cam) loadPresets() map[string]preset {
	p := map[string]preset{}
	if b, err := ioutil.ReadFile(c.presetsPath); err == nil {
		json.Unmarshal(b, &p)
	}
	return p
}

func (c *Cam) savePresets(p map[string]preset) error {
	b, err := json.MarshalIndent(p, "", " ")
	if err != nil {
		return err
	}
	tmp := c.presetsPath + ".tmp"
	if err := ioutil.WriteFile(tmp, b, 0644); err != nil {
		return err
	}
	return os.Rename(tmp, c.presetsPath)
}

func sortedTokens(p map[string]preset) []string {
	keys := make([]string, 0, len(p))
	for k := range p {
		keys = append(keys, k)
	}
	sort.Slice(keys, func(i, j int) bool {
		a, ea := strconv.Atoi(keys[i])
		b, eb := strconv.Atoi(keys[j])
		if ea == nil && eb == nil {
			return a < b
		}
		return keys[i] < keys[j]
	})
	return keys
}

func (c *Cam) setPreset(token, name string) (string, error) {
	c.mu.Lock()
	defer c.mu.Unlock()
	p := c.loadPresets()
	if token == "" {
		for i := 1; i <= maxPresets+1; i++ {
			if _, used := p[strconv.Itoa(i)]; !used {
				token = strconv.Itoa(i)
				break
			}
		}
	}
	if _, ok := p[token]; !ok && len(p) >= maxPresets {
		return "", fmt.Errorf("preset limit")
	}
	pan, tilt, _, err := c.position()
	if err != nil {
		return "", err
	}
	if name == "" {
		name = "Preset " + token
	}
	p[token] = preset{name, pan, tilt}
	return token, c.savePresets(p)
}

func (c *Cam) gotoPreset(token string, speed *float64) error {
	p, ok := c.loadPresets()[token]
	if !ok {
		return fmt.Errorf("unknown preset %q", token)
	}
	c.move(p.Pan, p.Tilt, speedToCam(speed), 2)
	return nil
}

func (c *Cam) removePreset(token string) error {
	c.mu.Lock()
	defer c.mu.Unlock()
	p := c.loadPresets()
	delete(p, token)
	return c.savePresets(p)
}
