#!/bin/sh
#
# ONVIF PTZ service (AtomSwing only). Does nothing unless ONVIF_ENABLE=on in hack.ini.
# Every failure path exits 0: this must never get in the way of booting.
#

HACK_INI=/tmp/hack.ini

if [ "$1" = "off" ]; then
  while pidof onvif > /dev/null ; do
    killall onvif > /dev/null 2>&1
    sleep 0.5
  done
  exit 0
fi

pidof onvif > /dev/null && exit 0
[ -x /usr/bin/onvif ] || exit 0
[ -f $HACK_INI ] || exit 0

ini() {
  awk -F "=" -v k="$1" '$1 == k {print $2}' $HACK_INI
}

[ "$(ini ONVIF_ENABLE)" = "on" ] || exit 0

PORT=$(ini ONVIF_PORT)
MAX_SPEED=$(ini ONVIF_MAX_SPEED)
case "$PORT" in ''|*[!0-9]*) PORT=8000 ;; esac
case "$MAX_SPEED" in [1-9]) ;; *) MAX_SPEED=9 ;; esac

# wait (up to ~60s) for the command socket; "move" only answers on AtomSwing
n=0
while [ $n -lt 30 ] ; do
  res=$(/scripts/cmd move 2> /dev/null)
  [ -n "$res" ] && break
  n=$((n + 1))
  sleep 2
done
case "$res" in ''|error*) exit 0 ;; esac

echo `date +"%Y/%m/%d %H:%M:%S"` ": onvif start (port $PORT, max speed $MAX_SPEED)"
exec /usr/bin/onvif -listen ":$PORT" -max-speed "$MAX_SPEED" -presets /media/mmc/onvif_presets.json
