/*************************************************************************
    > File Name: camer.c
    > 作者:YJK 
    > Mail: 745506980@qq.com 
    > Created Time: 2020年11月18日 星期三 14时17分50秒
 ************************************************************************/

#include"camer.h"
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

int fd;
int file_fd;
int frame_size;
static Video_Buffer * buffer = NULL;
FILE * file = NULL;
int ioctl_(int fd, int request, void *arg)
{
	int ret = 0;
	do{
		ret = ioctl(fd, request, arg);
	}while(ret == -1 && ret == EINTR);
		
}

int open_device(const char * device_name)
{
	struct stat st;
    if( -1 == stat( device_name, &st ) )
    {
        printf( "Cannot identify '%s'\n" , device_name );
        return -1;
    }

    if ( !S_ISCHR( st.st_mode ) )
    {
        printf( "%s is no device\n" , device_name );
        return -1;
    }

    fd = open(device_name, O_RDWR | O_NONBLOCK , 0);
    if ( -1 == fd )
    {
        printf( "Cannot open '%s'\n" , device_name );
        return -1;
    }
    return 0;	
}



int init_device(void)
{
	//查询设备信息
	struct v4l2_capability cap;
	
	if (ioctl_(fd, VIDIOC_QUERYCAP, &cap) == -1)
	{
		perror("VIDIOC_QUERYCAP");
		return -1;
	}
	printf("---------------------LINE:%d\n", __LINE__);
	printf("DriverName:%s\nCard Name:%s\nBus info:%s\nDriverVersion:%u.%u.%u\n",
		cap.driver,cap.card,cap.bus_info,(cap.version>>16)&0xFF,(cap.version>>8)&0xFF,(cap.version)&0xFF);	
	//选择视频输入
	struct v4l2_input input;
	CLEAN(input);
	input.index = 0;
	if ( ioctl_(fd, VIDIOC_S_INPUT,&input) == -1){
		printf("VIDIOC_S_INPUT IS ERROR! LINE:%d\n",__LINE__);
		return -1;
	}
	//设置帧格式
	struct v4l2_format fmt;
	CLEAN(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = WIDTH;
	fmt.fmt.pix.height = HEIGHT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
//	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;
	if (ioctl_(fd, VIDIOC_S_FMT, &fmt) == -1)
	{
		printf("VIDIOC_S_FMT IS ERROR! LINE:%d\n",__LINE__);
		return -1;
	}
	fmt.type = V4L2_BUF_TYPE_PRIVATE;
	if (ioctl_(fd, VIDIOC_S_FMT, &fmt) == -1){
		printf("VIDIOC_S_FMT IS ERROR! LINE:%d\n", __LINE__);
		return -1;
	}
	//查看帧格式
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if ( ioctl_(fd, VIDIOC_G_FMT, &fmt) == -1){
		printf("VIDIOC_G_FMT IS ERROR! LINE:%d\n", __LINE__);
		return -1;
	}
	printf("width:%d\nheight:%d\npixelformat:%c%c%c%c\n",
            fmt.fmt.pix.width, fmt.fmt.pix.height,
            fmt.fmt.pix.pixelformat &  0xFF,
            (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
            (fmt.fmt.pix.pixelformat >> 16) & 0xFF,
            (fmt.fmt.pix.pixelformat >> 24) & 0xFF
            );

	__u32 min = fmt.fmt.pix.width * 2;
    if ( fmt.fmt.pix.bytesperline < min )
        fmt.fmt.pix.bytesperline = min;
    min = ( unsigned int )WIDTH * HEIGHT * 3 / 2;
    if ( fmt.fmt.pix.sizeimage < min )
        fmt.fmt.pix.sizeimage = min;
    frame_size = fmt.fmt.pix.sizeimage;
	printf("After Buggy driver paranoia\n");
    printf("    >>fmt.fmt.pix.sizeimage = %d\n", fmt.fmt.pix.sizeimage);
    printf("    >>fmt.fmt.pix.bytesperline = %d\n", fmt.fmt.pix.bytesperline);
    printf("-#-#-#-#-#-#-#-#-#-#-#-#-#-\n");
    printf("\n");

	
	return 0;

}

 int init_mmap()
{
	//申请帧缓冲区
	struct v4l2_requestbuffers req;
	CLEAN(req);
	req.count = 4;
	req.memory = V4L2_MEMORY_MMAP;  //使用内存映射缓冲区
	req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//申请4个帧缓冲区，在内核空间中
	if ( ioctl_(fd, VIDIOC_REQBUFS, &req) == -1 ) 
	{
		printf("VIDIOC_REQBUFS IS ERROR! LINE:%d\n",__LINE__);
		return -1;
	}
	//获取每个帧信息，并映射到用户空间
	buffer = (Video_Buffer *)calloc(req.count, sizeof(Video_Buffer));
	if (buffer == NULL){
		printf("calloc is error! LINE:%d\n",__LINE__);
		return -1;
	}
	
	struct v4l2_buffer buf;
	int buf_index = 0;
	for (buf_index = 0; buf_index < req.count; buf_index ++)
	{
		CLEAN(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.index = buf_index;
		buf.memory = V4L2_MEMORY_MMAP;
		if (ioctl_(fd, VIDIOC_QUERYBUF, &buf) == -1) //获取每个帧缓冲区的信息 如length和offset
		{
			printf("VIDIOC_QUERYBUF IS ERROR! LINE:%d\n",__LINE__);
			return -1;
		}
		//将内核空间中的帧缓冲区映射到用户空间
		buffer[buf_index].length = buf.length;
		buffer[buf_index].start = mmap(NULL, //由内核分配映射的起始地址
									   buf.length,//长度
									   PROT_READ | PROT_WRITE, //可读写
									   MAP_SHARED,//可共享
									   fd,
									   buf.m.offset);
		if (buffer[buf_index].start == MAP_FAILED){
			printf("MAP_FAILED LINE:%d\n",__LINE__);
			return -1;
		}
		//将帧缓冲区放入视频输入队列
		if (ioctl_(fd, VIDIOC_QBUF, &buf) == -1)
		{
			printf("VIDIOC_QBUF IS ERROR! LINE:%d\n", __LINE__);
			return -1;
		}		
		printf("Frame buffer :%d   address :0x%x    length:%d\n",buf_index, (__u32)buffer[buf_index].start, buffer[buf_index].length);
	}	
}

void start_stream()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl_(fd, VIDIOC_STREAMON, &type) == -1){
		printf("VIDIOC_STREAMON IS ERROR! LINE:%d\n", __LINE__);
		exit(EXIT_FAILURE);
	}
}
void end_stream()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl_(fd, VIDIOC_STREAMOFF, &type) == -1){
		printf("VIDIOC_STREAMOFF IS ERROR! LINE:%d\n", __LINE__);
		exit(EXIT_FAILURE);
	}
}

static int read_frame()
{
	struct v4l2_buffer buf;
	int ret = 0;
	CLEAN(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;
	if (ioctl_(fd, VIDIOC_DQBUF, &buf) == -1){
		printf("VIDIOC_DQBUF! LINEL:%d\n", __LINE__);
		return -1;
	}
	ret = write(file_fd, buffer[buf.index].start ,frame_size);
	if (ret == -1)
	{
		printf("write is error !\n");
		return -1;
	}
	if (ioctl_(fd, VIDIOC_QBUF, &buf) == -1){
		printf("VIDIOC_QBUF! LINE:%d\n", __LINE__);
		return -1;
	}
	return 0;
}


int open_file(const char * file_name)
{
	
	file_fd = open(file_name, O_RDWR | O_CREAT, 0777);
	if (file_fd == -1)
	{
		printf("open file is error! LINE:%d\n", __LINE__);
		return -1;
	}
		
//	file = fopen(file_name, "wr+");
}

void close_mmap()
{
	int i = 0;
	for (i = 0; i < 4 ; i++)
	{
		munmap(buffer[i].start, buffer[i].length);
	}
	free(buffer);
}
void close_device()
{
	close(fd);
	close(file_fd);
}
int process_frame()
{
	struct timeval tvptr;
    int ret;
	tvptr.tv_usec = 0;  //等待50 us
    tvptr.tv_sec = 2;
    fd_set fdread;
    FD_ZERO(&fdread);
    FD_SET(fd, &fdread);
    ret = select(fd + 1, &fdread, NULL, NULL, &tvptr);
    if (ret == -1){
        perror("EXIT_FAILURE");
        exit(EXIT_FAILURE);
    }
	if (ret == 0){
		printf("timeout! \n");
	}
	read_frame();
}

#define DEVICE_NAME "/dev/video8"

#define FILE_NAME "./out.yuv"
int main(int argc,char *argv[])
{
	int ret = 0;
	int i;
	ret = open_device(DEVICE_NAME);
	if (ret == -1) 
		exit(EXIT_FAILURE);
	open_file(FILE_NAME);
	init_device();
	init_mmap();
	start_stream();

	for (i = 0 ; i < 600; i++)
	{
		process_frame();	
		printf("frame:%d\n",i);
	}

	end_stream();
	close_mmap();
	close_device();
	
	
	return 0;
}

