/*************************************************************************
    > File Name: camer.h
    > 作者:YJK 
    > Mail: 745506980@qq.com 
    > Created Time: 2020年11月18日 星期三 14时17分55秒
 ************************************************************************/

#ifndef	__CAMER_H 
#define __CAMER_H
#include<stdio.h>
#include<unistd.h>
#include<fcntl.h>
#include<stdlib.h>
#include<linux/videodev2.h>
#include<sys/ioctl.h>
#include<errno.h>                                                                                     
#include<string.h>
#include<assert.h>
#include<getopt.h>
#include<sys/stat.h>
#include<sys/mman.h>
#include<asm/types.h>
#include<linux/fb.h>

#define CLEAN(x) (memset(&(x), 0, sizeof(x)))

#define WIDTH 640

#define HEIGHT 480

typedef struct Video_Buffer{
	void * start;
	unsigned int length;
}Video_Buffer;


int ioctl_(int fd, int request, void *arg);

void sys_exit(const char *s);

int open_device(const char * device_name);

int open_file(const char * file_name);

void start_stream(void);

void end_stream(void);

int init_device(void);

int init_mmap(void);

static int read_frame(void);

int process_frame(void);

void close_mmap(void);

void close_device(void);


#endif

