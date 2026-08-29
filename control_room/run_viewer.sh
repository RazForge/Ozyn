#!/bin/bash
cd ~/Desktop/Github/Ozyn/control_room

# Build C camera helper
echo "Building camera helper..."
cat > /tmp/ozayn_cap.c << 'CEOF'
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>
int main(void) {
    int fd = open("/dev/video0", O_RDWR);
    if (fd < 0) { fprintf(stderr, "Cannot open camera\n"); return 1; }
    struct v4l2_format fmt = {0};
    fmt.type=1; fmt.fmt.pix.width=640; fmt.fmt.pix.height=480;
    fmt.fmt.pix.pixelformat=0x56595559; fmt.fmt.pix.field=1;
    ioctl(fd, VIDIOC_S_FMT, &fmt);
    struct v4l2_requestbuffers req={0}; req.count=1; req.type=1; req.memory=1;
    ioctl(fd, 0xc0145608, &req);
    struct v4l2_buffer buf={0}; buf.type=1; buf.memory=1;
    ioctl(fd, 0x80445609, &buf);
    void *m=mmap(0,buf.length,1,2,fd,buf.m.offset);
    int t=1; ioctl(fd,0x40045612,&t);
    fprintf(stderr, "CAMERA OK\n");
    while(1){ioctl(fd,0x8044560f,&buf);fd_set f;FD_ZERO(&f);FD_SET(fd,&f);
    struct timeval tv={1,0};if(select(fd+1,&f,0,0,&tv)<=0)continue;
    if(ioctl(fd,0x80445611,&buf)<0)continue;putchar('F');fflush(stdout);
    fwrite(m,1,640*480*2,stdout);fflush(stdout);}
    return 0;
}
CEOF
gcc -O2 -o /tmp/ozayn_cap /tmp/ozayn_cap.c

echo "Opening camera window..."
python3 live_feed.py
