#ifndef COMMON_H
#define COMMON_H

#include <stdlib.h>
#define SAFE_FREE(p) {if(p != NULL){free(p); p = NULL;}}

#define SOCK_CLOSED        1
#define SEND_OK            0
#define SOCKET_ERR        -1
#define BIND_ERR          -2
#define LISTEN_ERR        -3
#define CONNECT_ERR       -4
#define SEND_ERR          -5
#define RECV_ERR          -6
#define COMMAND_NOT_FOUND -7
#define ARG_SIZE_OVER     -8
#define BUF_SIZE_OVER     -9
#define START_ERR         -10
#define CD_ERR            -11

#define OFFSET_STAT 0
#define OFFSET_SIZE 1
#define OFFSET_DATA sizeof(size_t) + OFFSET_SIZE

#define BUF_SIZE      4096
#define BUF_DATA_SIZE (BUF_SIZE - OFFSET_DATA)

#define STAT_ERROR  0
#define STAT_OK     1
#define STAT_START  2
#define STAT_DOING  3
#define STAT_END    4

#define CMD_NONE  0
#define CMD_CD    101
#define CMD_LS    102
#define CMD_GET   103
#define CMD_PUT   104
#define CMD_MGET  105
#define CMD_MPUT  106
#define CMD_CLOSE 107

struct request_command {
	int cmd;
	int argc;
	char **argv;
};

void free_request_command(struct request_command *req_cmd);
struct request_command* new_request_command(char *str);

int get_cmd(const char* str);
const char* cmd_to_str(int cmd);

int send_data(int sock, size_t size, void *data, int status);
int recv_data(int sock, char *buf);

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
