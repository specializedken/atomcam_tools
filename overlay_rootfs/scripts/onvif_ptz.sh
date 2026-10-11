#!/bin/sh

# PTZ glue for onvif_simple_server (see onvif.sh for the command wiring).
# Moves go through the cam's command socket: "move" is absolute, "relmove" is
# a native relative step move in degrees (+ = right / up in the image).

MOVING_PREFIX=/tmp/onvif_ptz_moving
SPEED=5     # 1-9
PRI=2       # 0 (cancels everything) - 3

# one "<number>=<name> <pan> <tilt> 0" per line, which is what the server's get_presets expects
PRESET_FILE=/media/mmc/onvif_presets.txt
MAX_PRESETS=32
HOME_NAME=home

current_pos()
{
  /scripts/cmd move 2> /dev/null | awk '
    NF >= 2 {
      printf("%d,%d,0\n", int($1 + 0.5), int($2 + 0.5));
      ok = 1;
      exit;
    }
    END {
      if(!ok) print "0,0,0";
    }'
}

raw_pos()
{
  /scripts/cmd move 2> /dev/null | awk 'NF >= 2 { print $1, $2; exit }'
}

# one flag file per running move, so a cancelled move can't clear the flag of the move that replaced it
run_move()
{
  touch $MOVING_PREFIX.$$
  trap "rm -f $MOVING_PREFIX.$$" EXIT
  /scripts/cmd "$@" > /dev/null 2>&1
}

move_abs()
{
  pan=$1
  tilt=$2
  [ "$pan" = "" ] && pan=0
  [ "$tilt" = "" ] && tilt=0
  run_move move "$pan" "$tilt" $SPEED $PRI
}

move_rel()
{
  dpan=$1
  dtilt=$2
  [ "$dpan" = "" ] && dpan=0
  [ "$dtilt" = "" ] && dtilt=0
  run_move relmove "$dpan" "$dtilt" $SPEED $PRI
}

# ContinuousMove has no native equivalent: nudge a fixed 30 degrees per unit of velocity, a client Stop ends it early
step()
{
  value=$(awk -v v="$1" 'BEGIN { d = int((v + 0.05) * 30); if(d < 1) d = 1; print d }')
  case "$2" in
    left)  move_rel -$value 0 ;;
    right) move_rel $value 0 ;;
    up)    move_rel 0 $value ;;
    down)  move_rel 0 -$value ;;
  esac
}

stop_move()
{
  ls $MOVING_PREFIX.* > /dev/null 2>&1 || return
  # a priority 0 move cancels the one in flight; aim it at where we are so the head settles there
  set -- $(raw_pos)
  [ "$2" != "" ] && /scripts/cmd move "$1" "$2" 9 0 > /dev/null 2>&1
}

# the preset called "home" doubles as the ONVIF home position (Frigate returns to it after tracking)
preset_num_by_name()
{
  awk -F '[= ]' -v n="$1" '$2 == n { print $1; exit }' $PRESET_FILE 2> /dev/null
}

# $1 = preset number (< 1 = new), $2 = name
save_preset()
{
  num=$1
  name=$2
  [ "$name" = "" ] && return
  set -- $(raw_pos)
  [ "$2" = "" ] && return
  pan=$1
  tilt=$2
  touch $PRESET_FILE
  if ! [ "$num" -ge 1 ] 2> /dev/null; then
    [ `wc -l < $PRESET_FILE` -ge $MAX_PRESETS ] && return
    num=$(awk -F= 'BEGIN { n = 0 } $1 + 0 > n { n = $1 + 0 } END { print n + 1 }' $PRESET_FILE)
  fi
  { grep -v "^$num=" $PRESET_FILE; echo "$num=$name $pan $tilt 0"; } > $PRESET_FILE.tmp
  mv -f $PRESET_FILE.tmp $PRESET_FILE
}

goto_preset()
{
  set -- $(awk -F '[= ]' -v n="$1" '$1 == n { print $3, $4; exit }' $PRESET_FILE 2> /dev/null)
  [ "$2" != "" ] && move_abs "$1" "$2"
}

remove_preset()
{
  [ -f $PRESET_FILE ] || return
  grep -v "^$1=" $PRESET_FILE > $PRESET_FILE.tmp
  mv -f $PRESET_FILE.tmp $PRESET_FILE
}

case "$1" in
  get_position)
    current_pos
    ;;
  is_moving)
    ls $MOVING_PREFIX.* > /dev/null 2>&1 && echo 1 || echo 0
    ;;
  move_left)
    step "$2" left
    ;;
  move_right)
    step "$2" right
    ;;
  move_up)
    step "$2" up
    ;;
  move_down)
    step "$2" down
    ;;
  absolute)
    move_abs "$2" "$3"
    ;;
  relative)
    move_rel "$2" "$3"
    ;;
  stop)
    stop_move
    ;;
  get_presets)
    cat $PRESET_FILE 2> /dev/null
    ;;
  set_preset)
    save_preset "$2" "$3"
    ;;
  set_home)
    save_preset "$(preset_num_by_name $HOME_NAME)" $HOME_NAME
    ;;
  remove_preset)
    remove_preset "$2"
    ;;
  goto_preset)
    goto_preset "$2"
    ;;
  goto_home)
    goto_preset "$(preset_num_by_name $HOME_NAME)"
    ;;
esac
