#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>

#include <assert.h>
#include <strings.h>
#include <pthread.h>
#include <semaphore.h>

#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/sysmacros.h>

#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#include <signal.h>
#include <linux/fb.h>

#include <sys/ioctl.h>
#include <sys/file.h>
#include <syslog.h>
#include <sys/resource.h>
#include <pthread.h>

#include <linux/input.h> // for touch panel

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <arpa/inet.h>

#include <netdb.h>
#include <stdlib.h>
#include "lcd.h"

bool fangda_flag = true;	//true放大适应屏幕，flase不放大图片,自动居中显示


void jpg_tra(int rgb_height,int rgb_width,char *rgb_data,char *p);
//jpg图片解码转换为bmp图片格式
void JPG2RGB(char *jpg_data, int jpg_size,
             char **rgb_data, int *rgb_width, int *rgb_height)
{
    // 解码流程准备
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    // 读取大小为 jpg_size 个字节的 jpg_data
    jpeg_mem_src(&cinfo, jpg_data, jpg_size);

    // 读取JPEG文件的头，并判断其格式是否合法
    if(jpeg_read_header(&cinfo, true) != 1)
    {
        perror("读取JPG数据失败 ");
        exit(0);
    }

    // 开始解码 JPG 数据
    jpeg_start_decompress(&cinfo);

    *rgb_width   = cinfo.output_width;
    *rgb_height  = cinfo.output_height;
    int pix_size = cinfo.output_components; // 单位：字节

    // 根据图片的尺寸大小，分配一块相应的内存 rgb_data
    // 用来存放从 jpg_data 解码出来的图像数据
    int rgb_size = *rgb_width * *rgb_height * pix_size;
    *rgb_data = calloc(1, rgb_size);

    // 循环地将图片的每一行读出并解码到 rgb_data 中
    int row_size = *rgb_width * pix_size;
    while(cinfo.output_scanline < cinfo.output_height)
    {
        unsigned char *a[1];
        a[0] = *rgb_data + (cinfo.output_scanline) * row_size;
        jpeg_read_scanlines(&cinfo, a, 1);
    }

    // 解码完了，释放相关资源
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free(jpg_data);
}


//ｊｐｇ图片显示
//char *fi为图片所在路径
void jpg_show(char *fi)
{
    // 1，打开并读取指定的JPG图片
    char *jpg_data; // 需要解码的 JPG 数据
    int   jpg_size; // 需要解码的 JPG 数据大小

    struct stat buf;
    bzero(&buf, sizeof(buf));
    stat(fi, &buf);
    jpg_size = buf.st_size;

    jpg_data = calloc(1,jpg_size);

    int fd0 = open(fi,O_RDWR);
    if(fd0 == -1)
    {
        perror("打开fd0文件失败");
        exit(0);
    }
    read(fd0,jpg_data,jpg_size);
    close(fd0);

    // 2，准备好存放解码出来的RGB数据的内存和大小
    char *rgb_data; // 解码后的 RGB 数据
   // int   rgb_size; // 解码后的 RGB 数据大小
    int rgb_width;
    int rgb_height;

    // 3，开始交给 JPG 库帮忙解码

    JPG2RGB(jpg_data,jpg_size,&rgb_data, & rgb_width,&rgb_height);

    printf("w:%d,h%d\n",rgb_width,rgb_height);

    // 4，准备好LCD设备，以及映射内存
    int fd = open("/dev/fb0",O_RDWR);
    if(fd == -1)
    {
        perror("打开文件失败");
        exit(0);
    }
    struct fb_var_screeninfo vinfo; // 显卡设备的可变属性结构体
    ioctl(fd, FBIOGET_VSCREENINFO, &vinfo); // 获取可变属性

    unsigned long VWIDTH = vinfo.xres_virtual;
    unsigned long VHEIGHT = vinfo.yres_virtual;
    unsigned long BPP = vinfo.bits_per_pixel;
    char *p = mmap(NULL, VWIDTH * VHEIGHT * BPP/8,
                   PROT_READ|PROT_WRITE,
                   MAP_SHARED, fd, 0); // 申请一块虚拟区映射内存

    char *location = p+800*480*4;
    // 5，将解码出来的 RGB 数据，妥善地放入LCD的映射内存中
	
	//ｊｐｇ图片数据转换
    jpg_tra(rgb_height,rgb_width,rgb_data,location);
	
    vinfo.xoffset = 0;
    vinfo.yoffset = 480;
    if(ioctl(fd, FB_ACTIVATE_NOW, &vinfo)) // 偏移量均置位为 480
    {
        perror("ioctl()");
    }
    ioctl(fd, FBIOPAN_DISPLAY, &vinfo); // 配置属性并扫描显示
    // 6，释放相关资源，收工！*/
    close(fd);
    //free(jpg_data);
}
/*---------------------------------
//ｊｐｇ图片数据转换
//思路：1.计算出图片比ｌｃｄ大/小的宽和高；
		2.计算出图片比ｌｃｄ大/小的（宽，高） 与 lcd的（宽，高）的倍数，就是每次重复或跳过的像素点的个数  w_t，h_t
		3.然后计算出应该在那个位置重复或跳过w_t，h_t个像素点
----------------------------------*/
			//图片高度（y）  //图片宽度（X） // 图片数据 //lcd映射内存
void jpg_tra(int rgb_height,int rgb_width,char *rgb_data,char *p)
{
    //1.图片比ｌｃｄ大/小的宽和高
    int w,h,w_s=0,h_s=0;
    h = rgb_height-480;
    w = rgb_width-800;

    int w_t = (abs(w)/800)+1;	//图片比ｌｃｄ大/小的宽 与 lcd的宽的倍数，就是每次重复或跳过的像素点的个数
    int h_t = (abs(h)/480)+1;	//图片比ｌｃｄ大/小的高 与 lcd的高的倍数，就是每次重复或跳过的像素点的个数

    int tmp_buf[800*480] = {0};//把图片数据合适地转换成lcd屏幕大小的数据
    for(int i=0; i<480; i++)
    {
        // 铺满一行
        for(int j=0; j<800; j++)
        {
            //行大小对齐，改变其分辨率，图片的宽大小转换为ｌｃｄ的宽大小
            // (j%(800/abs(w)))==0 计算出应该　跳过的或重复　的像素，然后跳过或重复该像素点
            // w_s<=abs(w) w_s为跳过或重复的像素点数，而w_s不能大于abs(w)，否则图片读取超出出错

            if( (j%(800/abs(w/w_t)))==0 && w_s<abs(w/w_t))
            {

                if(w<0)
                {
                    if(fangda_flag)
                        rgb_data -=(3*w_t);//重复上w_t个像素点
                }
                else
                    rgb_data +=(3*w_t);//跳过下w_t个像素点
                w_s++;
            }


            //将小于ｌｃｄ的图片居中显示
            if(w<0 && fangda_flag != true)
            {
                if(j<(abs(w)/2) || j>=(rgb_width+abs(w)/2) || i<(abs(h)/2) || i>=(rgb_height+abs(h)/2))
                    tmp_buf[j+i*800] = 0x00FFFFFF;
                else//改变ＲＧＢ的位置，防止图片失真
                    tmp_buf[j+i*800] = (rgb_data[3*(j-(abs(w)/2))+rgb_width*3*(i-(abs(h)/2))]<<16 | rgb_data[3*(j-(abs(w)/2))+rgb_width*3*(i-(abs(h)/2))+1]<<8 | rgb_data[3*(j-(abs(w)/2))+rgb_width*3*(i-(abs(h)/2))+2]<<0);
               // printf("j:%d , i:%d \n",j,i);
               // printf("abs(w)/2:%d,abs(h)/2:%d\n",(abs(w)/2),(abs(h)/2));
            }
            else
				//
                tmp_buf[j+i*800] = (rgb_data[3*j+rgb_width*3*i]<<16 | rgb_data[3*j+rgb_width*3*i+1]<<8 | rgb_data[3*j+rgb_width*3*i+2]<<0);

            //将像素点的数据映射到ｌｃｄ上
            //memcpy(p+4*j+offset, rgb_data+3*(j+w_t*w_s)+rgb_width*3*(i+h_t*h_s), 3);
        }
        //填充完一行后，行位置回复，重新回到图片这一行的开头
        if(w_s>0)
        {
            if(w<0)
            {
                if(fangda_flag)
                    rgb_data +=3*w_s*w_t;
            }
            else
                rgb_data -=3*w_s*w_t;
             w_s=0;
        }
        //高大小对齐，改变其分辨率，图片的高大小转换为ｌｃｄ的高大小
        if( (i%(480/abs(h/h_t)))==0 && h_s<abs(h/h_t))
        {
            if(h<0)
            {
                if(fangda_flag)
                    rgb_data -=3*rgb_width*h_t;
            }
            else
                rgb_data +=3*rgb_width*h_t;
            h_s++;
        }
    }
    memcpy(p, tmp_buf, 800*480*4);
    if(fangda_flag)
        fangda_flag = 0;
}

int main()
{
		//打开LCD屏幕
	//lcd_open("/dev/fb0");
	jpg_show("a.jpg");
}