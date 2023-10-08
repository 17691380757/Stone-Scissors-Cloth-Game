#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<sys/types.h>
#include<sqlite3.h>
#include<stdbool.h>
#include<sys/wait.h>
#include<unistd.h>
#include <arpa/inet.h>
#include <assert.h>
#include <pthread.h>
#define SERVER_PORT 69
#define MAX_BUFFER_SIZE 1024
#include <errno.h>
//数据库文件名
static const char *DB_FILE="../data/aqlite3user.db";
//玩家游戏队列
// 定义队列节点结构体
typedef struct QueueNode {
	char username[100];
	struct QueueNode* next;
}QueueNode;

// 定义队列结构体
typedef struct Queue {
	QueueNode* head;
	QueueNode* tail;
}Queue;
//创建结构体封装线程函数的参数
typedef struct {
	int newfd;
	Queue* Stone;
	Queue* Scissor;
	Queue* Cloth;
} ThreadArgs;
// 定义互斥锁和队列
//pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
// 创建队列
void QueueInit(Queue* pq) {
	assert(pq);
	pq->head = pq->tail = NULL;
}
// 添加节点到队列
// 入队列
void EnQueue(Queue* pq, const char* username) {
	assert(pq);
	QueueNode* new_node = (QueueNode*)malloc(sizeof(QueueNode));
	strcpy(new_node->username, username);
	new_node->next = NULL;

	if (pq->tail == NULL) {
		pq->head = pq->tail = new_node;
	} else {
		pq->tail->next = new_node;
		pq->tail = new_node;
	}
}

//遍历队列并拼接用户名
char* ConcatUsernames(Queue* pq)
{
	assert(pq);
	int total_length=0;
	QueueNode* temp=pq->head;
	//计算所需长度
	while(temp!=NULL)
	{
		total_length+=strlen(temp->username)+1;
		temp=temp->next;
	}
	//分配内存空间并拼接用户名
	char* result=(char*)malloc(sizeof(char)*(total_length));
	result[0]='\0';//确保字符串以\0结尾
	temp=pq->head;
	while(temp!=NULL)
	{
		strcat(result,temp->username);
		if(temp->next!=NULL)

		{
			strcat(result,",");
		}
		temp=temp->next;
	}
	return result;
}
//清空队列
void ClearQueue(Queue* pq) {
	assert(pq);

	QueueNode* current = pq->head;
	while (current != NULL) {
		QueueNode* temp = current;
		current = current->next;
		free(temp);  // 释放节点内存
	}

	pq->head = pq->tail = NULL;  // 将头指针和尾指针置空
}
//销毁队列
void DestoryQueue(Queue* pq) {
	assert(pq);

	// 使用辅助指针遍历队列
	QueueNode* pNode = pq->head;
	while (pNode != NULL) {
		// 将队列头部节点删除，并释放其内存
		pq->head = pNode->next;
		free(pNode);
		pNode = pq->head;
	}

	// 重置队列的头部和尾部节点指针
	pq->tail = NULL;
}

//数据库连接
static sqlite3 *db;
void open_db()
{
	int rc = sqlite3_open(DB_FILE, &db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "无法打开数据库:%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}
//关闭数据库连接
void close_db()
{
	int rc = sqlite3_close(db);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "无法关闭数据库:%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}
//创建用户表
void creat_table()
{
	char* err_msg = 0;//保存错误信息
	const char* sql = "CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT UNIQUE,"
		"password TEXT,"
		"winNumbers INT DEFAULT 0);";
	int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "无法创建用户表：%s\n", err_msg);
		sqlite3_free(err_msg);
		exit(EXIT_FAILURE);
	}
}
//根据用户名得到云端的游戏数据
int getWinnumberByUsername(const char *username);
int getWinnumberByUsername(const char* username) {
	const char* sql = "SELECT winNumbers FROM users WHERE username=?";
	sqlite3_stmt* stmt;
	int rc, winNumbers;

	rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "查询准备失败: %s\n", sqlite3_errmsg(db));
		return 0; // 或者返回适当的错误代码
	}

	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		winNumbers = sqlite3_column_int(stmt, 0);
	} else if (rc == SQLITE_DONE) {
		// 没有找到对应的记录
		winNumbers = 0; // 或者返回适当的默认值
	} else {
		fprintf(stderr, "查询执行失败: %s\n", sqlite3_errmsg(db));
		winNumbers = 0; // 或者返回适当的错误代码
	}

	sqlite3_finalize(stmt);
	return winNumbers;
}
// 更新胜利次数的函数
void updateWinNumbers(const char* username) {
	// 加一胜利次数
	const char* update = "UPDATE users SET winNumbers = winNumbers + 1 WHERE username = ?";
	sqlite3_stmt* stmt;

	if (sqlite3_prepare_v2(db, update, -1, &stmt, NULL) != SQLITE_OK) {
		fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
		return;
	}

	sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);

	if (sqlite3_step(stmt) != SQLITE_DONE) {
		fprintf(stderr, "Failed to update winNumbers: %s\n", sqlite3_errmsg(db));
		return;
	}

	sqlite3_finalize(stmt);
}
// 改密函数
int changePassword(const char* username,const char* newPassword) {
	char sql[100]; // SQL 语句缓冲区，适当调整大小
	sqlite3_stmt* stmt;
	const char* tail;

	// 构建 SQL 语句
	sprintf(sql, "UPDATE users SET password='%s' WHERE username='%s'", newPassword, username);

	// 编译 SQL 语句
	int rc = sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, &tail);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL 语句编译错误： %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}

	// 执行 SQL 语句
	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "SQL 语句执行错误： %s\n", sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		return 0;
	}

	sqlite3_finalize(stmt); // 释放语句资源
	printf("密码已成功更新。\n");
	return 1;
}
//用户注册
void register_user(const char* username, const char* password) {
	const char* sql = "INSERT INTO users(username, password) VALUES (?, ?);";

	sqlite3_stmt* stmt;
	int rc = sqlite3_prepare_v2(db, sql, strlen(sql), &stmt, NULL);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "错误：无法准备 SQL 语句：%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "错误：无法绑定用户名：%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_bind_text(stmt, 2, password, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "错误：无法绑定密码：%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		fprintf(stderr, "错误：无法插入用户信息：%s\n", sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}

	sqlite3_finalize(stmt);
}
//用户登录
bool login(const char* username, const char* password) {
	char* err_msg;
	const char* sql = "SELECT * FROM users WHERE username=? AND password = ?;";
	char* sqlWithBindings = sqlite3_mprintf(sql, username, password);
	int rc = sqlite3_exec(db, sqlWithBindings, NULL, 0, &err_msg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "查询失败:%s\n", err_msg);
		sqlite3_free(err_msg);
		return false;
	}
	sqlite3_free(sqlWithBindings);
	return true;
}

//按照玩家选择插入队列
static int gamePlayerNumbers=0;
int processChoice(int choice,Queue* Stone,Queue* Scissor,Queue* Cloth,const char * username) {

	switch (choice) {
	case 1:
		//石头队列入队
		EnQueue(Stone,username);
		gamePlayerNumbers++;
		break;
	case 2:
		// 剪刀队列入队
		EnQueue(Scissor,username);
		gamePlayerNumbers++;
		break;
	case 3:
		// 布队列入队
		EnQueue(Cloth,username);
		gamePlayerNumbers++;
		break;
	default:
		printf("选择无效\n");
		// 处理无效选择的逻辑
		break;
	}

	return gamePlayerNumbers;
}
//与客户端通信的函数
int tcp_com(int client_sock, Queue* Stone, Queue* Scissor, Queue* Cloth)
{
	char buffer[MAX_BUFFER_SIZE];
	int choice = 0;
	//读取客户端请求
	ssize_t received_bytes = recv(client_sock, &choice, sizeof(choice), 0);
	if (received_bytes < 0) {
		perror("Receive failed");
		exit(EXIT_FAILURE);
	}
	else if (received_bytes == 0) {
		printf("Server closed the connection\n");
	}

	//处理登录或者注册请求
		// 发送请求
	char request[] = "请输入用户名和密码";
	if (send(client_sock, request, strlen(request), 0) < 0) {
		perror("Send failed");
		exit(EXIT_FAILURE);
	}

	// 接收用户名
	char username[100];
	received_bytes = recv(client_sock, username, sizeof(username) - 1, 0);
	if (received_bytes < 0) {
		perror("Receive failed");
		exit(EXIT_FAILURE);
	}
	else if (received_bytes == 0) {
		printf("Client closed the connection\n");
	}
	else {
		username[received_bytes] = '\0';
		printf("Received username from client: %s\n", username);
	}

	// 接收密码
	char password[100];
	received_bytes = recv(client_sock, password, sizeof(password) - 1, 0);
	if (received_bytes < 0) {
		perror("Receive failed");
		exit(EXIT_FAILURE);
	}
	else if (received_bytes == 0) {
		printf("Client closed the connection\n");
	}
	else {
		password[received_bytes] = '\0';
		printf("Received password from client: %s\n", password);
	}
	if (choice == 1) {
		register_user(username, password);
		printf("注册成功\n");
		if (send(client_sock, "注册成功", strlen("注册成功"), 0) < 0) {
			perror("Send faild");
			exit(EXIT_FAILURE);

		}
	}
	if (choice == 2) {
		bool loginResult = login(username, password);
		if (loginResult) {
			printf("登录成功！\n");
			int winNumbers = getWinnumberByUsername(username);//获取云端玩家数据
			printf("该玩家目前胜利次数：%d\n", winNumbers);
			// 登录成功后的操作
			if (send(client_sock, "登录成功", strlen("注册成功"), 0) < 0) {
				perror("Send faild");
				exit(EXIT_FAILURE);

			}
            char loginChoice[10]={'\0'};
			while (1)
			{
				
				if (recv(client_sock, loginChoice, sizeof(loginChoice) - 1, 0) < 0)
				{
					perror("Recv faild");
					exit(EXIT_FAILURE);
				}
				else
				{
					printf("%s\n", loginChoice);
					if (strcmp(loginChoice, "keep") == 0)
					{
						//游戏函数
						//接收游戏数据
						if (recv(client_sock, &choice, sizeof(choice), 0) < 0)
						{
							perror("Recv faild");
							exit(EXIT_FAILURE);
						}
						//将接收到的玩家数据存入不同的队列

						gamePlayerNumbers = processChoice(choice, Stone, Scissor, Cloth, username);
						while (1)
						{
							if (gamePlayerNumbers == 2)
							{
								Queue* queues[] = { NULL,Scissor,Cloth,Stone };
								//得到击败的玩家名单
								char* concatenated = ConcatUsernames(queues[choice]);
								// 检查 concatenated 是否不为空，如果不为空则调用函数更新胜利次数
								if (concatenated[0] != '\0') {
									updateWinNumbers(username);
								}
								//拼接胜利次数的字符串
								char str[200];
								winNumbers = getWinnumberByUsername(username);//获取云端玩家数据
								sprintf(str, "%s\t您的胜利次数为：%d\n", concatenated, winNumbers);
								if (send(client_sock, str, sizeof(str), 0) < 0)
								{
									perror("Recv faild");
									exit(EXIT_FAILURE);
								}

								break;
							}
							else {
								sleep(1);//等待一秒再次检验
							}
							//ClearQueue(Cloth);
							//ClearQueue(Scissor);
							//ClearQueue(Stone);
						}
						//ClearQueue(Cloth);
						//ClearQueue(Scissor);
						//ClearQueue(Stone);
					}
					else if(strcmp(loginChoice, "change") == 0)
					{
						//改密
						char new_password[100] = { '\0' };
						if (recv(client_sock, new_password, sizeof(new_password) - 1, 0) < 0)
						{
							perror("Recv newPassword faild");
							exit(EXIT_FAILURE);
						}
						printf("接收到新密码%s\n", new_password);
						int change_result = changePassword(username, new_password);
						if (send(client_sock, &change_result, sizeof(change_result), 0) < 0)
						{
							perror("send failed");
							exit(EXIT_FAILURE);
						}

					}
					else if (strcmp(loginChoice, "quit") == 0)
					{
						//表示某玩家退出
						printf("玩家%s以退出\n", username);
						return-1;
					}
				}
			}

		}
		else {
			printf("登录失败！\n");
			// 登录失败后的操作
			if (send(client_sock, "登陆失败", strlen("注册成功"), 0) < 0) {
				perror("Send faild");
				exit(EXIT_FAILURE);

			}
		}
	}

	//关闭客户端套接字
	//close(client_sock);
}
int tcp_server(const char* ip, int port)
{
	//1.创建tcp_socket对象 socket()
	//2.设置自己的地址 struct sockaddr_in
	//3.绑定自己的地址 bind()
	//4.监听是否有人连接 listen()
	//
	//1.创建tcp_socket对象 socket()
	int listenfd = 0;
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenfd < 0)
	{
		perror("socket error");
		return -1;
	}
	printf("socket ok\n");
	//2.设置自己的地址 struct sockaddr_in
	struct sockaddr_in myAddr;
	bzero(&myAddr, sizeof(myAddr));
	myAddr.sin_family = AF_INET; //地址协议族
	myAddr.sin_port = htons(port); //端口号
	myAddr.sin_addr.s_addr = inet_addr(ip);//ip地址
	//3.绑定自己的地址 bind()
	if (bind(listenfd, (const struct sockaddr*)&myAddr, sizeof(myAddr)) < 0)
	{
		perror("bind error");
		return -1;
	}
	printf("bind ok\n");
	//4.监听是否有人连接 listen()
	if (listen(listenfd, 5) < 0)
	{
		perror("listen error");
		return -1;
	}
	printf("listen ok\n");
	return listenfd;
}
// 信号处理函数
void handler(int sig)
{
	while(waitpid(-1,NULL,WNOHANG)>0);
}
//线程处理函数
void * childThread(void * arg)
{
	ThreadArgs* args = (ThreadArgs*)arg;
	int newfd = args->newfd;
	Queue* Stone = args->Stone;
	Queue* Scissor = args->Scissor;
	Queue* Cloth = args->Cloth;
	//3.跟客户端进行通信
	while(1)
	{
		int n=tcp_com(newfd,Stone,Scissor,Cloth);
		if(n<0)
		{
			printf("通信结束\n");
			close(newfd);
			break;
		}
	}
}
int main(int argc, const char* argv[])
{
	if (argc < 3)
	{
		printf("please input appname ip port\n");
		return -1;
	}
	//连接数据库
	open_db();
	creat_table();
	//1.调用tcp_server监听是否有人连接
	int listenfd = 0;
	listenfd = tcp_server(argv[1], atoi(argv[2]));
	//2.接收连接
	int newfd = 0;
	//不获得对方的地址
	//newfd=accept(listenfd,NULL,NULL);
	//获得客户端的地址
	struct sockaddr_in client;

	//创建游戏队列
	Queue Stone,Scissor,Cloth;
	QueueInit(&Stone);
	QueueInit(&Scissor);
	QueueInit(&Cloth);


	while(1)
	{
		bzero(&client,sizeof(client));
		int len=sizeof(client);
		//当连接成功的那瞬间，将客户端的信息保存到该地址上.
		newfd=accept(listenfd,(struct sockaddr *)&client,&len);
		if(newfd<0)
		{
			perror("accept error");
			return -1;
		}
		printf("accept ok\n");
		printf("ip=%s\tport=%d\n",inet_ntoa(client.sin_addr),ntohs(client.sin_port));
		//创建子线程和客户端进行通信
		pthread_t pid;

		// 主函数中创建子线程的代码
		ThreadArgs threadArgs;
		threadArgs.newfd = newfd;
		threadArgs.Stone = &Stone;
		threadArgs.Scissor = &Scissor;
		threadArgs.Cloth = &Cloth;
		if (pthread_create(&pid, NULL, childThread, (void*)&threadArgs) != 0) {
			fprintf(stderr, "Failed to pthread_create(): %s\n", strerror(errno));
			close(newfd);
		}
		// 等待子线程结束
		//pthread_join(pid, NULL);
	}

	//销毁队列
	DestoryQueue(&Stone);
	DestoryQueue(&Scissor);
	DestoryQueue(&Cloth);
	//关闭服务器套接字
	//关闭socket对象
	close(listenfd);
	close_db();
	return 0;
}
