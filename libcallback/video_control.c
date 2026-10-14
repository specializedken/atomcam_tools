#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <pthread.h>

extern char CommandResBuf[];

// contrast of image effect.
extern int IMP_ISP_Tuning_SetContrast(unsigned char contrast);
extern int IMP_ISP_Tuning_GetContrast(unsigned char *pcontrast);

// brightness of image effect.
extern int IMP_ISP_Tuning_SetBrightness(unsigned char bright);
extern int IMP_ISP_Tuning_GetBrightness(unsigned char *pbright);

// saturation of image effect.
extern int IMP_ISP_Tuning_SetSaturation(unsigned char sat);
extern int IMP_ISP_Tuning_GetSaturation(unsigned char *psat);

// sharpness of image effect.
extern int IMP_ISP_Tuning_SetSharpness(unsigned char sharpness);
extern int IMP_ISP_Tuning_GetSharpness(unsigned char *psharpness);

// AE compensation.
// AE compensation parameters can adjust the target of the image AE.
extern int IMP_ISP_Tuning_SetAeComp(int comp);
extern int IMP_ISP_Tuning_GetAeComp(int *comp);

// AE Max parameters.
extern int IMP_ISP_Tuning_SetAe_IT_MAX(unsigned int it_max);
extern int IMP_ISP_Tuning_GetAE_IT_MAX(unsigned int *it_max);

// DPC Strength.
extern int IMP_ISP_Tuning_SetDPC_Strength(unsigned int ratio);
extern int IMP_ISP_Tuning_GetDPC_Strength(unsigned int *ratio);

// DRC Strength.
extern int IMP_ISP_Tuning_SetDRC_Strength(unsigned int ratio);
extern int IMP_ISP_Tuning_GetDRC_Strength(unsigned int *ratio);

// highlight intensity controls.
extern int IMP_ISP_Tuning_SetHiLightDepress(uint32_t strength);
extern int IMP_ISP_Tuning_GetHiLightDepress(uint32_t *strength);

// Set 3D noise reduction intensity.
extern int IMP_ISP_Tuning_SetTemperStrength(uint32_t ratio);

// Set 2D noise reduction intensity.
extern int IMP_ISP_Tuning_SetSinterStrength(uint32_t ratio);

// Max value of sensor analog gain.
extern int IMP_ISP_Tuning_SetMaxAgain(uint32_t gain);
extern int IMP_ISP_Tuning_GetMaxAgain(uint32_t *gain);

// Max value of sensor Digital gain.
extern int IMP_ISP_Tuning_SetMaxDgain(uint32_t gain);
extern int IMP_ISP_Tuning_GetMaxDgain(uint32_t *gain);

// ISP image mirror(horizontal) effect function (enable/disable)
extern int IMP_ISP_Tuning_SetISPHflip(int mode);
extern int IMP_ISP_Tuning_GetISPHflip(int *pmode);

// ISP image mirror(vertical) effect function (enable/disable)
extern int IMP_ISP_Tuning_SetISPVflip(int mode);
extern int IMP_ISP_Tuning_GetISPVflip(int *pmode);

// ISP exposure attribute.
typedef union isp_core_expr_attr{
  struct {
    int mode; // 0:auto / 1: manual
    int unit; // 0: line / 1: us
    unsigned short time;
  } s_attr;
  struct {
    int mode; // 0:auto / 1: manual
    unsigned short time;
    unsigned short time_min;
    unsigned short time_max;
    unsigned short one_line_expr_in_us;
  } g_attr;
}IMPISPExpr;
extern int IMP_ISP_Tuning_SetExpr(IMPISPExpr *expr);
extern int IMP_ISP_Tuning_GetExpr(IMPISPExpr *expr);

typedef struct {
  unsigned int min_it;
  unsigned int min_again;
  unsigned int min_it_short;
  unsigned int min_again_short;
} IMPISPAEMin;
int IMP_ISP_Tuning_SetAeMin(IMPISPAEMin *ae_min);
int IMP_ISP_Tuning_GetAeMin(IMPISPAEMin *ae_min);

// ISP sensor fps
extern int IMP_ISP_Tuning_GetSensorFPS(unsigned int *num, unsigned int *den);
struct IMPEncoderFrmRate {
  unsigned int frmRateNum;
  unsigned int frmRateDen;
};
extern int IMP_Encoder_GetChnFrmRate(int ch, struct IMPEncoderFrmRate *frm);
extern int IMP_Encoder_SetChnFrmRate(int ch, struct IMPEncoderFrmRate *frm);
struct IMPEncoderGopAttr {
  int uGopCtrlMode;
  unsigned short uGopLength;
  unsigned char uNumB;
  unsigned int uMaxSameSenceCnt;
  int bEnableLT;
  unsigned int uFreqLT;
  int bLTRC;
};
extern int IMP_Encoder_SetChnGopAttr(int ch,struct IMPEncoderGopAttr *gopAttr);
extern int local_sdk_video_set_gop(int ch, int gop);
// read-only encoder state; the structs are not in any header we have, so they are only dumped as words
extern int IMP_Encoder_GetChnAttr(int ch, void *attr);
extern int IMP_Encoder_GetChnAttrRcMode(int ch, void *rc);
extern int IMP_Encoder_GetChnGopAttr(int ch, void *gop);
extern int IMP_Encoder_SetChnAttrRcMode(int ch, void *rc);
extern int IMP_Encoder_SetStreamBufSize(int ch, unsigned int size);
extern int IMP_Encoder_GetStreamBufSize(int ch, unsigned int *size);
extern int IMP_Encoder_SetMaxStreamCnt(int ch, int cnt);
extern int IMP_Encoder_GetMaxStreamCnt(int ch, int *cnt);
extern int video_stats_text(char *buf, size_t size);
extern void video_stats_reset(void);
extern int local_sdk_video_stop(int ch, int p1, int p2);
extern int local_sdk_video_start(int ch);

extern char *VideoCapture(int fd, char *p, char *tokenPtr);

static char *Flip(char *tokenPtr);
static char *Contrast(char *tokenPtr);
static char *Brightness(char *tokenPtr);
static char *Saturation(char *tokenPtr);
static char *Sharpness(char *tokenPtr);
static char *Sinter(char *tokenPtr);
static char *Temper(char *tokenPtr);
static char *AEComp(char *tokenPtr);
static char *AEItMax(char *tokenPtr);
static char *AEMin(char *tokenPtr);
static char *DPC(char *tokenPtr);
static char *DRC(char *tokenPtr);
static char *HiLight(char *tokenPtr);
static char *AGain(char *tokenPtr);
static char *DGain(char *tokenPtr);
static char *Expr(char *tokenPtr);
static char *Bitrate(char *tokenPtr);
static char *Framerate(char *tokenPtr);
static char *EncDump(char *tokenPtr);
static char *EncRc(char *tokenPtr);
static char *EncGop(char *tokenPtr);
static char *Iqp(char *tokenPtr);
static char *Rc(char *tokenPtr);
static char *StreamBuf(char *tokenPtr);
static char *StreamCnt(char *tokenPtr);
static char *Stats(char *tokenPtr);
static char *Gop(char *tokenPtr);

struct CommandTableSt {
  const char *cmd;
  char * (*func)(char *);
};
static struct CommandTableSt VideoCommandTable[] = {
  { "flip",      &Flip }, // flip [normal/flip/mirror/flip_mirror]
  { "cont",      &Contrast }, // cont 0 - 255(center:128)
  { "bri",       &Brightness }, // bri 0 - 255(center:128)
  { "sat",       &Saturation }, // sat 0 - 255(center:128)
  { "sharp",     &Sharpness }, // sharp 0 - 255(center:128)
  { "sinter",    &Sinter }, // sinter 0 - 255(center:128)
  { "temper",    &Temper }, // temper 0 - 255(center:128)
  { "aecomp",    &AEComp }, // aecomp 0 - 255
  { "aeitmax",   &AEItMax }, // aeitmax 0-
  { "aemin",     &AEMin }, // it again itshort againshort
  { "dpc",       &DPC }, // dpc 0 - 255
  { "drc",       &DRC }, // drc 0 - 255
  { "hilight",   &HiLight }, // hilight 0 - 10
  { "again",     &AGain }, // again 0 -
  { "dgain",     &DGain }, // dgain 0 -
  { "expr",      &Expr }, // expr manual|auto <time>
  { "bitrate",   &Bitrate }, // bitrate <ch> 10-3000(kbps)|auto
  { "fps",       &Framerate }, // fps <ch> 1-30(fps)|auto
  { "encdump",   &EncDump }, // encdump <ch 0-3> attr|rc|gop [<first word 0-255>]  (16 words per call)
  { "encrc",     &EncRc }, // encrc <ch> [<word 0-8> <value> ...]   read / change the rate control struct (word 0 = mode)
  { "encgop",    &EncGop }, // encgop <ch> [<gop length>]
  { "streambuf", &StreamBuf }, // streambuf <imp ch 0-2> [<kB>|auto]  encoder stream buffer, applied when the channel is (re)created
  { "streamcnt", &StreamCnt }, // streamcnt <imp ch 0-2> [<n>|auto]
  { "stats",     &Stats }, // stats [reset]   frames / largest frame / callback time / stalls per channel
  { "iqp",       &Iqp }, // iqp <ch 0|1|3> [<min QP 10-51>|auto]   (the floor of every frame; see "rc struct" in tools/encbench/README.md)
  { "gop",       &Gop }, // gop <ch 0|1|3> [<keyframe interval in seconds 1-10>|auto]
  { "rc",        &Rc }, // rc <ch 0|1|3> [<field> [<value>|auto]]   initqp maxqp ipdelta pbdelta options maxpic maxpsnr
};

static int (*real_local_sdk_video_set_kbps)(int ch, int kbps);
static int (*real_local_sdk_video_set_fps)(int fps);
static int (*real_local_sdk_video_set_gop)(int ch, int gop);
static int (*real_IMP_Encoder_CreateChn)(int ch, unsigned char *encAttr);

int VideoControl_UserFps = 0;
int VideoControl_AppFps = 25;

static int userBitrate[4] = { 0, 0, 0, 0 };
static int appBitrate[4] = { 960, 180, 0, 800 };

// User overrides of the minimum QP ("iqp", QP value) and the keyframe interval ("gop", whole SECONDS), indexed by the
// local SDK channel (0, 1, 3). 0 = leave what the app / SDK sets. They are re-applied whenever the app (re)creates a
// channel or sets a bitrate / frame rate (those rewrite the encoder's parameters) and by a slow keeper thread.
// The interval is kept in seconds because the encoder only produces keyframes at whole seconds (measured: lengths that
// are not a multiple of the frame rate read back +fps and run at ceil(len / fps) + 1 s), and the frame rate flips by
// itself between 15 and 20 with the daylight, so the frame count is derived from the live frame rate each time.
static int userIqp[4] = { 0, 0, 0, 0 };
static int userGop[4] = { 0, 0, 0, 0 };

// Encoder stream buffer (bytes the encoder may hold for the consumer) and number of buffered streams. libimp only
// accepts them before IMP_Encoder_CreateChn, so they are applied from the CreateChn hook; 0 = libimp's default.
// Set per libimp channel (0..2) with "video streambuf|streamcnt", or by the hack.ini keys STREAMBUF_<name> (kB) and
// STREAMCNT_<name>, which are read at channel creation because the boot scripts run after the channels exist.
static int userStreamBufKb[3] = { 0, 0, 0 };
static int userStreamCnt[3] = { 0, 0, 0 };

static int ini_int(const char *key) {

  const char *files[2] = { "/tmp/hack.ini", "/media/mmc/hack.ini" };
  for(int i = 0; i < 2; i++) {
    FILE *f = fopen(files[i], "r");
    if(!f) continue;
    char line[256];
    size_t kl = strlen(key);
    int val = 0;
    while(fgets(line, sizeof(line), f)) {
      if(!strncmp(line, key, kl) && (line[kl] == '=')) val = atoi(line + kl + 1);
    }
    fclose(f);
    return val;
  }
  return 0;
}

// Like ini_int, but tells whether the key is present (and non-empty) so that 0 and -1 can be real values.
static int ini_has(const char *key, int *out) {

  const char *files[2] = { "/tmp/hack.ini", "/media/mmc/hack.ini" };
  for(int i = 0; i < 2; i++) {
    FILE *f = fopen(files[i], "r");
    if(!f) continue;
    char line[256];
    size_t kl = strlen(key);
    int found = 0;
    while(fgets(line, sizeof(line), f)) {
      if(!strncmp(line, key, kl) && (line[kl] == '=') && (line[kl + 1] != '\n') && (line[kl + 1] != '\r') && line[kl + 1]) {
        *out = atoi(line + kl + 1);
        found = 1;
      }
    }
    fclose(f);
    return found;
  }
  return 0;
}

// local SDK channel -> libimp encoder channel (the main HEVC stream, SDK ch 3, is libimp ch 2)
static int imp_channel(int ch) {
  return (ch == 3) ? 2 : ch;
}

static int sdk_channel(int impCh) {
  return (impCh == 2) ? 3 : impCh;
}

// The remaining fields of the rate control struct. In the capped modes (4 and 8; the app uses 8) libimp's
// IMP_Encoder_SetChnAttrRcMode copies the 9 words into the encoder's own parameters (decoded from libimp.so, see
// tools/encbench/README.md "rc struct"): w1/w2 target/max kbps, w3 low = initial QP (-1 auto), w3 high = MIN QP (the
// what "video iqp" sets), w4 low = max QP, w4 high = I/P QP delta (-1 auto), w5 low = P/B QP delta (-1 auto),
// w6 = rate control options, w7 = largest picture in kbit (x1000 bits), w8 low = max PSNR in dB (x100).
// Measured: max picture, max PSNR and IP delta have NO effect when changed on a running channel, so they are applied
// from the CreateChn hook (before the encoder exists) and read from hack.ini there.
struct RcField {
  const char *name;
  const char *key;    // hack.ini key prefix, followed by _MAIN_AVC / _SUB_HEVC / _MAIN_HEVC; negative = off
  int word, shift;
  unsigned int mask;  // after the shift
  int isSigned;
  int lo, hi;
};
static const struct RcField RcFields[] = {
  { "initqp",  "INITQP",  3, 0,  0xffff,     1, -1, 51 },
  { "maxqp",   "MAXQP",   4, 0,  0xffff,     1, 10, 51 },
  { "ipdelta", "IPDELTA", 4, 16, 0xffff,     1, -1, 51 },
  { "pbdelta", "PBDELTA", 5, 0,  0xffff,     1, -1, 51 },
  { "options", "RCOPT",   6, 0,  0xffffffff, 0, 0, 255 },
  { "maxpic",  "MAXPIC",  7, 0,  0xffffffff, 0, 50, 8000 },
  { "maxpsnr", "MAXPSNR", 8, 0,  0xffff,     0, 20, 60 },
};
#define RC_FIELDS (sizeof(RcFields) / sizeof(RcFields[0]))
// Overrides per libimp channel (0..2): set by "video rc" or, at channel creation, by hack.ini.
static int userRcSet[3][RC_FIELDS];
static int userRcVal[3][RC_FIELDS];

static int rc_get(const unsigned int *rc, const struct RcField *f) {

  unsigned int v = (rc[f->word] >> f->shift) & f->mask;
  if(f->isSigned && (f->mask == 0xffff)) return (short)v;
  return (int)v;
}

static void rc_put(unsigned int *rc, const struct RcField *f, int val) {

  rc[f->word] = (rc[f->word] & ~(f->mask << f->shift)) | (((unsigned int)val & f->mask) << f->shift);
}

// Applies the user's overrides of one channel to a rate control struct (9 words). Returns how many were applied.
static int rc_apply_words(int impCh, unsigned int *rc) {

  if((impCh < 0) || (impCh > 2)) return 0;
  if((rc[0] != 4) && (rc[0] != 8)) return 0;
  int n = 0;
  for(int i = 0; i < RC_FIELDS; i++) {
    if(!userRcSet[impCh][i]) continue;
    rc_put(rc, &RcFields[i], userRcVal[impCh][i]);
    n++;
  }
  return n;
}

// Same on a running channel (read-modify-write); the encoder may ignore some fields until it is created again.
static void apply_rc(int ch) {

  int impCh = imp_channel(ch);
  if((impCh < 0) || (impCh > 2)) return;
  unsigned int rc[16];
  memset(rc, 0, sizeof(rc));
  if(IMP_Encoder_GetChnAttrRcMode(impCh, rc)) return;
  if(!rc_apply_words(impCh, rc)) return;
  int ret = IMP_Encoder_SetChnAttrRcMode(impCh, rc);
  fprintf(stderr, "video rc ch%d applied (%d)\n", ch, ret);
}

// "iqp" is the high 16 bits of word 3 of the rate control struct. libimp hands it to the encoder as the MIN QP of the
// capped modes (the floor of every frame, I and P; see "rc struct" in tools/encbench/README.md). It is NOT a keyframe QP.
// Frame rate the encoder channel is running at (rounded), the app's last value if it cannot be read.
static int encoder_fps(int impCh) {

  struct IMPEncoderFrmRate frm;
  memset(&frm, 0, sizeof(frm));
  if(!IMP_Encoder_GetChnFrmRate(impCh, &frm) && (frm.frmRateNum > 0) && (frm.frmRateDen > 0)) {
    return (frm.frmRateNum + frm.frmRateDen / 2) / frm.frmRateDen;
  }
  return (VideoControl_AppFps > 0) ? VideoControl_AppFps : 15;
}

static int gop_frames(int ch) {

  int n = userGop[ch] * encoder_fps(imp_channel(ch));
  return (n > 300) ? 300 : n;
}

// Both appliers only write when the encoder differs, so they are cheap enough for the keeper thread.
static void apply_iqp(int ch) {

  if(!userIqp[ch]) return;
  unsigned int rc[16];
  memset(rc, 0, sizeof(rc));
  int impCh = imp_channel(ch);
  if(IMP_Encoder_GetChnAttrRcMode(impCh, rc)) return;
  if((rc[3] >> 16) == (unsigned int)userIqp[ch]) return;
  rc[3] = ((unsigned int)userIqp[ch] << 16) | (rc[3] & 0xffff);
  int ret = IMP_Encoder_SetChnAttrRcMode(impCh, rc);
  fprintf(stderr, "video iqp ch%d: %d (%d)\n", ch, userIqp[ch], ret);
}

static void apply_gop(int ch) {

  if(!userGop[ch]) return;
  unsigned char buf[64];
  memset(buf, 0, sizeof(buf));
  struct IMPEncoderGopAttr *g = (struct IMPEncoderGopAttr *)buf;
  int impCh = imp_channel(ch);
  if(IMP_Encoder_GetChnGopAttr(impCh, g)) return;
  int frames = gop_frames(ch);
  if(g->uGopLength == frames) return;
  g->uGopLength = frames;
  int ret = IMP_Encoder_SetChnGopAttr(impCh, g);
  fprintf(stderr, "video gop ch%d: %d s = %d frames (%d)\n", ch, userGop[ch], frames, ret);
}

// The app (re)configures the encoders at its own pace (boot, daylight frame rate flips, network changes) and some of it
// is not observable from here, so the settings are also checked every few seconds: the cam always ends up in the state
// hack.ini / the UI describes.
static void *keeper_thread(void *arg) {

  static const int sdkChannels[3] = { 0, 1, 3 };
  for(;;) {
    sleep(5);
    for(int i = 0; i < 3; i++) {
      apply_iqp(sdkChannels[i]);
      apply_gop(sdkChannels[i]);
    }
  }
  return NULL;
}

static void __attribute ((constructor)) video_control_init(void) {

  real_local_sdk_video_set_kbps = dlsym(dlopen ("/system/lib/liblocalsdk.so", RTLD_LAZY), "local_sdk_video_set_kbps");
  real_local_sdk_video_set_fps = dlsym(dlopen ("/system/lib/liblocalsdk.so", RTLD_LAZY), "local_sdk_video_set_fps");
  real_local_sdk_video_set_gop = dlsym(dlopen ("/system/lib/liblocalsdk.so", RTLD_LAZY), "local_sdk_video_set_gop");
  real_IMP_Encoder_CreateChn = dlsym(dlopen ("/system/lib/libimp.so", RTLD_LAZY), "IMP_Encoder_CreateChn");
}

static void start_keeper(void) {

  static int started = 0;
  if(started) return;
  started = 1;
  pthread_t th;
  if(!pthread_create(&th, NULL, keeper_thread, NULL)) pthread_detach(th);
}

char *VideoCommand(int fd, char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(p) {
    for(int i = 0; i < sizeof(VideoCommandTable) / sizeof(struct CommandTableSt); i++) {
      if(!strcasecmp(p, VideoCommandTable[i].cmd)) return (*VideoCommandTable[i].func)(tokenPtr);
    }
  }
  return VideoCapture(fd, p, tokenPtr);
}

int local_sdk_video_set_kbps(int ch, int kbps) {

  if((ch == 0) || (ch == 1) || (ch == 3)) {
    appBitrate[ch] = kbps;
    if(userBitrate[ch]) {
      kbps = userBitrate[ch];
      fprintf(stderr, "video_set_kbps ch%d: %d -> %d\n", ch, appBitrate[ch], kbps);
    } else {
      fprintf(stderr, "video_set_kbps ch%d: %d\n", ch, kbps);
    }
  }
  int ret = real_local_sdk_video_set_kbps(ch, kbps);
  if((ch == 0) || (ch == 1) || (ch == 3)) {
    apply_iqp(ch);
    apply_rc(ch);
    apply_gop(ch);
  }
  return ret;
}

// The app sets the keyframe interval itself (about 2.5 x the frame rate) after the channel is created.
int local_sdk_video_set_gop(int ch, int gop) {

  if(((ch == 0) || (ch == 1) || (ch == 3)) && userGop[ch]) {
    int frames = gop_frames(ch);
    fprintf(stderr, "video_set_gop ch%d: %d -> %d\n", ch, gop, frames);
    gop = frames;
  }
  return real_local_sdk_video_set_gop(ch, gop);
}

int local_sdk_video_set_fps(int fps) {

  VideoControl_AppFps = fps;
  if(VideoControl_UserFps) {
    fps = VideoControl_UserFps;
    fprintf(stderr, "video_set_fps : %d -> %d\n", VideoControl_AppFps, fps);
  } else {
    fprintf(stderr, "video_set_fps : %d\n", fps);
  }
  int ret = real_local_sdk_video_set_fps(fps);
  // the app derives its keyframe interval (2.5 x fps) from the frame rate in here: put the user's values back
  static const int sdkChannels[3] = { 0, 1, 3 };
  for(int i = 0; i < 3; i++) {
    apply_gop(sdkChannels[i]);
    apply_iqp(sdkChannels[i]);
    apply_rc(sdkChannels[i]);
  }
  return ret;
}

int IMP_Encoder_CreateChn(int ch, unsigned char *attr) {

  static const char *bufKeys[3] = { "STREAMBUF_MAIN_AVC", "STREAMBUF_SUB_HEVC", "STREAMBUF_MAIN_HEVC" };
  static const char *cntKeys[3] = { "STREAMCNT_MAIN_AVC", "STREAMCNT_SUB_HEVC", "STREAMCNT_MAIN_HEVC" };
  int streamApplied = 0;
  if((ch >= 0) && (ch <= 2)) {
    int kb = userStreamBufKb[ch] ? userStreamBufKb[ch] : ini_int(bufKeys[ch]);
    int cnt = userStreamCnt[ch] ? userStreamCnt[ch] : ini_int(cntKeys[ch]);
    if(kb > 0) {
      int r = IMP_Encoder_SetStreamBufSize(ch, kb * 1024);
      fprintf(stderr, "video streambuf ch%d: %d kB (%d)\n", ch, kb, r);
      streamApplied = 1;
    }
    if(cnt > 0) {
      int r = IMP_Encoder_SetMaxStreamCnt(ch, cnt);
      fprintf(stderr, "video streamcnt ch%d: %d (%d)\n", ch, cnt, r);
      streamApplied = 1;
    }
  }

  // rate control fields that only take effect while the encoder is created: hack.ini <KEY>_<MAIN_AVC|SUB_HEVC|MAIN_HEVC>
  // (the user's "video rc" values win). The rate control struct is at byte 44 of the channel attr.
  if((ch >= 0) && (ch <= 2)) {
    static const char *suffix[3] = { "_MAIN_AVC", "_SUB_HEVC", "_MAIN_HEVC" };
    for(int i = 0; i < RC_FIELDS; i++) {
      if(userRcSet[ch][i]) continue;
      char key[48];
      snprintf(key, sizeof(key), "%s%s", RcFields[i].key, suffix[ch]);
      int v;
      if(ini_has(key, &v) && (v >= RcFields[i].lo) && (v <= RcFields[i].hi) && ((v >= 0) || (RcFields[i].lo < 0))) {
        userRcSet[ch][i] = 1;
        userRcVal[ch][i] = v;
      }
    }
    unsigned int *rcAttr = (unsigned int *)(attr + 44);
    int n = rc_apply_words(ch, rcAttr);
    if(n) fprintf(stderr, "video rc ch%d: %d field(s) set in CreateChn (mode %u)\n", ch, n, rcAttr[0]);
  }

  // minimum QP and keyframe interval from hack.ini, so the encoder starts in the configured state whatever order the
  // boot scripts run in (a value set with "video iqp|gop" wins)
  if((ch >= 0) && (ch <= 2)) {
    static const char *iqpKeys[3] = { "IQP_MAIN_AVC", "IQP_SUB_HEVC", "IQP_MAIN_HEVC" };
    static const char *gopKeys[3] = { "GOP_SEC_MAIN_AVC", "GOP_SEC_SUB_HEVC", "GOP_SEC_MAIN_HEVC" };
    int sc = sdk_channel(ch);
    int v;
    if(!userIqp[sc] && ini_has(iqpKeys[ch], &v) && (v >= 10) && (v <= 51)) userIqp[sc] = v;
    if(!userGop[sc] && ini_has(gopKeys[ch], &v) && (v >= 1) && (v <= 10)) userGop[sc] = v;
    start_keeper();
  }

  struct IMPEncoderGopAttr *gopAttr = (struct IMPEncoderGopAttr *)(attr + 88);
  struct IMPEncoderFrmRate *frm = (struct IMPEncoderFrmRate *)(attr + 80);
  int fps = frm->frmRateNum;
  frm->frmRateNum = 30;
  gopAttr->uGopLength = 30;
  int ret = real_IMP_Encoder_CreateChn(ch, attr);
  if(ret && streamApplied) {
    // never lose the video over an unusable stream buffer value: go back to libimp's defaults
    fprintf(stderr, "video stream buffer rejected (CreateChn ch%d: %d), using the defaults\n", ch, ret);
    IMP_Encoder_SetStreamBufSize(ch, 0);
    IMP_Encoder_SetMaxStreamCnt(ch, 0);
    ret = real_IMP_Encoder_CreateChn(ch, attr);
  }

  frm->frmRateNum = fps;
  gopAttr->uGopLength = fps;
  VideoControl_AppFps = fps;
  IMP_Encoder_SetChnGopAttr(ch, gopAttr);
  IMP_Encoder_SetChnFrmRate(ch, frm);
  if((ch >= 0) && (ch <= 2)) {
    apply_gop(sdk_channel(ch));
    apply_iqp(sdk_channel(ch));
    apply_rc(sdk_channel(ch));
  }
  return ret;
}

static char *Flip(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    int vflip, hflip;
    IMP_ISP_Tuning_GetISPHflip(&hflip);
    IMP_ISP_Tuning_GetISPVflip(&vflip);
    if(!hflip && !vflip) return "normal";
    if(hflip && !vflip) return "flip";
    if(!hflip && vflip) return "mirror";
    return "flip_mirror";
  }

  if(!strcasecmp(p, "normal")) {
    IMP_ISP_Tuning_SetISPVflip(0);
    IMP_ISP_Tuning_SetISPHflip(0);
  } else if(!strcasecmp(p, "flip")) {
    IMP_ISP_Tuning_SetISPVflip(1);
    IMP_ISP_Tuning_SetISPHflip(0);
  } else if(!strcasecmp(p, "mirror")) {
    IMP_ISP_Tuning_SetISPVflip(0);
    IMP_ISP_Tuning_SetISPHflip(1);
  } else if(!strcasecmp(p, "flip_mirror")) {
    IMP_ISP_Tuning_SetISPVflip(1);
    IMP_ISP_Tuning_SetISPHflip(1);
  } else {
    return "error";
  }
  return "ok";
}

static char *Contrast(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned char cont;
    IMP_ISP_Tuning_GetContrast(&cont);
    sprintf(CommandResBuf, "%d\n", cont);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetContrast(atoi(p));
  return res ? "error": "ok";
}

static char *Brightness(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned char bri;
    IMP_ISP_Tuning_GetBrightness(&bri);
    sprintf(CommandResBuf, "%d\n", bri);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetBrightness(atoi(p));
  return res ? "error": "ok";
}

static char *Saturation(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned char sat;
    IMP_ISP_Tuning_GetSaturation(&sat);
    sprintf(CommandResBuf, "%d\n", sat);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetSaturation(atoi(p));
  return res ? "error": "ok";
}

static char *Sharpness(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned char sharpness;
    IMP_ISP_Tuning_GetSharpness(&sharpness);
    sprintf(CommandResBuf, "%d\n", sharpness);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetSharpness(atoi(p));
  return res ? "error": "ok";
}

static char *AEComp(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    int comp;
    IMP_ISP_Tuning_GetAeComp(&comp);
    sprintf(CommandResBuf, "%d\n", comp);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetAeComp(atoi(p));
  return res ? "error": "ok";
}

static char *AEItMax(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int itMax;
    IMP_ISP_Tuning_GetAE_IT_MAX(&itMax);
    sprintf(CommandResBuf, "%d\n", itMax);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetAe_IT_MAX(atoi(p));
  return res ? "error": "ok";
}

static char *AEMin(char *tokenPtr) {

  IMPISPAEMin aemin;
  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    IMP_ISP_Tuning_GetAeMin(&aemin);
    sprintf(CommandResBuf, "%d %d %d %d\n", aemin.min_it, aemin.min_again, aemin.min_it_short, aemin.min_again_short);
    return CommandResBuf;
  }
  aemin.min_it = atoi(p);

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  aemin.min_again = atoi(p);

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  aemin.min_it_short = atoi(p);

  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  aemin.min_again_short = atoi(p);

  int res = IMP_ISP_Tuning_SetAeMin(&aemin);
  return res ? "error": "ok";
}

static char *Sinter(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int res = IMP_ISP_Tuning_SetSinterStrength(atoi(p));
  return res ? "error": "ok";
}

static char *Temper(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int res = IMP_ISP_Tuning_SetTemperStrength(atoi(p));
  return res ? "error": "ok";
}

static char *DPC(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int dpc;
    IMP_ISP_Tuning_GetDPC_Strength(&dpc);
    sprintf(CommandResBuf, "%d\n", dpc);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetDPC_Strength(atoi(p));
  return res ? "error": "ok";
}

static char *DRC(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int drc;
    IMP_ISP_Tuning_GetDRC_Strength(&drc);
    sprintf(CommandResBuf, "%d\n", drc);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetDRC_Strength(atoi(p));
  return res ? "error": "ok";
}

static char *HiLight(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int strength;
    IMP_ISP_Tuning_GetHiLightDepress(&strength);
    sprintf(CommandResBuf, "%d\n", strength);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetHiLightDepress(atoi(p));
  return res ? "error": "ok";
}

static char *AGain(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int gain;
    IMP_ISP_Tuning_GetMaxAgain(&gain);
    sprintf(CommandResBuf, "%d\n", gain);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetMaxAgain(atoi(p));
  return res ? "error": "ok";
}

static char *DGain(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int gain;
    IMP_ISP_Tuning_GetMaxDgain(&gain);
    sprintf(CommandResBuf, "%d\n", gain);
    return CommandResBuf;
  }
  int res = IMP_ISP_Tuning_SetMaxDgain(atoi(p));
  return res ? "error": "ok";
}

static char *Expr(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  IMPISPExpr attr;
  if(!p) {
    IMP_ISP_Tuning_GetExpr(&attr);
    sprintf(CommandResBuf, "%s %d %d %d %d\n", attr.g_attr.mode ? "manual" : "auto", attr.g_attr.time, attr.g_attr.time_min, attr.g_attr.time_max, attr.g_attr.one_line_expr_in_us);
    return CommandResBuf;
  }

  int res = 0;
  if(!strcasecmp(p, "auto")) {
    attr.s_attr.mode = 0;
    attr.s_attr.unit = 0;
    attr.s_attr.time = 0;
    IMPISPAEMin aemin;
    IMP_ISP_Tuning_GetAeMin(&aemin);
    p = strtok_r(NULL, " \t\r\n", &tokenPtr);
    if(!p) return "error";
    p = strtok_r(NULL, " \t\r\n", &tokenPtr);
    if(!p) return "error";
    aemin.min_it = atoi(p);
    p = strtok_r(NULL, " \t\r\n", &tokenPtr);
    if(!p) return "error";
    int itmax = atoi(p);
    res |= IMP_ISP_Tuning_SetAeMin(&aemin);
    res |= IMP_ISP_Tuning_SetAe_IT_MAX(itmax);
    res |= IMP_ISP_Tuning_SetExpr(&attr);
  } else if(!strcasecmp(p, "manual")) {
    attr.s_attr.mode = 1;
    attr.s_attr.unit = 0;
    p = strtok_r(NULL, " \t\r\n", &tokenPtr);
    if(!p) return "error";
    attr.s_attr.time = atoi(p);
    IMPISPAEMin aemin;
    IMP_ISP_Tuning_GetAeMin(&aemin);
    aemin.min_it = 1;
    res |= IMP_ISP_Tuning_SetAeMin(&aemin);
    res |= IMP_ISP_Tuning_SetAe_IT_MAX(1683);
    res |= IMP_ISP_Tuning_SetExpr(&attr);
  } else {
    res = -1;
  }
  return res ? "error": "ok";
}

static char *Bitrate(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch != 0) && (ch != 1) && (ch != 3)) return "error";
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    if(userBitrate[ch]) {
      sprintf(CommandResBuf, "%d\n", userBitrate[ch]);
    } else {
      sprintf(CommandResBuf, "auto %d\n", appBitrate[ch]);
    }
    return CommandResBuf;
  }
  if(!strcasecmp(p, "auto")) {
    userBitrate[ch] = 0;
    fprintf(stderr, "video_set_kbps ch%d: %d\n", ch, appBitrate[ch]);
    real_local_sdk_video_set_kbps(ch, appBitrate[ch]);
  } else {
    int kbps = atoi(p);
    if((kbps < 10) || (kbps > 3000)) return "error";
    userBitrate[ch] = kbps;
    fprintf(stderr, "video_set_kbps ch%d: %d\n", ch, userBitrate[ch]);
    real_local_sdk_video_set_kbps(ch, userBitrate[ch]);
  }
  return "ok";
}

static char *Framerate(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int num, den;
    IMP_ISP_Tuning_GetSensorFPS(&num, &den);
    struct IMPEncoderFrmRate encFps;
    IMP_Encoder_GetChnFrmRate(0, &encFps);
    if(VideoControl_UserFps) {
      sprintf(CommandResBuf, "%d isp:%d/%d enc:%d/%d\n", VideoControl_UserFps, num, den, encFps.frmRateNum, encFps.frmRateDen);
    } else {
      sprintf(CommandResBuf, "auto %d isp:%d/%d enc:%d/%d\n", VideoControl_AppFps, num, den, encFps.frmRateNum, encFps.frmRateDen);
    }
    return CommandResBuf;
  }

  int fps = VideoControl_AppFps;
  if(!strcasecmp(p, "auto")) {
    VideoControl_UserFps = 0;
    fprintf(stderr, "video framerate : auto %d\n", VideoControl_AppFps);
  } else {
    fps = atoi(p);
    if((fps < 1) || (fps > 30)) return "error";
    VideoControl_UserFps = fps;
    fprintf(stderr, "video framerate: %d\n", VideoControl_UserFps);
  }

  local_sdk_video_stop(0, 0, 0);
  local_sdk_video_stop(1, 0, 0);
  local_sdk_video_stop(3, 0, 0);
  local_sdk_video_set_gop(0, fps);
  local_sdk_video_set_gop(1, fps);
  local_sdk_video_set_gop(3, fps);
  real_local_sdk_video_set_fps(fps);
  local_sdk_video_start(0);
  local_sdk_video_start(1);
  local_sdk_video_start(3);
  return "ok";
}

// encdump <ch> attr|rc|gop [<first word>]: dump 16 32-bit words of the encoder channel state as "<word>:<hex>".
// A reply is limited to ~250 bytes, so ask again with the next first word to page through it.
static char *EncDump(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch < 0) || (ch > 3)) return "error";
  char *what = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!what) return "error";
  int first = 0;
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(p) first = atoi(p);
  if((first < 0) || (first > 240)) return "error";

  static unsigned int buf[256];
  memset(buf, 0, sizeof(buf));
  int ret;
  if(!strcasecmp(what, "attr")) {
    ret = IMP_Encoder_GetChnAttr(ch, buf);
  } else if(!strcasecmp(what, "rc")) {
    ret = IMP_Encoder_GetChnAttrRcMode(ch, buf);
  } else if(!strcasecmp(what, "gop")) {
    ret = IMP_Encoder_GetChnGopAttr(ch, buf);
  } else {
    return "error";
  }
  if(ret) {
    sprintf(CommandResBuf, "error %d\n", ret);
    return CommandResBuf;
  }
  int n = 0;
  for(int i = first; (i < first + 16) && (i < 256); i++) {
    n += sprintf(CommandResBuf + n, "%d:%x ", i, buf[i]);
  }
  CommandResBuf[n++] = '\n';
  CommandResBuf[n] = 0;
  return CommandResBuf;
}

// The rate control struct is 36 bytes (libimp's GetChnAttrRcMode copies 9 words): word 0 is the mode, words 1-8 the
// per-mode union (CBR: maxQp, minQp, outBitRate, iBiasLvl, frmQPStep, gopQPStep, adaptiveMode, gopRelation; the other
// modes differ - read it first). Changes are applied to the running encoder and are gone after a reboot.
// encrc <ch> [<word> <value> ...]: print the words, or change some and print the result.
static char *EncRc(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch < 0) || (ch > 3)) return "error";

  static unsigned int rc[16];
  memset(rc, 0, sizeof(rc));
  int ret = IMP_Encoder_GetChnAttrRcMode(ch, rc);
  if(ret) {
    sprintf(CommandResBuf, "error get %d\n", ret);
    return CommandResBuf;
  }
  int changed = 0;
  while((p = strtok_r(NULL, " \t\r\n", &tokenPtr))) {
    int idx = atoi(p);
    char *v = strtok_r(NULL, " \t\r\n", &tokenPtr);
    if(!v || (idx < 0) || (idx > 8)) return "error";
    rc[idx] = (unsigned int)strtol(v, NULL, 0);
    changed = 1;
  }
  if(changed) {
    ret = IMP_Encoder_SetChnAttrRcMode(ch, rc);
    fprintf(stderr, "encrc ch%d set: %d\n", ch, ret);
    if(ret) {
      sprintf(CommandResBuf, "error set %d\n", ret);
      return CommandResBuf;
    }
    memset(rc, 0, sizeof(rc));
    IMP_Encoder_GetChnAttrRcMode(ch, rc);
  }
  int n = 0;
  for(int i = 0; i < 9; i++) n += sprintf(CommandResBuf + n, "%d:%d ", i, (int)rc[i]);
  CommandResBuf[n++] = '\n';
  CommandResBuf[n] = 0;
  return CommandResBuf;
}

// encgop <ch> [<length>]: print / set the GOP (keyframe) length of a running channel.
static char *EncGop(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch < 0) || (ch > 3)) return "error";
  static unsigned char buf[64];
  memset(buf, 0, sizeof(buf));
  struct IMPEncoderGopAttr *g = (struct IMPEncoderGopAttr *)buf;
  int ret = IMP_Encoder_GetChnGopAttr(ch, g);
  if(ret) {
    sprintf(CommandResBuf, "error get %d\n", ret);
    return CommandResBuf;
  }
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(p) {
    int len = atoi(p);
    if((len < 1) || (len > 600)) return "error";
    g->uGopLength = len;
    ret = IMP_Encoder_SetChnGopAttr(ch, g);
    fprintf(stderr, "encgop ch%d set %d: %d\n", ch, len, ret);
    if(ret) {
      sprintf(CommandResBuf, "error set %d\n", ret);
      return CommandResBuf;
    }
    memset(buf, 0, sizeof(buf));
    IMP_Encoder_GetChnGopAttr(ch, g);
  }
  sprintf(CommandResBuf, "mode %d length %d numB %d sameScene %u LT %d freqLT %u LTRC %d\n", g->uGopCtrlMode, g->uGopLength, g->uNumB,
          g->uMaxSameSenceCnt, g->bEnableLT, g->uFreqLT, g->bLTRC);
  return CommandResBuf;
}

// iqp <ch> [<qp>|auto]: minimum quantizer of a channel (the floor the rate control may not go below; lower = more detail
// where the rate control has room, not necessarily bigger keyframes). "auto" = the app default.
static char *Iqp(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch != 0) && (ch != 1) && (ch != 3)) return "error";
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int rc[16];
    memset(rc, 0, sizeof(rc));
    if(IMP_Encoder_GetChnAttrRcMode(imp_channel(ch), rc)) return "error";
    sprintf(CommandResBuf, "%s %d\n", userIqp[ch] ? "user" : "auto", (int)(rc[3] >> 16));
    return CommandResBuf;
  }
  if(!strcasecmp(p, "auto")) {
    userIqp[ch] = 0;
    return "ok";
  }
  int qp = atoi(p);
  if((qp < 10) || (qp > 51)) return "error";
  userIqp[ch] = qp;
  start_keeper();
  apply_iqp(ch);
  return "ok";
}

// gop <ch> [<seconds 1-10>|auto]: keyframe interval of a channel in whole seconds. The encoder only produces keyframes at
// whole seconds; the frame count is derived from the live frame rate (see userGop). Without a value: "user|auto <s> s
// <frames> frames" with the frame count the encoder actually holds.
static char *Gop(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch != 0) && (ch != 1) && (ch != 3)) return "error";
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned char buf[64];
    memset(buf, 0, sizeof(buf));
    struct IMPEncoderGopAttr *g = (struct IMPEncoderGopAttr *)buf;
    if(IMP_Encoder_GetChnGopAttr(imp_channel(ch), g)) return "error";
    int fps = encoder_fps(imp_channel(ch));
    sprintf(CommandResBuf, "%s %d s %d frames at %d fps\n", userGop[ch] ? "user" : "auto",
            userGop[ch] ? userGop[ch] : (g->uGopLength + fps / 2) / fps, g->uGopLength, fps);
    return CommandResBuf;
  }
  if(!strcasecmp(p, "auto")) {
    userGop[ch] = 0;
    return "ok";
  }
  int sec = atoi(p);
  if((sec < 1) || (sec > 10)) return "error";
  userGop[ch] = sec;
  start_keeper();
  apply_gop(ch);
  return "ok";
}

// rc <ch> [<field> [<value>|auto]]: the remaining rate control fields (see struct RcField). Without a field the live
// decoded values are printed ("*" = user override). "auto" drops the override: the app's value comes back when the
// channel is created again (the running encoder keeps what it has).
static char *Rc(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch != 0) && (ch != 1) && (ch != 3)) return "error";
  int impCh = imp_channel(ch);
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int rc[16];
    memset(rc, 0, sizeof(rc));
    if(IMP_Encoder_GetChnAttrRcMode(impCh, rc)) return "error";
    int n = sprintf(CommandResBuf, "mode %u minqp %d", rc[0], (int)(rc[3] >> 16));
    for(int i = 0; i < RC_FIELDS; i++)
      n += sprintf(CommandResBuf + n, " %s%s %d", RcFields[i].name, userRcSet[impCh][i] ? "*" : "", rc_get(rc, &RcFields[i]));
    CommandResBuf[n++] = '\n';
    CommandResBuf[n] = 0;
    return CommandResBuf;
  }
  int fi = -1;
  for(int i = 0; i < RC_FIELDS; i++) if(!strcasecmp(p, RcFields[i].name)) fi = i;
  if(fi < 0) return "error";
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) {
    unsigned int rc[16];
    memset(rc, 0, sizeof(rc));
    if(IMP_Encoder_GetChnAttrRcMode(impCh, rc)) return "error";
    sprintf(CommandResBuf, "%s %d\n", userRcSet[impCh][fi] ? "user" : "auto", rc_get(rc, &RcFields[fi]));
    return CommandResBuf;
  }
  if(!strcasecmp(p, "auto")) {
    userRcSet[impCh][fi] = 0;
    return "ok";
  }
  int v = atoi(p);
  if((v < RcFields[fi].lo) || (v > RcFields[fi].hi)) return "error";
  userRcSet[impCh][fi] = 1;
  userRcVal[impCh][fi] = v;
  apply_rc(ch);
  return "ok";
}

static char *StreamBuf(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch < 0) || (ch > 2)) return "error";
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(p) {
    if(!strcasecmp(p, "auto")) {
      userStreamBufKb[ch] = 0;
    } else {
      int kb = atoi(p);
      if((kb < 64) || (kb > 8192)) return "error";
      userStreamBufKb[ch] = kb;
    }
    return "ok (applies when the channel is created again)";
  }
  unsigned int size = 0;
  int ret = IMP_Encoder_GetStreamBufSize(ch, &size);
  sprintf(CommandResBuf, "ch%d streambuf %u bytes (user %d kB, ini %d kB) get=%d\n", ch, size, userStreamBufKb[ch],
          ini_int(ch == 0 ? "STREAMBUF_MAIN_AVC" : ch == 1 ? "STREAMBUF_SUB_HEVC" : "STREAMBUF_MAIN_HEVC"), ret);
  return CommandResBuf;
}

static char *StreamCnt(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(!p) return "error";
  int ch = atoi(p);
  if((ch < 0) || (ch > 2)) return "error";
  p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(p) {
    if(!strcasecmp(p, "auto")) {
      userStreamCnt[ch] = 0;
    } else {
      int n = atoi(p);
      if((n < 1) || (n > 32)) return "error";
      userStreamCnt[ch] = n;
    }
    return "ok (applies when the channel is created again)";
  }
  int cnt = 0;
  int ret = IMP_Encoder_GetMaxStreamCnt(ch, &cnt);
  sprintf(CommandResBuf, "ch%d streamcnt %d (user %d) get=%d\n", ch, cnt, userStreamCnt[ch], ret);
  return CommandResBuf;
}

static char *Stats(char *tokenPtr) {

  char *p = strtok_r(NULL, " \t\r\n", &tokenPtr);
  if(p && !strcasecmp(p, "reset")) {
    video_stats_reset();
    return "ok";
  }
  int n = video_stats_text(CommandResBuf, 240);
  CommandResBuf[n] = 0;
  return CommandResBuf;
}
