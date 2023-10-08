#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#define SERVER_PORT 69
//连接服务器
int tcp_connect(const char *ip,int port)
{
	int tcp_socket = 0;
	tcp_socket = socket(AF_INET,SOCK_STREAM,0);
        if (tcp_socket < 0)
	{
		perror("Socket creation faild");
		exit(EXIT_FAILURE);

	}
	struct sockaddr_in seer;
	bzero(&seer,sizeof(seer));

	seer.sin_family = AF_INET;
	seer.sin_port = htons(port);
	seer.sin_addr.s_addr = inet_addr(ip);

	if(connect(tcp_socket,(const struct sockaddr *)&seer,sizeof(seer)) < 0)
	{
		perror("error");
		return -1;
	}

	return tcp_socket;
}
void game_user(int sockfd)
{
	int gamechoice=0;
    if (send(sockfd, "keep", sizeof("keep"), 0) < 0)
	{
		perror("send failed");
		exit(EXIT_FAILURE);
	}
	printf("开始石头剪刀布游戏！\n");
	do{
		printf("请输入你的选择：1.石头2.剪刀3.布\n");
        scanf("%d",&gamechoice);
		if(gamechoice!=1&&gamechoice!=2&&gamechoice!=3){
			printf("你的选择有误，请重新输入\n");
		}
	}while(gamechoice!=3&&gamechoice!=1&&gamechoice!=2);
	//发送游戏指令
    if (send(sockfd, &gamechoice, sizeof(gamechoice), 0) < 0)
	{
		perror("send failed");
		exit(EXIT_FAILURE);
	}
	//接收游戏指令
	char winertext[100]={'\0'};
    if (recv(sockfd, winertext, sizeof(winertext)-1, 0) < 0)
	{
		perror("Recv failed");
		exit(EXIT_FAILURE);
	}
	printf("您战胜了：%s", winertext);
}
void change_password(int sockfd)
{
	char newPassword[100]={'\0'};
	if (send(sockfd, "change", sizeof("change"), 0) < 0)
	{
		perror("send failed");
		exit(EXIT_FAILURE);
	}
	printf("请输入新的密码:\n");
    scanf("%s",newPassword);
    if (send(sockfd, newPassword, sizeof(newPassword)-1, 0) < 0) {
		perror("Send failed");
		exit(EXIT_FAILURE);
	}
    // 接收响应
	int response =-1;
	ssize_t recv_len = recv(sockfd, &response, sizeof(response), 0);
	if (recv_len < 0) {
		perror("Receive failed");
		exit(EXIT_FAILURE);
	}

	// 处理响应数据
	if(response==0)
	{
		printf("修改密码失败\n");
	}
	else if(1==response)
	{
		printf("密码修改成功\n");
	}
}
int main(int argc, const char *argv[])
{
	int sockfd=0;
	//struct sockaddr_in server_addr;
	sockfd = tcp_connect(argv[1],atoi(argv[2]));
	//构建登陆或者注册请求
	int choice=0;
	do
	{
		printf("请输入你的选择:1.注册  2.登录\n");
		scanf("%d", &choice);
		//发送登录或者注册请求
		if(choice==1||choice==2)
		{
			if (send(sockfd, &choice, sizeof(choice), 0) < 0)
			{
				perror("send failed");
				exit(EXIT_FAILURE);
			}
		}
		else{
			printf("你的输入有误,请重新输入\n");
		}
	}while(choice!=1&&choice!=2);
	
	//接收服务器响应
	char request[100] = { '\0' };
	if (recv(sockfd, request, sizeof(request)-1, 0) < 0)
	{
		perror("Recv failed");
		exit(EXIT_FAILURE);
	}
	printf("接收到服务器响应：%s\n", request);
        while (getchar() != '\n');  // 清空stdin缓冲区

	// 读取用户名
	printf("请输入用户名: \n");
        sleep(3);
	char username[100];
	fgets(username, sizeof(username), stdin);
	username[strcspn(username, "\n")] = '\0';  // 去除换行符
        
        // 发送用户名
	if (send(sockfd, username, sizeof(username)-1, 0) < 0) {
		perror("Send failed");
		exit(EXIT_FAILURE);
	}
        while (getchar() != '\n');  // 清空stdin缓冲区
	// 读取密码
	printf("请输入密码: \n");
	char password[100];
	fgets(password, sizeof(password), stdin);
	password[strcspn(password, "\n")] = '\0';  // 去除换行符

	//sleep(3);
	// 发送密码
	if (send(sockfd, password, sizeof(password)-1, 0) < 0) {
		perror("Send failed");
		exit(EXIT_FAILURE);
	}
	char response[100];

	// 接收响应
	ssize_t recv_len = recv(sockfd, response, sizeof(response) - 1, 0);
	if (recv_len < 0) {
		perror("Receive failed");
		exit(EXIT_FAILURE);
	}

	// 添加字符串结束符
	response[recv_len] = '\0';

	// 处理响应数据
	printf("收到服务器的响应: %s\n", response);
    if(strcmp("登录成功",response)==0)
	{
		char input[10];
		printf("您希望开始石头剪刀布游戏，改密还是退出？\n");
		while(1)
		{
			printf("please input 'keep' or or 'chage' or 'quit'\n");
			scanf("%s",input);
            if(strcmp(input,"keep")==0)
			{
				//继续游戏
				game_user(sockfd);
			}
			else if(strcmp(input,"quit")==0)
			{
				//退出
				printf("游戏结束\n");
				if(send(sockfd,"quit",sizeof("quit"),0)<0)
				{
					perror("Send error");
					exit(EXIT_FAILURE);
				}
				break;
			}
			else if(strcmp(input,"change")==0)
			{
				//修改密码
				change_password(sockfd);
			}
			else{
				printf("INvalid input!PLease enter 'keep' or 'change' or 'quit'.\n");
			}
		}
	//关闭套接字
	close(sockfd);
	return 0;
	}
}
