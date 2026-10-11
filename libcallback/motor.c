#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

extern char CommandResBuf[];

extern int local_sdk_motor_get_position(float *step,float *angle);
extern int local_sdk_motor_move_abs_angle(float pan, float tilt, int speed, void (*done)(float a, float b), void (*canceled)(void), int mode);
extern int local_sdk_motor_move_rel_angle(float pan, float tilt, int speed, void (*done)(float a, float b), void (*canceled)(void), int mode);
extern int local_sdk_motor_move(int h, int v, int speed, void (*done)(float a, float b), void (*canceled)(void), int mode);
extern int local_sdk_motor_stop(void (*done)(float a, float b), void (*canceled)(void), int mode);
extern int IMP_ISP_Tuning_GetISPHflip(int *pmode);
extern int IMP_ISP_Tuning_GetISPVflip(int *pmode);
extern void CommandResponse(int fd, const char *res);
extern int swing;

int MotorFd = -1;
int LastPri = 3;
static int ContMoving = 0;
struct timeval MotorLastMovedTime = { 0, 0 };

static void motor_move_done(float pan, float tilt) {

  if(MotorFd >= 0) {
    int vflip, hflip;
    IMP_ISP_Tuning_GetISPHflip(&hflip);
    IMP_ISP_Tuning_GetISPVflip(&vflip);
    if(hflip) pan = 355.0 - pan;
    if(vflip) tilt = 180.0 - tilt;
    sprintf(CommandResBuf, "%f %f %d %d\n", pan, tilt, hflip, vflip);
    CommandResponse(MotorFd, CommandResBuf);
  }
  MotorFd = -1;
  struct timeval tv;
  gettimeofday(&MotorLastMovedTime, NULL);
}

// the vendor app reads the position back inside its own relative-move callback, so do the same
static void motor_relmove_done(float a, float b) {

  float pan, tilt;
  if(local_sdk_motor_get_position(&pan, &tilt)) {
    pan = a;
    tilt = b;
  }
  motor_move_done(pan, tilt);
}

static void motor_move_canceled() {

  if(MotorFd >= 0) CommandResponse(MotorFd, "error : canceled\n");
  MotorFd = -1;
  gettimeofday(&MotorLastMovedTime, NULL);
}

char *MotorMove(int fd, char *tokenPtr) {

  if(!swing) return "error";

  int vflip, hflip;
  IMP_ISP_Tuning_GetISPHflip(&hflip);
  IMP_ISP_Tuning_GetISPVflip(&vflip);

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    float pan; // 0-355
    float tilt; // 0-180
    int ret = local_sdk_motor_get_position(&pan, &tilt);
    if(ret) return "error";
    if(hflip) pan = 355.0 - pan;
    if(vflip) tilt = 180.0 - tilt;
    sprintf(CommandResBuf, "%f %f %d %d %d\n", pan, tilt, hflip, vflip, MotorFd >= 0 ? 0 : 1);
    return CommandResBuf;
  }
  float pan = atof(p); // 0-355
  if((pan < 0.0) || (pan > 355.0)) return "error";
  if(hflip) pan = 355.0 - pan;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  float tilt = atof(p); // 0-180
  if((tilt < 0.0) || (tilt > 180.0)) return "error";
  if(vflip) tilt = 180.0 - tilt;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  int speed = 9; // 1: low - 9: high speed
  if(p) speed = atoi(p);
  if(speed < 1) speed = 1;
  if(speed > 9) speed = 9;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  int pri = 2; // 0: high - 3: low
  if(p) pri = atoi(p);
  if(pri < 0) pri = 0;
  if(pri > 3) pri = 3;

  if(MotorFd >= 0) {
    if(pri < LastPri) {
      CommandResponse(MotorFd, "error : multiple request.");
    } else {
      return "error : dismiss request.";
    }
  }
  MotorFd = fd;
  LastPri = pri;
  int res = local_sdk_motor_move_abs_angle(pan, tilt, speed, &motor_move_done, &motor_move_canceled, pri);
  return NULL;
}

// relmove <dpan> <dtilt> [speed] [priority] : relative move in degrees, + = right / up as seen in the image
char *MotorRelMove(int fd, char *tokenPtr) {

  if(!swing) return "error";

  int vflip, hflip;
  IMP_ISP_Tuning_GetISPHflip(&hflip);
  IMP_ISP_Tuning_GetISPVflip(&vflip);

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  float pan = atof(p);
  if((pan < -355.0) || (pan > 355.0)) return "error";
  if(hflip) pan = -pan;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  float tilt = atof(p);
  if((tilt < -180.0) || (tilt > 180.0)) return "error";
  if(vflip) tilt = -tilt;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  int speed = 9; // 1: low - 9: high speed
  if(p) speed = atoi(p);
  if(speed < 1) speed = 1;
  if(speed > 9) speed = 9;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  int pri = 2; // 0: high - 3: low
  if(p) pri = atoi(p);
  if(pri < 0) pri = 0;
  if(pri > 3) pri = 3;

  if(MotorFd >= 0) {
    if(pri < LastPri) {
      CommandResponse(MotorFd, "error : multiple request.");
    } else {
      return "error : dismiss request.";
    }
  }
  MotorFd = fd;
  LastPri = pri;
  local_sdk_motor_move_rel_angle(pan, tilt, speed, &motor_relmove_done, &motor_move_canceled, pri);
  return NULL;
}

// continuous moves have no completion to report; just keep ptzstatus honest
static void motor_cont_done(float a, float b) {

  ContMoving = 0;
  gettimeofday(&MotorLastMovedTime, NULL);
}

static void motor_cont_canceled() {

  ContMoving = 0;
  gettimeofday(&MotorLastMovedTime, NULL);
}

// contmove <h> <v> [speed] [priority] : drive the head until contstop (or the end of travel). Returns at once.
// h and v are the SDK's raw direction commands, 0 = none, 1 / 2 = the two directions. Which one is which
// depends on the mounting, so onvif_ptz.sh measures it once per boot instead of guessing.
char *MotorContMove(int fd, char *tokenPtr) {

  if(!swing) return "error";

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int h = atoi(p);
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int v = atoi(p);
  if((h < 0) || (h > 2) || (v < 0) || (v > 2)) return "error";

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  int speed = 9; // 1: low - 9: high speed
  if(p) speed = atoi(p);
  if(speed < 1) speed = 1;
  if(speed > 9) speed = 9;

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  int pri = 2; // 0: high - 3: low
  if(p) pri = atoi(p);
  if(pri < 0) pri = 0;
  if(pri > 3) pri = 3;

  if((h == 0) && (v == 0)) {
    local_sdk_motor_stop(&motor_cont_done, &motor_cont_canceled, 1);
    ContMoving = 0;
    gettimeofday(&MotorLastMovedTime, NULL);
    return "ok";
  }
  if(MotorFd >= 0) {
    if(pri < LastPri) {
      CommandResponse(MotorFd, "error : multiple request.");
      MotorFd = -1;
    } else {
      return "error : dismiss request.";
    }
  }
  ContMoving = 1;
  gettimeofday(&MotorLastMovedTime, NULL);
  local_sdk_motor_move(h, v, speed, &motor_cont_done, &motor_cont_canceled, pri);
  return "ok";
}

char *MotorContStop(int fd, char *tokenPtr) {

  if(!swing) return "error";
  local_sdk_motor_stop(&motor_cont_done, &motor_cont_canceled, 1);
  ContMoving = 0;
  gettimeofday(&MotorLastMovedTime, NULL);
  return "ok";
}

// ptzstatus : "<pan>,<tilt>,0,<moving>" in one line (rounded, flip corrected like move), meant for the ONVIF
// GetStatus hook so it costs one socket round trip. moving covers every mover: move, relmove, contmove.
char *MotorPtzStatus(int fd, char *tokenPtr) {

  if(!swing) return "error";

  int vflip, hflip;
  IMP_ISP_Tuning_GetISPHflip(&hflip);
  IMP_ISP_Tuning_GetISPVflip(&vflip);
  float pan; // 0-355
  float tilt; // 0-180
  if(local_sdk_motor_get_position(&pan, &tilt)) return "error";
  if(hflip) pan = 355.0 - pan;
  if(vflip) tilt = 180.0 - tilt;
  sprintf(CommandResBuf, "%d,%d,0,%d\n", (int)(pan + 0.5), (int)(tilt + 0.5), (MotorFd >= 0 || ContMoving) ? 1 : 0);
  return CommandResBuf;
}
