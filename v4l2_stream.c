#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<fcntl.h>
#include<linux/videodev2.h>
#include<string.h>
#include<errno.h>
#include<sys/mman.h>
#include<SDL2/SDL.h>
#include<assert.h>
 
#define LOAD_BGRA  0
#define LOAD_RGB24  0
#define LOAD_BGB24  0
#define LOAD_YUYV422P 1
 
 
#define PIXEL_W 640
#define PIXEL_H 480
#define SCREEN_W 640
#define SCREEN_H 480
 
#if LOAD_BGRA 
#define BPP 32
#elif LOAD_RGB24 | LOAD_BRG24
#define BPP 24
#elif LOAD_YUYV422P
#define BPP 16
#endif
 
 
int screen_w=SCREEN_W;
int screen_h=SCREEN_H;
 
const int pixel_w=PIXEL_W;
const int pixel_h=PIXEL_H;
 
unsigned char buffer[PIXEL_W*PIXEL_H*BPP/8];
 
unsigned char buffer_convert[PIXEL_W*PIXEL_H*4];
 
 
struct buffers
{
	void* start;
	unsigned int length;
}*buffers;
 
 
 
 
 
int main()
{
 
	int fd=open("/dev/video0",O_RDWR);
	if(fd<0)
	{
		printf("open failure\n");
		return -1;
	}
 
	int ret=-1;
	struct v4l2_capability cap;
	ret=ioctl(fd,VIDIOC_QUERYCAP,&cap);
	if(ret<0)
	{
		perror("ioctl");
		return -1;
	}
 
	if(!(cap.capabilities & V4L2_BUF_TYPE_VIDEO_CAPTURE))
	{
		printf("not support video capture\n");
		return -1;
	}
 
 
	struct v4l2_fmtdesc fmtdesc;
	fmtdesc.index=0;
 
	fmtdesc.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while(ioctl(fd,VIDIOC_ENUM_FMT,&fmtdesc)!=-1)
	{
		printf("%d\t%d\n",fmtdesc.index+1,fmtdesc.type);
	    fmtdesc.index++;
	}
 
	struct v4l2_requestbuffers req;
	req.count=4;
	req.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory=V4L2_MEMORY_MMAP;
 
	ret=ioctl(fd,VIDIOC_REQBUFS,&req);
	if(ret<0)
	{
		printf("out of memory\n");
		return -1;
	}
	
 
	buffers=(struct buffers*)calloc(req.count,sizeof(*buffers));
	assert(buffers!=NULL);
 
	struct v4l2_buffer buf;
	for(int n_buffers=0;n_buffers < req.count;n_buffers++)
  	{
		memset(&buf,0,sizeof(buf));
		buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory=V4L2_MEMORY_MMAP;
		buf.index=n_buffers;
 
		if(ioctl(fd,VIDIOC_QUERYBUF,&buf)==-1)
 		{
			printf(" qurey failure\n");
			return -1;
		}
 
		buffers[n_buffers].length=buf.length;
		buffers[n_buffers].start=mmap(NULL,buf.length,PROT_READ | PROT_WRITE,MAP_SHARED,fd,buf.m.offset);
		if(MAP_FAILED==buffers[n_buffers].start)
		{
			perror("mmap");
			return -1;
		}
 
	}
		//capture
		unsigned int i=0;
		enum v4l2_buf_type type;
		for(i=0;i<4;i++)
 		{
			buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
			buf.memory=V4L2_MEMORY_MMAP;
			buf.index=i;
 
			if(ret=ioctl(fd,VIDIOC_QBUF,&buf)<0)
  			{
				perror("qbuf");
				return -1;
			}
		}
			type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
			ret=ioctl(fd,VIDIOC_STREAMON,&type);
				if(ret<0)
				{
					perror("streamon");
					return -1;
				}
 
 
			//sdl broatcast
			if(!(SDL_Init(SDL_INIT_VIDEO)))
			{ 
				printf(" sdl_init failure\n");
				return -1;
			}
            
			SDL_Window* screen=SDL_CreateWindow("simple sdl player",
					SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,/***X,Y postion            ******/ screen_w,screen_h,SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
 
			if(screen==NULL)
			{
				printf(" sdl create window failure\n");
				return -1;
			}
 
			SDL_Renderer*  sdlRenderer=SDL_CreateRenderer(screen,-1,0);
 
			Uint32 pixformat=0;
#if LOAD_BGRA
			pixformat=SDL_PIXFORMAT_ARGB888;
#elif LOAD_BGR24
			pixformat=SDL_PIXFORMAT_BGR888;
#elif LOAD_RGB24
		    pixformat=SDL_PIXFORMAT_RGB888;
#elif LOAD_YUV422P
			pixformat=SDL_PIXFORMAT_YUY2;
#endif
			SDL_Texture* texture=SDL_CreateTexture(sdlRenderer,pixformat,
					SDL_TEXTUREACCESS_STREAMING,pixel_w,pixel_h);
 
			SDL_Rect sdlRect;
		//struct v4l2_buffer buf;
			while(1)
			{
				buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
				buf.memory=V4L2_MEMORY_MMAP;
 
				ret=ioctl(fd,VIDIOC_DQBUF,&buf);
				if(ret<0)
  				{
					perror("v4l2_buf");
					return -1;
				}
                
				/**************programe process*****************/
 
#if LOAD_BGRA
				SDL_UpdateTexture(sdlTexture,NULL,buffer,pixel_w*4);
#elif LOAD_BGR24 | LOAD_RGB24
				convert _24to32(buffer,buffer_convert,pixel_w,pixel_h);
				SDL_UpdateTexture(sdlTexture,NULL,buffer_convert,pixel_w*4);
#elif LOAD_YUV422p
				SDL_UpdateTexture(sdlTexture,NULL,buffer[buf.index].start,pixel_w*2);
#endif
 
 
				sdlRect.x=0;
				sdlRect.y=0;
				sdlRect.w=screen_w;
				sdlRect.h=screen_h;
				SDL_RenderPresent(sdlRenderer);
				SDL_Delay(20);
 
				ret=ioctl(fd,VIDIOC_QBUF,&buf);
				if(ret<0)
				{
					perror("ioctl");
					return -1;
				}
			}
				close(fd);
				return -1;
 
}