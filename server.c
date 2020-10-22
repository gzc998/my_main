#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

//客户端信息结构体 
struct  clien
{
	char ip[50];  
	int  port;  
	int  clien_socket;
	pthread_t read_tid;
};  


//定义结构体数组保存客户端的链接人数  
struct  clien   all[50]; 

int  people=0; //当前在线的人数 


//发送数据给客户端 
void *write_clien(void *arg)
{	
	while(1)
	{
		printf("1.显示所有客户端信息  2.给客户端发送数据\n");
		int a=0;
		scanf("%d",&a);
		if(a == 1)
		{
			printf("_____________________\n");
			for(int i=0;i<people;i++)
			{
				printf("ip:%s,prot:%d,socket:%d\n",all[i].ip,all[i].port,all[i].clien_socket);
			}
			
			printf("_____________________\n");
		}
		
		if(a == 2)
		{
			printf("请选择需要发送的客户端\n");
			int c=0;
			scanf("%d",&c);
			
			printf("输入发送的数据\n");
			char buf[1024]={0}; 
			scanf("%s",buf);
			
			//发送
			write(all[c].clien_socket,buf,strlen(buf));
		}
	}	
}

//发送数据给客户端 
void *read_clien(void *arg)
{	
	pthread_detach(pthread_self());
	int c = arg;
	char buf[1024]={0};
	while(1)
	{
		read(all[c].clien_socket,buf,sizeof(buf));
		printf("接收到%d的数据：%s\n",all[c].clien_socket,buf);
		bzero(buf,sizeof(buf));
	}
	pthread_exit(0);
}

//记得输入服务器的端口
int main(int argc,char *argv[])
{
   //1.创建服务器通信对象 
   int tcp_socket = socket(AF_INET, SOCK_STREAM, 0); //创建TCP 通信 socket 
	   if(tcp_socket < 0)
	   {
		   perror("");
		   return -1; 
	   }
	   else
	   {
		    printf("创建服务器socket成功\n");
	   }
	 
	 
	//2.绑定服务器通信socket     
	 struct sockaddr_in  server_addr;  //服务器地址信息
     server_addr.sin_family  =  AF_INET;	 //IPV4 协议
     server_addr.sin_port    =   htons(atoi(argv[1]));  //端口号
     server_addr.sin_addr.s_addr =   INADDR_ANY;//自动绑定本地网卡地址
    int ret  = bind(tcp_socket,(struct sockaddr *)&server_addr,sizeof(struct sockaddr));
    	if(ret < 0)
    	{
    	    perror("");
    	    return 0; 
    	}
    	else
    	{
    	    printf("bind ok\n");
    	}   
	
	//3.设置服务器为监听模式 
	ret = listen(tcp_socket,5);
		if(ret < 0)
    	{
    	    perror("");
    	    return 0; 
    	}
    	else
    	{
    	    printf("listen ok\n");
    	} 


	//创建一个线程给客户端发送数据  
	
	
   pthread_t tid;
   pthread_create(&tid,NULL,write_clien,NULL);

	while(1)
	{	
		//客户端的信息结构体 
		struct sockaddr_in   clien_addr={0};
		int len = sizeof(clien_addr);

		int new_socket=accept(tcp_socket,(struct sockaddr *)&clien_addr,&len);   
		if(new_socket < 0)
		{
			perror("");
			return 0;
		}
		else
		{
			printf("有客户端链接进来  %d\n",new_socket);

			//把结构体中的信息取出来
			printf("ip:%s\n",inet_ntoa(clien_addr.sin_addr));
			printf("port:%d\n",ntohs(clien_addr.sin_port));
			printf("____________________________________\n");

			//保存客户端的信息  
			strcpy(all[people].ip,inet_ntoa(clien_addr.sin_addr));  
			all[people].port = ntohs(clien_addr.sin_port);  
			all[people].clien_socket = new_socket;
			pthread_create(&all[people].read_tid,NULL,read_clien,people);
			people++;  
		}
	   
	}
	
	 

	   
}