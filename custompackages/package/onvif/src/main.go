// onvif: minimal ONVIF Device/Media/PTZ service for AtomSwing cams.
// Translates SOAP into the command socket ("move <pan> <tilt> <speed> <pri>") on localhost:4000.
package main

import (
	"flag"
	"fmt"
	"log"
	"net/http"
	"time"
)

func (c *Cam) serve(w http.ResponseWriter, r *http.Request) {
	if r.Method != http.MethodPost {
		http.Error(w, "POST only", http.StatusMethodNotAllowed)
		return
	}
	var out string
	code := http.StatusOK
	root, err := parse(http.MaxBytesReader(w, r.Body, 1<<16))
	if err == nil {
		body := root.find("Body")
		if body == nil || len(body.kids) == 0 {
			err = fmt.Errorf("no SOAP body")
		} else {
			var inner string
			inner, err = c.handle(r.Host, body.kids[0].name, body.kids[0])
			out = fmt.Sprintf(env, inner)
		}
	}
	if err != nil {
		code = http.StatusInternalServerError
		if _, ok := err.(notSupported); ok {
			out = fault("Action not supported: " + err.Error())
		} else {
			log.Printf("error: %v", err)
			out = fault(err.Error())
		}
	}
	w.Header().Set("Content-Type", "application/soap+xml; charset=utf-8")
	w.WriteHeader(code)
	fmt.Fprint(w, out)
}

func main() {
	listen := flag.String("listen", ":8000", "address to serve ONVIF on")
	camAddr := flag.String("cam", "127.0.0.1:4000", "atomcam_tools command socket")
	presets := flag.String("presets", "/media/mmc/onvif_presets.json", "preset store")
	maxSpeed := flag.Int("max-speed", 9, "cap on cam speed, 1 (slow) - 9 (fast)")
	hfov := flag.Float64("hfov", 108, "horizontal field of view in degrees (TranslationSpaceFov)")
	vfov := flag.Float64("vfov", 54, "vertical field of view in degrees")
	flag.Parse()

	c := &Cam{addr: *camAddr, presetsPath: *presets, hfov: *hfov, vfov: *vfov,
		maxSpeed: int(clamp(float64(*maxSpeed), 1, 9))}
	mux := http.NewServeMux()
	mux.HandleFunc("/", c.serve)
	srv := &http.Server{Addr: *listen, Handler: mux, ReadTimeout: 10 * time.Second, WriteTimeout: 30 * time.Second}
	log.Printf("onvif: listening on %s -> %s (max speed %d)", *listen, *camAddr, c.maxSpeed)
	log.Fatal(srv.ListenAndServe())
}
