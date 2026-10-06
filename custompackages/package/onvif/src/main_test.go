package main

import (
	"fmt"
	"io/ioutil"
	"net"
	"net/http"
	"net/http/httptest"
	"os"
	"path/filepath"
	"strings"
	"sync"
	"testing"
	"time"
)

// fakeCam mimics the command socket: "move" reports the position, "move p t s pri" blocks then settles.
type fakeCam struct {
	ln        net.Listener
	mu        sync.Mutex
	pan, tilt float64
	cmds      []string
	busy      bool
	moveTime  time.Duration
}

func newFakeCam(t *testing.T) *fakeCam {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	f := &fakeCam{ln: ln, pan: 100, tilt: 90, moveTime: 50 * time.Millisecond}
	go func() {
		for {
			conn, err := ln.Accept()
			if err != nil {
				return
			}
			go f.handle(conn)
		}
	}()
	t.Cleanup(func() { ln.Close() })
	return f
}

func (f *fakeCam) handle(conn net.Conn) {
	defer conn.Close()
	buf := make([]byte, 256)
	n, _ := conn.Read(buf)
	cmd := strings.TrimSpace(string(buf[:n]))
	f.mu.Lock()
	f.cmds = append(f.cmds, cmd)
	f.mu.Unlock()
	var pan, tilt float64
	var speed, pri int
	if c, _ := fmt.Sscanf(cmd, "move %f %f %d %d", &pan, &tilt, &speed, &pri); c < 2 {
		f.mu.Lock()
		idle := 1
		if f.busy {
			idle = 0
		}
		fmt.Fprintf(conn, "%f %f 1 1 %d\n\x00", f.pan, f.tilt, idle)
		f.mu.Unlock()
		return
	}
	f.mu.Lock()
	f.busy = true
	f.mu.Unlock()
	time.Sleep(f.moveTime)
	f.mu.Lock()
	f.pan, f.tilt, f.busy = pan, tilt, false
	f.mu.Unlock()
	fmt.Fprintf(conn, "%f %f 1 1\n\x00", pan, tilt)
}

func (f *fakeCam) moves() []string {
	f.mu.Lock()
	defer f.mu.Unlock()
	var out []string
	for _, c := range f.cmds {
		if strings.HasPrefix(c, "move ") {
			out = append(out, c)
		}
	}
	return out
}

func setup(t *testing.T, maxSpeed int) (*Cam, *fakeCam, *httptest.Server) {
	f := newFakeCam(t)
	dir, _ := ioutil.TempDir("", "onvif")
	t.Cleanup(func() { os.RemoveAll(dir) })
	c := &Cam{addr: f.ln.Addr().String(), presetsPath: filepath.Join(dir, "p.json"),
		hfov: 108, vfov: 54, maxSpeed: maxSpeed}
	srv := httptest.NewServer(http.HandlerFunc(c.serve))
	t.Cleanup(srv.Close)
	return c, f, srv
}

func soap(t *testing.T, srv *httptest.Server, body string) (int, string) {
	req := fmt.Sprintf(`<?xml version="1.0"?><s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope"`+
		` xmlns:tptz="http://www.onvif.org/ver20/ptz/wsdl" xmlns:tt="http://www.onvif.org/ver10/schema"`+
		` xmlns:trt="http://www.onvif.org/ver10/media/wsdl" xmlns:tds="http://www.onvif.org/ver10/device/wsdl">`+
		`<s:Body>%s</s:Body></s:Envelope>`, body)
	r, err := http.Post(srv.URL, "application/soap+xml", strings.NewReader(req))
	if err != nil {
		t.Fatal(err)
	}
	defer r.Body.Close()
	b, _ := ioutil.ReadAll(r.Body)
	return r.StatusCode, string(b)
}

func waitIdle(c *Cam) {
	for i := 0; i < 100 && c.inflightNow() != 0; i++ {
		time.Sleep(10 * time.Millisecond)
	}
}

func TestDiscoveryOps(t *testing.T) {
	_, _, srv := setup(t, 9)
	for _, op := range []string{"GetCapabilities", "GetServices", "GetDeviceInformation", "GetSystemDateAndTime",
		"GetProfiles", "GetProfile", "GetStreamUri", "GetServiceCapabilities", "GetConfigurations",
		"GetConfiguration", "GetConfigurationOptions", "GetNodes", "GetNode"} {
		code, body := soap(t, srv, "<x:"+op+" xmlns:x=\"urn:x\"/>")
		if code != 200 || !strings.Contains(body, op+"Response") {
			t.Errorf("%s: %d %s", op, code, body)
		}
	}
	_, body := soap(t, srv, `<x:GetNode xmlns:x="urn:x"/>`)
	if !strings.Contains(body, "TranslationSpaceFov") {
		t.Error("FOV translation space not advertised")
	}
}

func TestUnknownOpFaults(t *testing.T) {
	_, _, srv := setup(t, 9)
	code, body := soap(t, srv, `<x:Bogus xmlns:x="urn:x"/>`)
	if code != 500 || !strings.Contains(body, "Fault") {
		t.Errorf("%d %s", code, body)
	}
}

func TestGetStatusMovingWhileInflight(t *testing.T) {
	c, f, srv := setup(t, 9)
	f.moveTime = 300 * time.Millisecond
	soap(t, srv, `<tptz:AbsoluteMove><tptz:Position><tt:PanTilt x="0" y="0"/></tptz:Position></tptz:AbsoluteMove>`)
	_, body := soap(t, srv, `<tptz:GetStatus/>`)
	if !strings.Contains(body, "MOVING") {
		t.Errorf("want MOVING right after a move: %s", body)
	}
	waitIdle(c)
	_, body = soap(t, srv, `<tptz:GetStatus/>`)
	if !strings.Contains(body, "IDLE") {
		t.Errorf("want IDLE after the move: %s", body)
	}
}

func TestAbsoluteMoveMapsAndCapsSpeed(t *testing.T) {
	c, f, srv := setup(t, 3)
	soap(t, srv, `<tptz:AbsoluteMove><tptz:Position><tt:PanTilt x="1" y="-1"/></tptz:Position>`+
		`<tptz:Speed><tt:PanTilt x="1" y="1"/></tptz:Speed></tptz:AbsoluteMove>`)
	waitIdle(c)
	if m := f.moves(); len(m) != 1 || m[0] != "move 355.00 0.00 3 2" {
		t.Errorf("moves = %v", m)
	}
}

func TestRelativeMoveFov(t *testing.T) {
	c, f, srv := setup(t, 9)
	soap(t, srv, `<tptz:RelativeMove><tptz:Translation>`+
		`<tt:PanTilt x="0.5" y="-1" space="http://www.onvif.org/ver10/tptz/PanTiltSpaces/TranslationSpaceFov"/>`+
		`</tptz:Translation><tptz:Speed><tt:PanTilt x="0.5" y="0.5"/></tptz:Speed></tptz:RelativeMove>`)
	waitIdle(c)
	// from 100/90: pan + 0.5*108/2 = 127, tilt - 1*54/2 = 63, speed ceil(.5*9) = 5
	m := f.moves()
	if len(m) != 1 || m[0] != "move 127.00 63.00 5 2" {
		t.Errorf("moves = %v", m)
	}
}

func TestContinuousMoveHeadsForLimit(t *testing.T) {
	c, f, srv := setup(t, 9)
	soap(t, srv, `<tptz:ContinuousMove><tptz:Velocity><tt:PanTilt x="-0.4" y="0"/></tptz:Velocity></tptz:ContinuousMove>`)
	waitIdle(c)
	if m := f.moves(); m[len(m)-1] != "move 0.00 90.00 4 2" {
		t.Errorf("moves = %v", m)
	}
}

func TestStopIsPriorityZeroAtCurrentPosition(t *testing.T) {
	c, f, srv := setup(t, 3)
	soap(t, srv, `<tptz:Stop/>`)
	waitIdle(c)
	if m := f.moves(); m[len(m)-1] != "move 100.00 90.00 3 0" {
		t.Errorf("moves = %v", m)
	}
}

func TestPresetsRoundTripAndEscaping(t *testing.T) {
	c, f, srv := setup(t, 9)
	_, body := soap(t, srv, `<tptz:SetPreset><tptz:PresetName>a &amp; b</tptz:PresetName></tptz:SetPreset>`)
	if !strings.Contains(body, "<tptz:PresetToken>1</tptz:PresetToken>") {
		t.Fatalf("SetPreset: %s", body)
	}
	_, body = soap(t, srv, `<tptz:GetPresets/>`)
	if !strings.Contains(body, `token="1"`) || !strings.Contains(body, "a &amp; b") {
		t.Errorf("GetPresets: %s", body)
	}
	f.mu.Lock()
	f.pan = 10
	f.mu.Unlock()
	soap(t, srv, `<tptz:GotoPreset><tptz:PresetToken>1</tptz:PresetToken></tptz:GotoPreset>`)
	waitIdle(c)
	if m := f.moves(); m[len(m)-1] != "move 100.00 90.00 9 2" {
		t.Errorf("moves = %v", m)
	}
	code, _ := soap(t, srv, `<tptz:GotoPreset><tptz:PresetToken>nope</tptz:PresetToken></tptz:GotoPreset>`)
	if code != 500 {
		t.Errorf("unknown preset: %d", code)
	}
	soap(t, srv, `<tptz:RemovePreset><tptz:PresetToken>1</tptz:PresetToken></tptz:RemovePreset>`)
	_, body = soap(t, srv, `<tptz:GetPresets/>`)
	if strings.Contains(body, "tptz:Preset ") {
		t.Errorf("preset not removed: %s", body)
	}
}

func TestPresetLimit(t *testing.T) {
	_, _, srv := setup(t, 9)
	for i := 0; i < maxPresets; i++ {
		if code, body := soap(t, srv, `<tptz:SetPreset/>`); code != 200 {
			t.Fatalf("preset %d: %s", i, body)
		}
	}
	if code, _ := soap(t, srv, `<tptz:SetPreset/>`); code != 500 {
		t.Error("expected preset limit fault")
	}
}

func TestCamOffline(t *testing.T) {
	c, _, srv := setup(t, 9)
	c.addr = "127.0.0.1:1"
	code, body := soap(t, srv, `<tptz:GetStatus/>`)
	if code != 500 || !strings.Contains(body, "Fault") {
		t.Errorf("%d %s", code, body)
	}
}
