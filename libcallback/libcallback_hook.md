# libcallback

iCamera_appの起動時にLD_PRELOADで読み込ませることでdynamic linkされる前にlibcallbackのhook関数が読み込まれlinkされる。

これによって内部で使用しているlibararyの関数を置き換えることができるが、libararyに出ていない関数は置き換えられないのでできることに限界はある。



### command.c

コマンドを受け付けるためのIF。

port 4000でlocalhostのみに限定してsocketで通信を待ち受けている。

各種コマンドを解釈後、それぞれのコマンド処理を呼び出し、応答を返す。



### alarm_config.c

##### commandIF

```
usage : alarmConfig <alarmType> alarmInterval [<interval>]
```

WyzeCamのみ有効。

alarmType(0~14) 毎のalarmIntervalの設定値を設定／取得する。

引数が無い場合は現在の設定値を返す。

intervalに30秒未満は設定できない、defaultが300秒なので30~300の範囲での設定が可能。

##### hook point

```c
/system/lib/libc.so.0 : void *memset(void *s, int c, size_t n)
```

alarmConfigのテーブルアドレスを取得するため、テーブル初期化しているところをhook。



### alarm_interval.c

##### commandIF

```
usage : alarm [<interval>]
```

user_configのalarmIntervalの設定値を設定／取得する。

引数が無い場合は現在の設定値を返す。

intervalに30秒未満は設定できない、defaultが300秒なので30~300の範囲での設定が可能。



### audio_callback.c

##### commandIF

```
audio [<ch>] [on|off]
```

audio chのALSA loopbackへの出力の制御。

引数が無い場合は現在の設定値を返す。



##### hook point

```c
/system/lib/liblocalsdk.so : int local_sdk_audio_set_pcm_frame_callback(int ch, void *callback)
```

audioのPCMデータを横取りするため、callback設定関数で設定されるcallback関数を置き換える処理。

置き換えたcallback関数でALSA loopbackデバイスにデータを横流しして、iCamera_appの本来のcallbackに戻る処理をする。



### audio_control.c

##### commandIF

```
audio hpf [on|off]
audio agc [<gainLevel> <maxGain>]
audio ns [off|<level 0-3>]
audio aec [on|off]
audio vol [<volume -30-120>]
audio gain [<gain 0-31>]
audio alc [<alc 0-7>]
```

各種soundの設定。

基本的には引数をつけなければ現在の設定値を返す。

パラメータの意味するところはよくわからないものもある。

audio hpfはHighPassFilterのon/off。

audio agcはAutoGainControlの設定。

audio nsはNoiseSuppressionの設定。

audio aecはエコーキャンセルのon/off。

audio volはボリューム設定。

audio gainは入力ゲイン調整の設定。

audio alcはよくわからない。



### audio_play.c

```
aplay <wav file> [<volume -30-120:default 40>]
```

指定されたwavファイルをスピーカーから再生する。

wavファイルは8Ksampleのデータのみ対応。



### curl.c

#####  commandIF

```
curl debug [on|off]
curl upload [disable|enable]
```

iCamera_appがサーバーにalarm videoをpostするのを制御する。

引数が無い場合は現在の設定値を返す。

curl debugはiCamera_appがcurlで通信している内容を表示する機能のon/offの設定。

curl uploadはiCamera_appがalarm videoをuploadする事を許可するかどうかの設定。

##### hook point

```C
/thirdlib/libcurl.so : CURLcode curl_easy_perform(struct SessionHandle *data)
```

動体検知の周期を短縮した時にサーバーに頻繁にuploadしないように５分間以内の場合の棄却処理。

サーバーへの検知動画のuploadを停止した場合の棄却処理を行う。

棄却処理は単純に捨ててしまうとエラーになってしまうため、それらしい応答を返してうまく処理する必要がある。



### freopen.c

##### hook point

```c
/lib/libc.so.0 : FILE *freopen(const char *pathname, const char *mode, FILE *stream)
```

stdoutの情報からhookを掛けるため、iCamera_appがstdoutの出力を止める処理をしているところを回避。



### gmtime_r.c

##### hook point

```C
/lib/libc.so.0 : struct tm *gmtime_r(const time_t *timep, struct tm *result)
```

AtomCamSwingでカメラを動かすと動体検知が反応するため、動かしている間は存在しない曜日を返すことでAIプロセスを無効にする。

ここはiCamera_appの実装に依存しているので、コードが変わると効かなくなる可能性がある。



### get_jpeg.c

#####  commandIF

```
skipRecJpeg [on|off]
```

/media/mmc/record/以下の連続録画の記録時にjpegも一緒に記録されるが、不要な時にonにすることで記録されなくなる。

##### hook point

```C
/thirdlib/liblocalsdk.so : int local_sdk_video_get_jpeg(int ch, char *path)
```

skipRecJpegがonの場合、指定chの静止画をjpegとして記録する時にpathを比較して/media/mmc/record/以下の場合は無視する。




### jpeg.c

#####  commandIF

```
jpeg [<ch:default 0>] [-n]
```

指定されたvideo chのjpegデータを返す。

-nを指定しない場合、httpのres headerをつけて返す。

-nを指定した場合、生のjpegデータを返す。



### mmc_format.c

##### hook point

```C
/system/lib/liblocalsdk.so : int local_sdk_device_mmc_format()	
```

SD-Cardをformatしないように何もせずに戻る。



### mmc_mount.c

##### hook point

```C
/system/lib/liblocalsdk.so : int local_sdk_device_open(int id, char *buf)
```

既にtoolsでmountされているのにiCamera_appがSD-Cardを二重にmountしてきてFileSystemがおかしくなるので、/media/mmcをmountさせないようにする。



### motor.c

##### commandIF

```
move [<pan 0-355> <tilt 0-180>] [<speed 0-9:default 9>] [<priority 0-3:default 2>]
```

AtomCamSwingでpan/tiltの向きを変更／取得する。

引数が無い場合は現在の位置とhflip, vflipを返す。

pan, tiltを指定した場合、その方向まで向きを変える。

移動している間は次の移動コマンドはエラーになる。

移動し終わるとpan, tilt, flip, vflipの値を返す。

```
relmove <dpan> <dtilt> [<speed 1-9:default 9>] [<priority 0-3:default 2>]
```

AtomCamSwingでpan/tiltを現在位置からの相対角度(度)で動かす。dpan, dtiltは画像上で右/上が正で、hflip/vflipは内部で補正する。

内部では`local_sdk_motor_move_rel_angle`(角度をステップに変換して`move_rel_step`を呼ぶ)を使う。範囲は|dpan|<=355, |dtilt|<=180。

moveと同様に移動中は次のコマンドはエラーになり(優先度が高い場合は移動中のものをキャンセル)、移動し終わると位置とflipの値を`move`と同じ形式で返す。

```
contmove <h 0-2> <v 0-2> [<speed 1-9:default 9>] [<priority 0-3:default 2>]
contstop
ptzstatus
```

AtomCamSwingでpan/tiltを止めるまで動かし続ける(SDKの`local_sdk_motor_move`/`local_sdk_motor_stop`)。h, vはSDKの生の方向コマンドで、0は動かさない、1と2は互いに逆向き。どちらがどの向きかは設置(flip)で変わる。すぐに`ok`を返す。

`ptzstatus`は`pan,tilt,0,moving`を1行で返す(整数に丸め、flipは`move`と同様に補正)。movingは`move`/`relmove`/`contmove`のどれかが動作中なら1。ONVIFのGetStatus用。



### mp4write.c

##### commandIF

```
mp4write [sd|ram sd|ram]
```

record、alarmのvideoの一時書き込み場所をSD-CardにするかRAMDiskにするかを指定する。

defaultはRAMDisk。

引数のない場合は現在の設定値を返す。

1番めの引数はrecord(１分ごとの定期的なvideo)についての設定。

2番目の引数はalarm(動体検知でのvideo)についての設定。

##### hook point

```C
/system/lib/libmp4rw.so : int mp4write_start_handler(void *handler, char *file, struct Mp4StartConfig *config)
/lib/libc.so.0 : int snprintf(char *str, size_t size, const char *format, ...)
```

alarmかrecordのファイルを/tmp/のRAMDiskに作成してからSD-Cardに移動するのがiCamera_appの標準動作。

これを直接SD-Cardの/media/mmc/tmp/に記録して、同じファイルシステム内で移動するこで負荷を低減させる処理。

snprintfはWyzeCamではmp4write_start_handler実行後に再度pathが設定されてremoveされなくなるため、設定をさせないための処理。



### night_light.c

##### commandIF

```
night [on|off|auto]
```

night lightのon/off/自動の切り替え設定。



### opendir.c

##### hook point

```C
/lib/libc.so.0 : DIR *opendir(const char *pathname)
```

iCamera_appのtimelapseの記録イベントを検知してstdoutにwebhook用のメッセージを出力する。

iCamera_appのtimelapseのWebHookを使わないなら不要。



### remove.c

##### hook point

```C
/lib/libc.so.0 : int remove(const char *pathname)
```

iCamera_appのtimelapseの完了イベントを検知してstdoutにwebhook用のメッセージを出力する。

iCamera_appのtimelapseのWebHookを使わないなら不要。



### setlinebuf.c

iCamera_appのstdout出力がバファリングされるため、lineごとに出力するように設定。

イベント検知のために使用。



### timelapse.c

##### commandIF

```
timelapse [<filename> <interval> <numOfTimes> <fps>]
timelapse stop
timelapse restart
timelapse close
timelapse mp4 <filename>
```

timelapse videoを録画する。

引数無しは現在の記録状態を返す。

最終的にinterval間隔でnumOfTimes回のフレームをfpsのフレームレートで再生されるfilenameで指定したmp4ファイルを作成するが、開始時は同名の拡張子が_mp4, stszの名前の中間ファイルを生成する。

stopは記録を停止する。

restartはstopした記録を再開する。

closeは記録を停止してmp4ファイルを生成する。

mp4は停止していて中間ファイルが残った状態のものをmp4ファイルに変換する。



### usb_power.c

iCamera_appからのUSB VBUS制御を無視する。



### user_config.c

##### commandIF

```
config <name> [<value>]
```

iCamera_app内部のuser_configの値の取得と設定。

valueを指定しなければそのnameの値を取得する。

valueを指定した場合はそのnameの値にvalueを設定する。

ただし、ファイルへの書き出しはされないため別途ファイルに書く必要がある。

変更している値の意味を理解した上で使用する事。

##### hook point

```C
/lib/libc.so.0 : int strncmp(const char *s1, const char *s2, size_t size)
```

iCamera_appの内部変数のget/setをするために内部変数テーブルのポインタを取得する。

Get/SetUserConfig()でget/setするが、setで/atom/config/.user_configファイルは変更されないので恒久化するには別途更新する必要がある。



### video_callback.c

##### commandIF

```
video [<ch:default 0>] [on|off]
```

v4l2loopbackの各chのvideoのcaptureのon/off。

引数無し、chのみ指定の場合は現在の設定値を返す。

##### hook point

```C
/system/lib/liblocalsdk.so : int local_sdk_video_set_encode_frame_callback(int sch, void *callback) 
```

VideoのH264/265 frameを横取りするため、callback設定関数で設定されるcallback関数を置き換える処理。

置き換えたcallback関数でv4l2loopbackデバイスにデータを横流しして、iCamera_appの本来のcallbackに戻る処理をする。



### video_control.c

##### commandIF

```
video flip [normal/flip/mirror/flip_mirror]
video cont 0 - 255(center:128)
video bri 0 - 255(center:128)
video sat 0 - 255(center:128)
video sharp 0 - 255(center:128)
video sinter 0 - 255(center:128)
video temper 0 - 255(center:128)
video aecomp 0 - 255
video aeitmax 0-
video dpc 0 - 255
video drc 0 - 255
video hilight 0 - 10
video again 0 -
video dgain 0 -
video expr manual|auto <time>
video bitrate <ch> 10-3000(kbps)|auto
video fps <ch> 1-30(fps)|auto
video iqp <ch 0|1|3> [10-51|auto]
video gop <ch 0|1|3> [1-10(秒)|auto]
video rc <ch 0|1|3> [<field> [<value>|auto]]
video encdump <ch 0-3> attr|rc|gop [<first word>]
video encrc <ch> [<word 0-8> <value> ...]
video encgop <ch> [<length>]
video streambuf <imp ch 0-2> [<kB>|auto]
video streamcnt <imp ch 0-2> [<n>|auto]
video stats [reset]
```

videoの各種パラメータ設定。よくわからないものもある。

引数無しの場合は現在の設定値を表示する。

video flipは画面の上下左右の反転。

video contはコントラスト設定。

video briはブライトネス設定。

video satはサチュレーション設定。

video sharpはシャープネス設定。

video aecompは自動露出設定。

video aeitmaxは自動露出関連、よくわからない。

video dpc,drc,hilight,again, dgainこのあたりもよくわからない。

video exprは露出設定のauto/manualの切り替えだけど、manualだと安定しない。

video bitrate, fpsはH264/H265のvbrのbitrateとフレームレート設定。

チャンネル番号は、`bitrate` `iqp` `gop` `rc`ではSDKの番号(0=Main H.264, 1=Sub HEVC, 3=Main HEVC)、`encdump` `encrc` `encgop` `streambuf` `streamcnt`ではlibimpの番号(0=Main H.264, 1=Sub HEVC, 2=Main HEVC)です。

video iqpは最小QP(レート制御構造体のword 3の上位16bit)の設定。全フレームの下限であり、キーフレームだけの設定ではない。アプリがビットレートやフレームレートを設定し直すたびに再適用される。

video gopはキーフレーム間隔を整数秒(1-10)で設定する。エンコーダは整数秒ごとにしかキーフレームを作らない(フレーム数がfpsの倍数でない値は、読み出し値がfps分大きくなり、ceil(値/fps)+1秒で動作する)ため、フレーム数はその時のフレームレートから計算し、フレームレートが変わる(夜間15fps/昼間20fps)たびに再計算する。アプリがフレームレートから決める値(2.5倍)を上書きする。値なしで読み出すと`user|auto <秒> s <フレーム数> frames at <fps> fps`を返す。hack.iniのキーは`GOP_SEC_MAIN_AVC` `GOP_SEC_SUB_HEVC` `GOP_SEC_MAIN_HEVC`で、エンコーダ生成時に読み込む。最小QP(`IQP_*`)も同様に生成時に読み込み、数秒おきにエンコーダの値を確認して設定値に戻す。

video rcはレート制御の残りの項目の設定。項目は`initqp` `maxqp` `ipdelta` `pbdelta` `options` `maxpic`(kbit) `maxpsnr`(dB)。項目を省略すると現在値を表示し、ユーザー指定のものには`*`が付く。`ipdelta` `maxpic` `maxpsnr`などは稼働中のエンコーダには反映されず、エンコーダ作成時(`IMP_Encoder_CreateChn`のフック)に反映される。hack.iniの`<項目>_<MAIN_AVC|SUB_HEVC|MAIN_HEVC>`キーでも指定できる。レート制御構造体(9ワード、モード4と8)の意味は下の「rc struct」を参照。

video encdump, encrc, encgopはエンコーダの属性・レート制御・GOPの読み書き(調査用)。encrcは稼働中のエンコーダのレート制御構造体を直接書き換える。

video streambuf, streamcntはエンコーダのストリームバッファの大きさと個数(エンコーダ作成時に反映される)。

video statsはチャンネルごとのフレーム数、最大フレームサイズ、コールバック時間、停止回数の統計(resetで0に戻す)。

##### hook point

```c
/system/lib/liblocalsdk.so : int local_sdk_video_set_kbps(int ch, int kbps)
```

```c
/system/lib/liblocalsdk.so : int local_sdk_video_set_fps(int ch, int kbps)
```

```c
/system/lib/libimp.so : int IMP_Encoder_CreateChn(int ch, unsigned char *attr)
```

Videoの BitRate, FrameRate, GOPの設定をユーザー設定値に変更する処理。

IMP_Encoder_CreateChnで最初に設定されたGOPとfpsは最大値に設定されるため、以降でこの値以上の値を設定することはできない。

また、GOPはfpsで割り切れる必要がある。

### wait_motion.c

##### commandIF

```
waitMotion [<timeout>] 
```

動体検知の状態が変化するのを待つ。

timeoutを指定しないと待ち続ける。

検知した場合、```detect <left> <right> <top> <bottom> <pan> <tilt>```の値を返す。

検知が外れた場合、```clear```を返す。

##### hook point

```
/system/lib/liblocalsdk.so : int local_sdk_video_osd_update_rect(int ch, int display, struct RectInfoSt *rectInfo)
```

AtomCamSwingでiCamera_appの動体検知の枠描画から追尾目標を計算して返す処理。



### property.c

##### commandIF

```
property
property raw <item> <val>
property nightVision [on|off|auto] 
property nightCutThr [dark|darkness] 
property IrLED [on|off] 
property motionDet [on|off] 
property motionLevel [low|mid|high] 
property soundDet [on|off] 
property soundLevel [low|mid|high] 
property cautionDet [on|off] 
property drawBoxSwitch [on|off] 
property recordType [cont|motion] 
property indicator [on|off] 
property horSwitch [on|off] 
property verSwitch [on|off] 
property rotate [on|off] 
property audioRec [on|off] 
property timestamp [on|off] 
property watermark [on|off] 
property motionArea [all|rect] [<sx> <sy> <width> <height>]
```

MobileAppのUIからの操作に相当するコマンド群。

propertyはlist形式で現在値を出力

property rawはitemとvalを直接値で指定（主にdebug用）

property nightVisionはナイトビジョンのオン／オフ／自動の切り替え
property nightCutThrはナイトビジョン自動の時の切り替えタイミング。暗い／非常に暗い
property IrLEDはナイトビジョン用赤外線ライトのオン／オフ
property motionDetはモーション検知のオン／オフ
property motionLevel はモーション検知の感度調整。高／中／低
property soundDetはサウンド検出のオン／オフ
property soundLevelはサウンド検出の感度調整。高／中／低
property cautionDetは火災／CO警報機音検知のオン／オフ
property drawBoxSwitchはモーションタグのオン／オフ
property recordTypeは録画モードの切り替え。モーション検知時のみ／連続録画
property indicatorはステータスランプのオン／オフ
property horSwitchは画像水平反転のオン／オフ
property verSwitchは画像垂直反転のオン／オフ
property rotateは画像水平垂直反転のオン／オフ
property audioRecは録音のオン／オフ
property timestampはタイムスタンプのオン／オフ
property watermarkはロゴのオン／オフ
property motionAreaは検知領域の切り替え
property raw以外は引数無しの場合は現在値を返す。

##### hook point

Constructor set_property_initでiCamera_appの.rodataからp2p recv protocolの文字列を検索、.text内でそこを参照している箇所を探し出し、その関数を呼び出している関数の先頭アドレスを求めている。

これを

```
void ProtocolSetProperty(char * buf1, char *req, char *res);
```

と定義して、req, resのjson文字列でアクセスしてAPIを呼び出している。

### rc struct

`IMP_Encoder_GetChnAttrRcMode` / `SetChnAttrRcMode`が扱う9ワードの意味です(カメラの`libimp.so`を逆アセンブルして確認)。モードは 0=固定QP, 1=CBR, 2=VBR, 4と8=同じ変換(上限付き。アプリは全チャンネルで8を使用)で、3/5/6/7は拒否されます。モード4/8:

| ワード | 内容 | アプリの値 | 備考 |
|---|---|---|---|
| 0 | モード | 8 | |
| 1 | 目標ビットレート kbps | 上限の0.75倍 | |
| 2 | 上限ビットレート kbps | | `video bitrate`で設定 |
| 3 下位 | 初期QP | -1(自動) | `video rc initqp` |
| 3 上位 | **最小QP** | AVC 23 / HEVC 32 | `video iqp`。全フレームの下限で、キーフレーム専用のQPではない |
| 4 下位 | 最大QP | 51 | `maxqp` |
| 4 上位 | I/PのQP差 | -1(自動) | `ipdelta`。キーフレームを大きくする(AVCで3→10: 115→193kB) |
| 5 下位 | P/BのQP差 | -1(自動) | `pbdelta` |
| 6 | オプション(ビットマスク) | 1 | `options` |
| 7 | 最大ピクチャサイズ kbit | 1920(1080p) / 640(sub) | `maxpic`。libimpが解像度に応じた下限(1080pで1352kbit、360pで624kbit)に引き上げる。ビットレートには依存しない |
| 8 下位 | 最大PSNR dB | 48 | `maxpsnr` |

- ワード7・8とI/P差は、**動作中のチャンネルに書いても効果がありません**(エンコーダ作成時のみ有効)。hack.iniのキーを`IMP_Encoder_CreateChn`のフックで反映するのはこのためです。実行中に効くのはビットレートとQP範囲です。
- 最小QPを30→26→20と下げても、レート制御が既に選んでいる値より小さい範囲ではキーフレームは変わらず、Pフレームが4-8倍になります。38に上げるとキーフレームは約55kBになります。
- CBR(モード1)は1ワードずれた配置(w1=ビットレート, w2=初期/最小QP, w3=最大QPとI/P差, w4=P/B差, w5=オプション, w6=最大ピクチャ)です。実機では未使用です。

