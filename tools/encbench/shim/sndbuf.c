// LD_PRELOAD shim for v4l2rtspserver: live555 asks for a 50 KB SO_SNDBUF on every RTSP client socket, which a 200 kB
// keyframe burst does not fit in. Raise any SO_SNDBUF request below SNDBUF_MIN (env, default 512 KB) to that value.
// The kernel clamps to net.core.wmem_max, so raise that too (/proc/sys/net/core/wmem_max).
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {

  static int (*real)(int, int, int, const void *, socklen_t);
  if(!real) real = dlsym(RTLD_NEXT, "setsockopt");
  if((level == SOL_SOCKET) && (optname == SO_SNDBUF) && optval && (optlen == sizeof(int))) {
    int want = getenv("SNDBUF_MIN") ? atoi(getenv("SNDBUF_MIN")) : 512 * 1024;
    int v = *(const int *)optval;
    if(v < want) {
      fprintf(stderr, "sndbuf fd%d: %d -> %d\n", fd, v, want);
      v = want;
      return real(fd, level, optname, &v, sizeof(v));
    }
  }
  return real(fd, level, optname, optval, optlen);
}
