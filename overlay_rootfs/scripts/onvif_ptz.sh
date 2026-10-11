#!/bin/sh

# PTZ glue for onvif_simple_server (see onvif.sh for the command wiring).
# Moves go through the cam's command socket: "move" is absolute, "relmove" is
# a native relative step move in degrees (+ = right / up in the image).

MOVING_PREFIX=/tmp/onvif_ptz_moving
SPEED=5     # 1-9
PRI=2       # 0 (cancels everything) - 3

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
  pos=$(/scripts/cmd move 2> /dev/null)
  set -- $pos
  [ "$2" != "" ] && /scripts/cmd move "$1" "$2" 9 0 > /dev/null 2>&1
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
esac
