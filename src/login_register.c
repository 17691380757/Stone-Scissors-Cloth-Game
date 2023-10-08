#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sqlite3.h>
#include <stdbool.h>
//数据库文件名
static const char *DB_FILE="aqlite3user.db";
//数据库连接
static sqlite3 *db;
//打开sqlite3数据库连接
void open_db()
{
	int rc =sqlite3_open(DB_FILE,&db);
	if(rc!=SQLITE_OK){
		fprintf(stderr,"无法打开数据库:%s\n",sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}
//关闭数据库连接
void close_db()
{
	int rc =sqlite3_close(db);
	if(rc!=SQLITE_OK){
		fprintf(stderr,"无法关闭数据库:%s\n",sqlite3_errmsg(db));
		exit(EXIT_FAILURE);
	}
}
//创建用户表
void creat_table()
{
	char *err_msg=0;//保存错误信息
	const char *sql="CREATE TABLE IF NOT EXISTS users("
		"id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"username TEXT UNIQUE,"
		"password TEXT);";
	int rc =sqlite3_exec(db,sql,0,0,&err_msg);
	if(rc!=SQLITE_OK){
		fprintf(stderr,"无法创建用户表：%s\n",err_msg);
		sqlite3_free(err_msg);
		exit(EXIT_FAILURE);
	}
}
//用户注册
void register_user(const char *username,const char *password){
	char *err_msg=0;
	const char *sql="INSERT INTO users(username,password) VALUES (?,?);";
	int rc=sqlite3_exec(db,sql,0,0,&err_msg);
	if(rc!=SQLITE_OK){
		fprintf(stderr,"无法插入用户信息：%s\n",err_msg);
		exit(EXIT_FAILURE);
	}
}
//用户登录
bool login(const char *username,const char *password){
	char *err_msg;
	const char *sql="SELECT * FROM users WHERE username=? AND password = ?;";
	char *sqlWithBindings=sqlite3_mprintf(sql,username,password);
	int rc=sqlite3_exec(db,sqlWithBindings,NULL,0,&err_msg);
	if(rc!=SQLITE_OK){
		fprintf(stderr,"查询失败:%s\n",err_msg);
		sqlite3_free(err_msg);
		return false;
	}
	sqlite3_free(sqlWithBindings);
	return true;
}
//测试
int main(int argc, char *argv[]) {
    open_db();
    creat_table();
    register_user("Alice", "12345");
	bool loginResult = login("Alice","12345");

    if (loginResult) {
        printf("登录成功！\n");
        // 登录成功后的操作
    } else {
        printf("登录失败！\n");
        // 登录失败后的操作
    }
    close_db();
    return 0;
}
