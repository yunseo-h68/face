#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"

/* 동적할당된 request_command 구조체의 메모리를 해제한다. */
void free_request_command(struct request_command *req_cmd)
{
	int i = 0;
	if (req_cmd == NULL) {
		return;
	}
	for (i = 0; i < req_cmd->argc; i++) {
		SAFE_FREE(req_cmd->argv[i]);
	}
	SAFE_FREE(req_cmd->argv);
	SAFE_FREE(req_cmd);
}


/* 문자열로 된 명령을 request_command 구조체로 변환한다. */
struct request_command* new_request_command(char *str)
{
	int i = 0;
	char *temp_ptr = NULL;
	char *command = NULL;
	char buf[BUFSIZ] = {0,};
	size_t data_size = 0;
	struct request_command* req_cmd = (struct request_command*)malloc(sizeof(struct request_command));
	strncpy(buf, str, BUFSIZ);

	// 명령 코드
	command = strtok(buf, " ");
	req_cmd->cmd = get_cmd(command);

	// 인자의 개수
	temp_ptr = strtok(NULL, " ");
	for (i = 0; temp_ptr != NULL; i++) {
		if (temp_ptr != NULL && strlen(temp_ptr) + 1 > BUF_DATA_SIZE) {
			// 인자의 길이가 너무 길 경우 오류
			req_cmd->cmd = ARG_SIZE_OVER;
			return req_cmd;
		}
		temp_ptr = strtok(NULL, " ");
	}
	req_cmd->argc = i;

	// 인자 세팅
	req_cmd->argv = (char **)malloc(sizeof(char *) * req_cmd->argc);
	strncpy(buf, str, BUFSIZ);
	temp_ptr = strtok(buf, " ");
	temp_ptr = strtok(NULL, " ");
	for (i = 0; temp_ptr != NULL; i++) {
		data_size = strlen(temp_ptr) + 1;
		req_cmd->argv[i] = (char *)malloc(data_size);
		strncpy(req_cmd->argv[i], temp_ptr, data_size);
		temp_ptr = strtok(NULL, " ");
	}

	// 결과 반환
	return req_cmd;
}

/* 문자열로 된 명령의 상수값을 반환한다. */
int get_cmd(const char *str)
{
	if (!strcmp(str, "cd")) return CMD_CD;
	else if (!strcmp(str, "ls")) return CMD_LS;
	else if (!strcmp(str, "get")) return CMD_GET;
	else if (!strcmp(str, "put")) return CMD_PUT;
	else if (!strcmp(str, "mget")) return CMD_MGET;
	else if (!strcmp(str, "mput")) return CMD_MPUT;
	else if (!strcmp(str, "close") || 
			!strcmp(str, "quit") ||
			!strcmp(str, "exit")) return CMD_CLOSE;
	return CMD_NONE;
}

/* 명령어 상수를 명령어 문자열로 변환한다. */
const char* cmd_to_str(int cmd)
{
	switch (cmd) {
		case CMD_CLOSE:
			return "close";
		case CMD_CD:
			return "cd";
		case CMD_LS:
			return "ls";
		case CMD_GET:
			return "get";
		case CMD_PUT:
			return "put";
		case CMD_MGET:
			return "mget";
		case CMD_MPUT:
			return "mput";
	}
	return "none";
}

/* Data를 전송한다. */
int send_data(int sock, size_t size, void *data, int status)
{
	char buf[BUF_SIZE] = {0,};

	if (size > BUF_DATA_SIZE) {
		return BUF_SIZE_OVER;
	}

	buf[OFFSET_STAT] = status;
	memcpy((buf + OFFSET_SIZE), &size, sizeof(size_t));
	if (data != NULL) {
		memcpy((buf + OFFSET_DATA), data, size);
	}
	if (send(sock, buf, BUF_SIZE, 0) == -1) {
		return SEND_ERR;
	}
	return SEND_OK;
}

/* Data를 받는다. */
int recv_data(int sock, char *buf)
{
	if (recv(sock, buf, BUF_SIZE, 0) == -1 || buf[OFFSET_STAT] == STAT_ERROR) {
		return -1;
	}
	return 0;
}

/* off_t형 Data를 받는다. */
off_t recv_off_t(int sock)
{
	off_t data = 0;
	char buf[BUF_SIZE] = {0,};
	if (recv_data(sock, buf) == -1) {
		return 0;
	}
	memcpy(&data, (buf + OFFSET_DATA), sizeof(off_t));
	return data;
}

/* int형 Data를 받는다. */
int recv_int(int sock)
{
	int data = 0;
	char buf[BUF_SIZE] = {0,};
	if (recv_data(sock, buf) == -1) {
		return -1;
	}
	memcpy(&data, (buf + OFFSET_DATA), sizeof(int));
	return data;
}

/* char형 Data를 받는다. */
char recv_char(int sock)
{
	char buf[BUF_SIZE] = {0, };
	if (recv_data(sock, buf) == -1) {
		return -1;
	}
	return buf[OFFSET_DATA];
}

/* 문자열 Data를 받는다. */
char* recv_str(int sock)
{
	size_t size = 0;
	char *data = NULL;
	char buf[BUF_SIZE] = {0,};
	if (recv_data(sock, buf) == -1) {
		return NULL;
	}
	size = get_packet_size(buf);
	data = (char*)malloc(size);
	memcpy(data, (buf + OFFSET_DATA), size);
	return data;
}

/* 상태값만 가져온다. */
char recv_stat(int sock)
{
	char buf[BUF_SIZE] = {0, };
	if (recv_data(sock, buf) == -1) {
		return STAT_ERROR;
	}
	return get_packet_stat(buf);
}

/* 패킷에서 status 값을 가져온다. */
char get_packet_stat(char *packet)
{
	return packet[OFFSET_STAT];
}

/* 패킷에서 데이터의 길이 값을 가져온다. */
size_t get_packet_size(char *packet)
{
	size_t size = 0;
	memcpy(&size, (packet + OFFSET_SIZE), sizeof(size_t));
	return size;
}

/* 경로에서 최하위 경로의 시작 주소를 반환한다.  */
/* ex) ~/src 에서 최하위 경로 src                */
/*     ~/test/test.txt 에서 최하위 경로 test.txt */
char* get_last_path(char* path)
{
	unsigned int i = 0;
	for (i = strlen(path) - 1; i > 0; i--) {
		if (path[i] == '/' && i + 1 <= strlen(path) - 1) {
			return &path[i+1];
		}
	}
	return path;
}

/* 파일을 업로드한다. */
int file_upload(int sock, char *path)
{
	int fd = 0;
	ssize_t len = 0;
	size_t data_size = 0;
	char buf[BUF_SIZE] = {0,};
	struct stat file_stat;

	if (sock <= 0 || path == NULL) {
		return -1;
	}

	// 파일을 연다.
	fd = open(path, O_RDONLY);
	if (fd == -1 || fstat(fd, &file_stat) == -1) {
		// 만약 파일을 열어 파일 정보를 읽는 데 실패한다면
		// 사이즈를 0으로 전송하고 종료
		if (send_data(sock, data_size, 0, STAT_OK) == SEND_ERR) {
			return -1;
		}
		return -1;
	}
	
	// 사이즈 전송
	data_size = sizeof(off_t);
	printf("... file size : %ld\n", file_stat.st_size);
	if (send_data(sock, data_size, &file_stat.st_size, STAT_OK) == SEND_ERR) {
		return -1;
	}

	// 파일 이름 전송
	data_size = strlen(get_last_path(path)) + 1;
	printf("... file_name : %s\n", get_last_path(path));
	if (send_data(sock, data_size, get_last_path(path), STAT_OK) == SEND_ERR) {
		return -1;
	}

	puts("... Uploading file");
	// 전송 시작
	while ((len = read(fd, buf, BUF_SIZE)) > 0) {
		if (send(sock, buf, len, 0) < 0) {
			err_print("파일 전송 중 오류");
			break;
		}
	}

	// 정상 수신 여부 수신
	if (recv_stat(sock) != STAT_OK) {
		return -1;
	}
	puts("File upload complete");

	// 파일을 닫는다.
	close(fd);
	return 0;
}

/* 파일을 다운로드한다. */
int file_download(int sock, char *path)
{
	int fd = 0;
	ssize_t len = 0;
	off_t all_len = 0;
	off_t file_size = 0;
	char buf[BUF_SIZE] = {0, };
	char *file_name = NULL;
	char *file_path = NULL;

	if (sock <= 0) {
		return -1;
	}

	// 사이즈 받기
	if ((file_size = recv_off_t(sock)) == 0) {
		err_print("파일 정보를 읽어오는 데 실패함");
		return -1;
	}
	printf("... file size : %ld\n", file_size);

	// 파일 이름 받고 생성
	if ((file_name = recv_str(sock)) == NULL) {
		err_print("파일 이름 수신 오류");
		return -1;
	}
	printf("... file name : %s\n", file_name);
	if (path == NULL) {
		file_path = file_name;
	} else {
		file_path = (char *)malloc(strlen(path) + strlen(file_name) + 2);
		strcpy(file_path, path);
		strcat(file_path, file_name);
	}
	if((fd = creat(file_path, 0644)) == -1) {
		err_print("파일 생성 오류");
		SAFE_FREE(file_name);
		SAFE_FREE(file_path);
		return -1;
	}
	SAFE_FREE(file_name);
	SAFE_FREE(file_path);

	puts("... Downloading file");
	// 파일 다운로드 시작
	while ((len = recv(sock, buf, BUF_SIZE, 0)) > 0) {
		if (write(fd, buf, len) < 0) {
			err_print("파일 쓰기 작업 중 오류");
			continue;
		}
		all_len += len;
		if (all_len >= file_size) {
			if (all_len > file_size) {
				err_print("데이터 수신 오류 의심");
			}
			break;
		}
	}
	printf("... %ld of %ld\n", all_len, file_size);

	// 전송 완료 신호 전송
	if (send_data(sock, 0, NULL, STAT_OK) != SEND_OK) {
		err_print("파일 전송 완료 신호 수신 오류");
		return -1;
	}
	puts("... File download complete");

	// 파일을 닫는다.
	close(fd);
	
	return 0;
}

/* 작업 경로를 변경한다. */
int cmd_cd(const char *base_path, const char *path, char **work_path, int is_admin)
{
	char *cwd = NULL;
	char *end_of_base = NULL;
	char *real_path = NULL;
	
	// base_path 혹은 work_path의 값이 NULL이면 오류
	// 동작을 정상적으로 수행할 수 없음
	// 이 오류는 동작 중 발생하는 오류가 아닌 로직 오류
	if (base_path == NULL || work_path == NULL) {
		err_panic("base_path or work_path is NULL");
	}

	// Default 작업 경로는 ~(HOME, base_path)
	SAFE_FREE(*work_path);
	*work_path = (char*)malloc(2);
	strcpy(*work_path, "~");

	// 경로를 지정하지 않으면 HOME으로 작업경로를 변경
	if (path == NULL || !strlen(path)) {
		return chdir(base_path);
	}

	real_path = (char *)malloc(strlen(base_path) + strlen(path) + 2);

	// 경로의 시작점이 ~이라면 ~를 base_path로 치환
	if (path[0] == '~') {
		strcpy(real_path, base_path);
		// ~/something.../something.../ 과 같이 하위 경로가 있다면 추가
		if (strlen(path) > 2 && strlen(path + 1) > 0) {
			strcat(real_path, path + 1);
		}
	} else {
		// 아니라면 path를 real_path에 복사
		strcpy(real_path, path);
	}

	// 경로가 지정되면 해당 경로로 작업 경로 변경
	if (chdir(real_path) == -1) {
		SAFE_FREE(real_path);
		return -1;
	}

	cwd = getcwd(NULL, BUFSIZ);
	if (!is_admin && strlen(base_path) > strlen(cwd)) {
		// HOME보다 상위 경로로 이동했을 때 권한이 없었다면
		// HOME으로 작업 경로 재변경
		SAFE_FREE(cwd);
		SAFE_FREE(real_path);
		return chdir(base_path);
	} else if (strlen(base_path) > strlen(cwd)) {
		// 작업 경로가 HOME보다 상위라면
		// work_path를 cwd로 설정
		SAFE_FREE(*work_path);
		*work_path = cwd;
		return 0;
	}else if (strlen(base_path) == strlen(cwd)) {
		return 0;
	}
	// 현재 작업경로가 HOME보다 하위라면
	// base_path를 ~로 치환하여 work_path를 설정
	end_of_base = &cwd[strlen(base_path)];
	SAFE_FREE(*work_path);
	*work_path = (char*)malloc(strlen(end_of_base) + 2);
	strcpy(*work_path, "~");
	strcat(*work_path, end_of_base);
	SAFE_FREE(cwd);
	SAFE_FREE(real_path);
	return 0;
}

/* directory 내 파일/하위 directory 이름들을 공백으로 구분한 문자열 반환한다. */
char* cmd_ls(const char* path, int is_all)
{
	int i = 0;
	int name_count = 0;
	const int NAME_SIZE = 256;
	char* directory_contents = NULL;
	struct dirent** name_list = NULL;

	if (path == NULL) {
		name_count = scandir(".", &name_list, NULL, alphasort);
	} else {
		name_count = scandir(path, &name_list, NULL, alphasort);
	}

	if (name_count < 0) {
		return NULL;
	}

	directory_contents = (char*)malloc(NAME_SIZE * name_count + name_count);
	memset(directory_contents, 0, NAME_SIZE * name_count + name_count);
	for (i = 0; i < name_count; i++) {
		if (!strcmp(name_list[i]->d_name, ".") || !strcmp(name_list[i]->d_name, "..")) {
			continue;
		}
		if (!is_all && name_list[i]->d_name[0] == '.') {
			continue;
		}

		strcat(directory_contents, name_list[i]->d_name);
		strcat(directory_contents, " ");
	}

	for (i = 0; i < name_count; i++) {
		SAFE_FREE(name_list[i]);
	}
	SAFE_FREE(name_list);

	return directory_contents;
}

/* 소켓을 생성하고 bind, listen 등의 사전 작업을 수행한 후, 소켓의 fd 반환 */
int open_server(int port)
{
	int serv_sock = 0;
	int err_chk = 0;
	struct sockaddr_in serv_addr;

	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if (serv_sock == -1) {
		return SOCKET_ERR;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);

	err_chk = bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (err_chk == -1) {
		return BIND_ERR;
	}

	err_chk = listen(serv_sock, 5);
	if (err_chk == -1) {
		return LISTEN_ERR;
	}

	return serv_sock;
}

/* 소켓을 생성하고 서버에 connect 작업을 수행한 후, 소켓의 fd 반환 */
int open_connection(const char *host, int port)
{
	int sock = 0;
	int err_chk = 0;
	struct sockaddr_in serv_addr;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		return SOCKET_ERR;
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(host);
	serv_addr.sin_port = htons(port);

	err_chk = connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
	if (err_chk == -1) {
		return CONNECT_ERR;
	}
	return sock;
}

/* 메시지를 stderr에 출력한다. */
int err_print(const char *message)
{
	fputs("\033[1;31mERROR : \033[0m", stderr);
	fputs(message, stderr);
	fputs("\n", stdout);
	return 0;
}

/* 어떤 값이 특정 값일 경우, 에러 메시지를 출력하고 프로그램을 종료시킨다 */
int err_handling(int err_chk, const int err_num, const char* message)
{
	if (err_chk != err_num) {
		return err_chk;
	}
	err_print(message);
	exit(1);
}

/* 기존 err_handling함수에서 조건문을 삭제한 함수 */
int err_panic(const char* message)
{
	err_print(message);
	exit(1);
}
