package main

import (
	"encoding/xml"
	"fmt"
	"io"
	"net"
	"os"
	"strconv"
	"strings"
	"time"
)

const (
	sp  = "http://www.onvif.org/ver10/tptz/"
	env = `<?xml version="1.0" encoding="UTF-8"?>` +
		`<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" ` +
		`xmlns:tt="http://www.onvif.org/ver10/schema" ` +
		`xmlns:tds="http://www.onvif.org/ver10/device/wsdl" ` +
		`xmlns:trt="http://www.onvif.org/ver10/media/wsdl" ` +
		`xmlns:tptz="http://www.onvif.org/ver20/ptz/wsdl"><s:Body>%s</s:Body></s:Envelope>`
)

// node is a namespace-agnostic XML element (only local names matter to ONVIF clients).
type node struct {
	name string
	attr map[string]string
	text string
	kids []*node
}

func parse(r io.Reader) (*node, error) {
	d := xml.NewDecoder(r)
	var stack []*node
	var root *node
	for {
		t, err := d.Token()
		if err == io.EOF {
			break
		}
		if err != nil {
			return nil, err
		}
		switch e := t.(type) {
		case xml.StartElement:
			n := &node{name: e.Name.Local, attr: map[string]string{}}
			for _, a := range e.Attr {
				n.attr[a.Name.Local] = a.Value
			}
			if len(stack) > 0 {
				p := stack[len(stack)-1]
				p.kids = append(p.kids, n)
			} else {
				root = n
			}
			stack = append(stack, n)
		case xml.EndElement:
			stack = stack[:len(stack)-1]
		case xml.CharData:
			if len(stack) > 0 {
				stack[len(stack)-1].text += string(e)
			}
		}
	}
	if root == nil {
		return nil, fmt.Errorf("empty request")
	}
	return root, nil
}

// find returns the first element called name, depth first, n included.
func (n *node) find(name string) *node {
	if n == nil {
		return nil
	}
	if n.name == name {
		return n
	}
	for _, k := range n.kids {
		if f := k.find(name); f != nil {
			return f
		}
	}
	return nil
}

func (n *node) str(name string) string {
	if e := n.find(name); e != nil {
		return strings.TrimSpace(e.text)
	}
	return ""
}

// xy reads the x/y attributes of the first <name> element.
func (n *node) xy(name string) (x, y float64, ok bool) {
	e := n.find(name)
	if e == nil {
		return 0, 0, false
	}
	xs, ok1 := e.attr["x"]
	ys, ok2 := e.attr["y"]
	if !ok1 || !ok2 {
		return 0, 0, false
	}
	x, e1 := strconv.ParseFloat(xs, 64)
	y, e2 := strconv.ParseFloat(ys, 64)
	return x, y, e1 == nil && e2 == nil
}

// speed returns max(|x|,|y|) of <Speed><PanTilt>, or nil when absent.
func (n *node) speed() *float64 {
	s := n.find("Speed")
	if s == nil {
		return nil
	}
	x, y, ok := s.xy("PanTilt")
	if !ok {
		return nil
	}
	v := max64(abs64(x), abs64(y))
	return &v
}

func abs64(v float64) float64 {
	if v < 0 {
		return -v
	}
	return v
}

func max64(a, b float64) float64 {
	if a > b {
		return a
	}
	return b
}

func esc(s string) string {
	var b strings.Builder
	xml.EscapeText(&b, []byte(s))
	return b.String()
}

func vec(x, y float64, space string) string {
	s := ""
	if space != "" {
		s = fmt.Sprintf(` space="%s"`, space)
	}
	return fmt.Sprintf(`<tt:PanTilt x="%.5f" y="%.5f"%s/>`, x, y, s)
}

func fault(msg string) string {
	return fmt.Sprintf(env, `<s:Fault><s:Code><s:Value>s:Receiver</s:Value></s:Code>`+
		`<s:Reason><s:Text xml:lang="en">`+esc(msg)+`</s:Text></s:Reason></s:Fault>`)
}

func rng(tag, uri string, lo, hi int) string {
	return fmt.Sprintf(`<tt:%s><tt:URI>%s</tt:URI>`+
		`<tt:XRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:XRange>`+
		`<tt:YRange><tt:Min>%d</tt:Min><tt:Max>%d</tt:Max></tt:YRange></tt:%s>`,
		tag, uri, lo, hi, lo, hi, tag)
}

var spaces = rng("AbsolutePanTiltPositionSpace", sp+"PanTiltSpaces/PositionGenericSpace", -1, 1) +
	rng("RelativePanTiltTranslationSpace", sp+"PanTiltSpaces/TranslationGenericSpace", -1, 1) +
	rng("RelativePanTiltTranslationSpace", sp+"PanTiltSpaces/TranslationSpaceFov", -1, 1) +
	rng("ContinuousPanTiltVelocitySpace", sp+"PanTiltSpaces/VelocityGenericSpace", -1, 1) +
	`<tt:PanTiltSpeedSpace><tt:URI>` + sp + `PanTiltSpaces/GenericSpeedSpace</tt:URI>` +
	`<tt:XRange><tt:Min>0</tt:Min><tt:Max>1</tt:Max></tt:XRange></tt:PanTiltSpeedSpace>`

// ptzConf/node use %s for the element prefix ("tt" inside profiles, "tptz" in PTZ responses).
const ptzConfFmt = `<%[1]s:PTZConfiguration token="ptzconf1"><tt:Name>PTZ</tt:Name><tt:UseCount>1</tt:UseCount>` +
	`<tt:NodeToken>ptznode1</tt:NodeToken>` +
	`<tt:DefaultAbsolutePantTiltPositionSpace>` + sp + `PanTiltSpaces/PositionGenericSpace</tt:DefaultAbsolutePantTiltPositionSpace>` +
	`<tt:DefaultRelativePanTiltTranslationSpace>` + sp + `PanTiltSpaces/TranslationGenericSpace</tt:DefaultRelativePanTiltTranslationSpace>` +
	`<tt:DefaultContinuousPanTiltVelocitySpace>` + sp + `PanTiltSpaces/VelocityGenericSpace</tt:DefaultContinuousPanTiltVelocitySpace>` +
	`<tt:DefaultPTZSpeed><tt:PanTilt x="1" y="1" space="` + sp + `PanTiltSpaces/GenericSpeedSpace"/></tt:DefaultPTZSpeed>` +
	`<tt:DefaultPTZTimeout>PT10S</tt:DefaultPTZTimeout></%[1]s:PTZConfiguration>`

func ptzConf(prefix string) string { return fmt.Sprintf(ptzConfFmt, prefix) }

func ptzNode(prefix string) string {
	return fmt.Sprintf(`<%[1]s:PTZNode token="ptznode1" FixedHomePosition="false"><tt:Name>PTZNode</tt:Name>`+
		`<tt:SupportedPTZSpaces>%[2]s</tt:SupportedPTZSpaces>`+
		`<tt:MaximumNumberOfPresets>%[3]d</tt:MaximumNumberOfPresets>`+
		`<tt:HomeSupported>false</tt:HomeSupported></%[1]s:PTZNode>`, prefix, spaces, maxPresets)
}

func profile(tag string) string {
	return `<trt:` + tag + ` fixed="true" token="profile1"><tt:Name>profile1</tt:Name>` +
		`<tt:VideoSourceConfiguration token="vsc1"><tt:Name>vsc1</tt:Name><tt:UseCount>1</tt:UseCount>` +
		`<tt:SourceToken>vs1</tt:SourceToken><tt:Bounds x="0" y="0" width="1920" height="1080"/>` +
		`</tt:VideoSourceConfiguration>` +
		`<tt:VideoEncoderConfiguration token="vec1"><tt:Name>vec1</tt:Name><tt:UseCount>1</tt:UseCount>` +
		`<tt:Encoding>H264</tt:Encoding><tt:Resolution><tt:Width>1920</tt:Width><tt:Height>1080</tt:Height></tt:Resolution>` +
		`<tt:Quality>5</tt:Quality><tt:RateControl><tt:FrameRateLimit>15</tt:FrameRateLimit>` +
		`<tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>2048</tt:BitrateLimit></tt:RateControl>` +
		`<tt:Multicast><tt:Address><tt:Type>IPv4</tt:Type><tt:IPv4Address>0.0.0.0</tt:IPv4Address></tt:Address>` +
		`<tt:Port>0</tt:Port><tt:TTL>1</tt:TTL><tt:AutoStart>false</tt:AutoStart></tt:Multicast>` +
		`<tt:SessionTimeout>PT60S</tt:SessionTimeout></tt:VideoEncoderConfiguration>` +
		ptzConf("tt") + `</trt:` + tag + `>`
}

type notSupported string

func (e notSupported) Error() string { return string(e) }

func hostOnly(h string) string {
	if host, _, err := net.SplitHostPort(h); err == nil {
		return host
	}
	return h
}

// handle returns the SOAP body for operation op; notSupported for unknown ops.
func (c *Cam) handle(host, op string, root *node) (string, error) {
	base := "http://" + host
	switch op {
	case "GetCapabilities":
		return `<tds:GetCapabilitiesResponse><tds:Capabilities>` +
			`<tt:Device><tt:XAddr>` + base + `/onvif/device_service</tt:XAddr></tt:Device>` +
			`<tt:Media><tt:XAddr>` + base + `/onvif/media_service</tt:XAddr></tt:Media>` +
			`<tt:PTZ><tt:XAddr>` + base + `/onvif/ptz_service</tt:XAddr></tt:PTZ>` +
			`</tds:Capabilities></tds:GetCapabilitiesResponse>`, nil
	case "GetServices":
		svc := func(ns, path string) string {
			return `<tds:Service><tds:Namespace>` + ns + `</tds:Namespace><tds:XAddr>` + base + `/onvif/` + path + `</tds:XAddr>` +
				`<tds:Version><tt:Major>2</tt:Major><tt:Minor>0</tt:Minor></tds:Version></tds:Service>`
		}
		return `<tds:GetServicesResponse>` +
			svc("http://www.onvif.org/ver10/device/wsdl", "device_service") +
			svc("http://www.onvif.org/ver10/media/wsdl", "media_service") +
			svc("http://www.onvif.org/ver20/ptz/wsdl", "ptz_service") +
			`</tds:GetServicesResponse>`, nil
	case "GetDeviceInformation":
		name, _ := os.Hostname()
		return `<tds:GetDeviceInformationResponse><tds:Manufacturer>atomcam_tools</tds:Manufacturer>` +
			`<tds:Model>AtomSwing</tds:Model><tds:FirmwareVersion>atomcam_tools</tds:FirmwareVersion>` +
			`<tds:SerialNumber>` + esc(name) + `</tds:SerialNumber><tds:HardwareId>atomswing</tds:HardwareId>` +
			`</tds:GetDeviceInformationResponse>`, nil
	case "GetSystemDateAndTime":
		t := time.Now().UTC()
		return fmt.Sprintf(`<tds:GetSystemDateAndTimeResponse><tds:SystemDateAndTime>`+
			`<tt:DateTimeType>NTP</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>`+
			`<tt:UTCDateTime><tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>`+
			`<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date></tt:UTCDateTime>`+
			`</tds:SystemDateAndTime></tds:GetSystemDateAndTimeResponse>`,
			t.Hour(), t.Minute(), t.Second(), t.Year(), int(t.Month()), t.Day()), nil
	case "GetProfiles":
		return `<trt:GetProfilesResponse>` + profile("Profiles") + `</trt:GetProfilesResponse>`, nil
	case "GetProfile":
		return `<trt:GetProfileResponse>` + profile("Profile") + `</trt:GetProfileResponse>`, nil
	case "GetStreamUri":
		return `<trt:GetStreamUriResponse><trt:MediaUri>` +
			`<tt:Uri>rtsp://` + hostOnly(host) + `:8554/video0_unicast</tt:Uri><tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>` +
			`<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot><tt:Timeout>PT0S</tt:Timeout>` +
			`</trt:MediaUri></trt:GetStreamUriResponse>`, nil
	case "GetServiceCapabilities":
		return `<tptz:GetServiceCapabilitiesResponse><tptz:Capabilities EFlip="false" Reverse="false" ` +
			`GetCompatibleConfigurations="false" MoveStatus="true" StatusPosition="true"/>` +
			`</tptz:GetServiceCapabilitiesResponse>`, nil
	case "GetConfigurations":
		return `<tptz:GetConfigurationsResponse>` + ptzConf("tptz") + `</tptz:GetConfigurationsResponse>`, nil
	case "GetConfiguration":
		return `<tptz:GetConfigurationResponse>` + ptzConf("tptz") + `</tptz:GetConfigurationResponse>`, nil
	case "GetConfigurationOptions":
		return `<tptz:GetConfigurationOptionsResponse><tptz:PTZConfigurationOptions>` +
			`<tt:Spaces>` + spaces + `</tt:Spaces><tt:PTZTimeout><tt:Min>PT0S</tt:Min><tt:Max>PT60S</tt:Max></tt:PTZTimeout>` +
			`</tptz:PTZConfigurationOptions></tptz:GetConfigurationOptionsResponse>`, nil
	case "GetNodes":
		return `<tptz:GetNodesResponse>` + ptzNode("tptz") + `</tptz:GetNodesResponse>`, nil
	case "GetNode":
		return `<tptz:GetNodeResponse>` + ptzNode("tptz") + `</tptz:GetNodeResponse>`, nil
	case "GetStatus":
		x, y, idle, err := c.status()
		if err != nil {
			return "", err
		}
		st := "MOVING"
		if idle {
			st = "IDLE"
		}
		return `<tptz:GetStatusResponse><tptz:PTZStatus><tt:Position>` +
			vec(x, y, sp+"PanTiltSpaces/PositionGenericSpace") + `</tt:Position>` +
			`<tt:MoveStatus><tt:PanTilt>` + st + `</tt:PanTilt></tt:MoveStatus>` +
			`<tt:UtcTime>` + time.Now().UTC().Format("2006-01-02T15:04:05Z") + `</tt:UtcTime>` +
			`</tptz:PTZStatus></tptz:GetStatusResponse>`, nil
	case "ContinuousMove":
		var vx, vy float64
		if v := root.find("Velocity"); v != nil {
			vx, vy, _ = v.xy("PanTilt")
		}
		if err := c.continuous(vx, vy); err != nil {
			return "", err
		}
		return `<tptz:ContinuousMoveResponse/>`, nil
	case "AbsoluteMove":
		x, y, ok := root.find("Position").xy("PanTilt")
		if !ok {
			return "", fmt.Errorf("missing Position")
		}
		c.absolute(x, y, root.speed())
		return `<tptz:AbsoluteMoveResponse/>`, nil
	case "RelativeMove":
		t := root.find("Translation")
		x, y, ok := t.xy("PanTilt")
		if !ok {
			return "", fmt.Errorf("missing Translation")
		}
		var err error
		if strings.Contains(t.find("PanTilt").attr["space"], "Fov") {
			err = c.relativeFov(x, y, root.speed())
		} else {
			err = c.relative(x, y, root.speed())
		}
		if err != nil {
			return "", err
		}
		return `<tptz:RelativeMoveResponse/>`, nil
	case "Stop":
		if err := c.stop(); err != nil {
			return "", err
		}
		return `<tptz:StopResponse/>`, nil
	case "GetPresets":
		p := c.loadPresets()
		var b strings.Builder
		for _, tok := range sortedTokens(p) {
			x, y := c.toNorm(p[tok].Pan, p[tok].Tilt)
			fmt.Fprintf(&b, `<tptz:Preset token="%s"><tt:Name>%s</tt:Name><tt:PTZPosition>%s</tt:PTZPosition></tptz:Preset>`,
				esc(tok), esc(p[tok].Name), vec(x, y, sp+"PanTiltSpaces/PositionGenericSpace"))
		}
		return `<tptz:GetPresetsResponse>` + b.String() + `</tptz:GetPresetsResponse>`, nil
	case "SetPreset":
		tok, err := c.setPreset(root.str("PresetToken"), root.str("PresetName"))
		if err != nil {
			return "", err
		}
		return `<tptz:SetPresetResponse><tptz:PresetToken>` + esc(tok) + `</tptz:PresetToken></tptz:SetPresetResponse>`, nil
	case "GotoPreset":
		if err := c.gotoPreset(root.str("PresetToken"), root.speed()); err != nil {
			return "", err
		}
		return `<tptz:GotoPresetResponse/>`, nil
	case "RemovePreset":
		if err := c.removePreset(root.str("PresetToken")); err != nil {
			return "", err
		}
		return `<tptz:RemovePresetResponse/>`, nil
	}
	return "", notSupported(op)
}
