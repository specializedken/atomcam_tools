# onvif

Small, dependency-free Go daemon that exposes an ONVIF Device/Media/PTZ service for AtomSwing cams
and translates it into the command socket on `localhost:4000` (`move <pan> <tilt> <speed> <pri>`).

Started by `/etc/init.d/S76onvif` -> `/scripts/onvif.sh` only when `ONVIF_ENABLE=on` in hack.ini
(default off). Supported: GetCapabilities/Services/DeviceInformation/SystemDateAndTime, Media profile +
stream URI, PTZ Absolute/Relative(+`TranslationSpaceFov`)/Continuous move, Stop, GetStatus, presets.
Not supported: zoom, ONVIF home position, EFlip, authentication.

    cd src && go vet ./... && go test -race ./...     # tests use a fake command socket, no cam needed
