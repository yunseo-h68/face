#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#define SAFE_FREE(p) {if(p != NULL){free(p); p = NULL;}}

#define SOCK_CLOSED        2
#define SEND_OK            1
#define OK                 0
#define ERR               -1
#define SOCKET_ERR        -2
#define BIND_ERR          -3
#define LISTEN_ERR        -4
#define CONNECT_ERR       -5
#define SEND_ERR          -6
#define RECV_ERR          -7
#define COMMAND_NOT_FOUND -8
#define ARG_SIZE_OVER     -9
#define BUF_SIZE_OVER     -10
#define START_ERR         -11
#define CD_ERR            -12

#define BUF_SIZE      4096
#define ARG_MAX_SIZE  100
#define ARG_MAX_COUNT 1024

#define STAT_ERROR  0
#define STAT_OK     1
#define STAT_START  2
#define STAT_DOING  3
#define STAT_END    4

#define CMD_NONE      0
#define CMD_START_NUM 100
#define CMD_CD        CMD_START_NUM + 0
#define CMD_LS        CMD_START_NUM + 1
#define CMD_GET       CMD_START_NUM + 2
#define CMD_PUT       CMD_START_NUM + 3
#define CMD_MGET      CMD_START_NUM + 4
#define CMD_MPUT      CMD_START_NUM + 5
#define CMD_CLOSE     CMD_START_NUM + 6

struct request_command {
	int cmd;
	int argc;
	char argv[ARG_MAX_COUNT][ARG_MAX_SIZE];
};

struct packet {
	int status;
	size_t size;
	char data[BUF_SIZE];
};

typedef int(*PROCESS_CMD_FUNC)(int, struct request_command*);

struct request_command* new_request_command(char *str);

int get_cmd(const char* str);
const char* cmd_to_str(int cmd);

int send_data(int sock, size_t size, void *data, int status);
int recv_data(int sock, struct packet *pack);

off_t recv_off_t(int sock);
int recv_int(int sock);
char recv_char(int sock);
char recv_stat(int sock);
char* recv_str(int sock);

char get_packet_stat(char *packet);
size_t get_packet_size(char *packet);

char* get_last_path(char* path);
int file_upload(int sock, char *path);
int file_download(int sock, char *path);

int open_server(int port);
int open_connection(const char *host, int port);

int err_print(const char* message);
int err_handling(int err_chk, const int err_num, const char* message);
int err_panic(const char* message);

int cmd_cd(const char *base_path, const char *path, char **work_path, int is_admin);
char* cmd_ls(const char* path, int is_all);

#endif
