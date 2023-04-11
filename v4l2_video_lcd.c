#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <string.h>
#include <linux/fb.h>
#include <memory.h>

int lcdfd = 0;
int *lcdptr = NULL;
int lcd_w = 800, lcd_h = 480;

void yuyv_to_rgb(unsigned char *yuyvdata, unsigned char * rgbdata, int w, int h)
{
	int r1, g1, b1;
	int r2, g2, b2;
	for (int i = 0; i < w*h/2; i++) {
		char data[4];
		memcpy(data, yuyvdata + i*4, 4);
		unsigned char Y0 = data[0];
		unsigned char U0 = data[1];
		unsigned char Y1 = data[2];
		unsigned char V1 = data[3];
		r1 = Y0 + 1.4075*(V1 - 128);
		if (r1 > 255)
			r1 = 255;
		if (r1 < 0)
			r1 = 0;
		g1 = Y0 - 0.3455*(U0 - 128) - 0.7169*(V1 - 128);
		if (g1 > 255)
			g1 = 255;
		if (g1 < 0)
			g1 = 0;
		b1 = Y0 + 1.779*(U0 - 128);
		if (b1 > 255)
			b1 = 255;
		if (b1 < 0)
			b1 = 0;

		r2 = Y1 + 1.4075*(V1 - 128);
		if (r2 > 255)
			r2 = 255;
		if (r2 < 0)
			r2 = 0;
		g2 = Y1 - 0.3455*(U0 - 128) - 0.7169*(V1 - 128);
		if (g2 > 255)
			g2 = 255;
		if (g2 < 0)
			g2 = 0;
		b2 = Y1 + 1.779*(U0 - 128);
		if (b2 > 255)
			b2 = 255;
		if (b2 < 0)
			b2 = 0;

		rgbdata[i*6 + 0] = r1;
		rgbdata[i*6 + 1] = g1;
		rgbdata[i*6 + 2] = b1;
		rgbdata[i*6 + 3] = r2;
		rgbdata[i*6 + 4] = g2;
		rgbdata[i*6 + 5] = b2;
	}
}



void lcd_show_rgb(unsigned char *rgbdata, int w, int h)
{
	unsigned int *ptr = lcdptr;
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			memcpy(ptr + j, rgbdata + j*3, 3);
		}
		ptr += lcd_w;//偏移一行
		rgbdata += w*3;//偏移一行
	}
}

int main()
{
	lcdfd = open("/dev/fb0", O_RDWR);
	if (lcdfd < 0) {
		perror("LCD open failed:");
	}
	/*获取LCD信息*/
	struct fb_var_screeninfo info;
	int lret = ioctl(lcdfd, FBIOGET_VSCREENINFO, &info);
	if (lret < 0) {
		perror("get info failed:");
	}
	//获取虚拟LCD屏幕宽和高
	lcd_w = info.xres_virtual;
	lcd_h = info.yres_virtual;
	//开发板用物理宽和高;
	//lcd_w = info.xres;
	//lcd_h = info.yres;
	lcdptr = (int *)mmap(NULL, lcd_w*lcd_h*4,PROT_READ | PROT_WRITE, MAP_SHARED, lcdfd, 0);
	if (lcdptr == NULL) {
		perror("lcd mmap failed:");
	}
	/*打开设备*/
	int fd = open("/dev/video0", O_RDWR);
	if (fd < 0) {
		perror("Open video0:");
		return -1;
	}

	/*获取设备相关信息*/
	struct v4l2_fmtdesc v4fmt;
	v4fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4fmt.index = 1;//0 MJPEG 1 YUYV
	int ret = ioctl(fd, VIDIOC_ENUM_FMT, &v4fmt);
	if (ret < 0) {
		perror("Get inf failed:");
		return -1;
	}
	printf("index:%d\n", v4fmt.index);
	printf("flags:%d\n", v4fmt.flags);
	printf("description:%s\n", v4fmt.description);
	unsigned char *p =(unsigned char *)(&v4fmt.pixelformat);
	printf("pixelformat:%c%c%c%c\n", p[0], p[1], p[2], p[3]);
	
	/*设置视频格式*/
	struct v4l2_format vfmt;
	vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vfmt.fmt.pix.width = 640;
	vfmt.fmt.pix.height = 480;
	vfmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;//设置视频采集格式YUYV
	ret = ioctl(fd, VIDIOC_S_FMT, &vfmt);
	if (ret < 0) {
		perror("Set format failed:");
		return -1;
	}
	memset(&vfmt, 0, sizeof(vfmt));
	vfmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_G_FMT, &vfmt);
	if (ret < 0) {
		perror("Get format failed:");
		return -1;
	}
	
	if (vfmt.fmt.pix.width == 640 && vfmt.fmt.pix.height == 480 && vfmt.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
		printf("Set successful!\n");
	}
	else {
		printf("Set failed!\n");
		return -1;
	}

	/*申请内核缓冲区队列*/
	struct v4l2_requestbuffers reqbuffer;
	reqbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	reqbuffer.count = 4;
	reqbuffer.memory = V4L2_MEMORY_MMAP;//映射方式
	ret = ioctl(fd, VIDIOC_REQBUFS, &reqbuffer);
	if (ret < 0) {
		perror("Request Queue space failed:");
		return -1;
	}
	
	/*映射到用户空间*/
	struct v4l2_buffer mapbuffer;
	int i;
	unsigned char *mptr[4];
	unsigned int size[4];//存储大小，方便释放
	mapbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	for (i = 0; i < 4; i++) {
		mapbuffer.index = i;
		ret = ioctl(fd, VIDIOC_QUERYBUF, &mapbuffer);
		if (ret < 0) {
			perror("Kernel space queue failed:");
			return -1;
		}
		mptr[i] = (unsigned char *)mmap(NULL, mapbuffer.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapbuffer.m.offset);
		size[i] = mapbuffer.length;
		//使用完毕
		ret = ioctl(fd, VIDIOC_QBUF, &mapbuffer);
		if (ret < 0) {
			perror("Return failed:");
			return -1;
		}
	}

	/*开始采集数据*/
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = ioctl(fd, VIDIOC_STREAMON, &type);
	if (ret < 0) {
		perror("Start failed:");
		return -1;
	}
	//从队列中提取一帧数据
	unsigned char rgbdata[640*480*3];
	while (1) {
		struct v4l2_buffer readbuffer;
		readbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		ret = ioctl(fd, VIDIOC_DQBUF, &readbuffer);
		if (ret < 0) {
			perror("Capture failed:");
		}
		//显示在LCD上
		yuyv_to_rgb(mptr[readbuffer.index], rgbdata, 640, 480);
		lcd_show_rgb(rgbdata, 640, 480);
		//通知内核已经使用完毕
		ret = ioctl(fd, VIDIOC_QBUF, &readbuffer);
		if (ret < 0) {
			perror("return failed:");
		}
	}
	//停止采集数据
	ret = ioctl(fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0) {
		perror("Stop failed:");
		return -1;
	}

	//释放映射空间
	for (i = 0; i < 4; i++) {
		munmap(mptr[i], size[i]);
	}
	close(fd);
	return 0;
}

