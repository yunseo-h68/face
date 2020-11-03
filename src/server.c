#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"

int send_big_str(int sock, char *str);
int process_cmd_get(int sock, struct request_command *req_cmd);
int process_cmd_put(int sock, struct request_command *req_cmd);
int process_cmd_mget(int sock, struct request_command *req_cmd);
int process_cmd_mput(int sock, struct request_command *req_cmd);
int process_cmd_cd(int sock, struct request_command *req_cmd);
int process_cmd_ls(int sock, struct request_command *req_cmd);

char base_path[BUFSIZ] = {0, };

int main(int argc, char* argv[])
{
	int i = 0;
	int err_chk = 0;
	int serv_sock = 0;
	int clnt_sock = 0;

	socklen_t clnt_addr_size;
	struct sockaddr_in clnt_addr;
	struct request_command *req_cmd = (struct request_command*)malloc(sizeof(struct request_command));

	// 명령 처리 함수들에 대한 함수 포인터 배열
	PROCESS_CMD_FUNC cmd_functions[6] = {process_cmd_cd, process_cmd_ls, 
		process_cmd_get, process_cmd_put, 
		process_cmd_mget, process_cmd_mput};

	if (argc != 2) {
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}

	// 프로그램 실행 경로가 Base Path(Home)가 된다.
	getcwd(base_path, BUFSIZ);

	// 서버를 위한 Socket 생성
	serv_sock = open_server(atoi(argv[1]));
	err_handling(serv_sock, SOCKET_ERR, "socket() error");
	err_handling(serv_sock, BIND_ERR, "bind() error");
	err_handling(serv_sock, LISTEN_ERR, "listen() error");

	// 클라이언트와의 연결
	clnt_addr_size = sizeof(clnt_addr);
	clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &clnt_addr_size);
	err_handling(clnt_sock, -1, "accept() error");

	fputs("Connected to client\n\n", stdout);

	while (1) {
		// 명령 수신 및 수신된 명령 정보 출력
		if (recv(clnt_sock, req_cmd, sizeof(struct request_command), 0) == -1) {
			err_panic("recv() error");
		}
		if (req_cmd->cmd == CMD_NONE) {
			continue;
		}
		printf("Request : %s(%d)\n", cmd_to_str(req_cmd->cmd), req_cmd->cmd);
		printf("Args(%d) : \n", req_cmd->argc);
		for (i = 0; i < req_cmd->argc; i++) {
			fputs(req_cmd->argv[i], stdout);
			fputs(" ", stdout);
		}
		fputs("\n", stdout);

		// 만약 명령 타입이 CMD_CLOSE라면 클라이언트와의 연결 종료
		if (req_cmd->cmd == CMD_CLOSE) {
			fputs("\nDisconnected from client\n", stdout);
			close(clnt_sock);
			close(serv_sock);
			break;
		}

		// 명령을 처리한다.
		err_chk = cmd_functions[req_cmd->cmd - CMD_START_NUM](clnt_sock, req_cmd);
		if (err_chk == SEND_ERR) {
			err_panic("send() error");
		} else if (err_chk == ERR) {
			err_print("명령 수행 실패");
		}
	}
	SAFE_FREE(req_cmd);
	return 0;
}

int process_cmd_get(int sock, struct request_command *req_cmd)
{
	int i = 0;
	for (i = 0; i < req_cmd->argc; i++) {
		file_upload(sock, req_cmd->argv[i]);
	}
	return OK;
}

int process_cmd_put(int sock, struct request_command *req_cmd)
{
	int i = 0;
	for (i = 0; i < req_cmd->argc; i++) {
		file_download(sock, "./");
	
	}
	return OK;
}

int process_cmd_mget(int sock, struct request_command *req_cmd)
{
	int i = 0;
	for (i = 0; i < req_cmd->argc; i++) {
		if (recv_char(sock) == 'y') {
			file_upload(sock, req_cmd->argv[i]);
		}
	}
	return OK;
}

int process_cmd_mput(int sock, struct request_command *req_cmd)
{
	int i = 0;
	for (i = 0; i < req_cmd->argc; i++) {
		if (recv_char(sock) == 'y') {
			file_download(sock, "./");
		}
	}
	return OK;
}

int process_cmd_cd(int sock, struct request_command *req_cmd)
{
	char *work_path = NULL;
	
	// 작업 경로 변경. 현재 base_path보다 상위 경로도 접근 가능
	if (req_cmd->argc < 1) {
		// 만약 path가 지정되지 않으면 base_path로 작업 경로 변경
		if (cmd_cd(base_path, NULL, &work_path, 1) == -1) {
			// 만약 작업 경로를 변경하는 데 실패한다면 work_path를 "\0"으로 변경.
			SAFE_FREE(work_path);
			work_path = (char*)malloc(1);
			work_path[0] = '\0';
		}
	} else {
		// path가 지정되면 해당 path로 작업 경로 변경
		if (cmd_cd(base_path, req_cmd->argv[0], &work_path, 1) == -1) {
			// 만약 작업 경로를 변경하는 데 실패한다면 work_path를 "\0"으로 변경.
			SAFE_FREE(work_path);
			work_path = (char*)malloc(1);
			work_path[0] = '\0';
		}
	}

	// 설정된 작업 경로 전송 시작 신호 송신
	if (send_data(sock, 0, NULL, STAT_START) != SEND_OK) {
		SAFE_FREE(work_path);
		return SEND_ERR;
	}

	// 설정된 작업 경로 전송 시작
	if (send_big_str(sock, work_path) == SEND_ERR) {
		SAFE_FREE(work_path);
		return SEND_ERR;
	}

	// 결과 전송 완료 신호 송신
	if (send_data(sock, 0, NULL, STAT_END) != SEND_OK) {
		SAFE_FREE(work_path);
		return SEND_ERR;
	}

	SAFE_FREE(work_path);
	return OK;
}

int process_cmd_ls(int sock, struct request_command *req_cmd)
{
	int i = 0;
	int path_count = 0;
	int has_option_a = 0;
	char *directory_contents = NULL;
	char **path = NULL;
	
	if (req_cmd->argc > 0) {
		// 인자가 한 개 이상이라면 옵션이 아닌 인자를 경로로 해서 탐색
		path = (char **)malloc(sizeof(char*) * req_cmd->argc);
		memset(path, 0, sizeof(char*) * req_cmd->argc);
	}

	// 인자 중에서 옵션 및 경로 파싱
	for (i = 0; i < req_cmd->argc; i++) {
		// 옵션 파싱
		if (req_cmd->argv[i][0] == '-') {
			if (!strcmp(req_cmd->argv[i], "-a")) {
				has_option_a = 1;
			}
		} else {
			// 옵션이 아닌 인자는 경로
			path[path_count] = req_cmd->argv[i];
			path_count++;
		}
	}

	// 경로가 지정되지 않으면 현재 경로로
	if (path_count == 0 || path == NULL) {
		SAFE_FREE(path);
		path = (char **)malloc(sizeof(char*) * 1);
		path[0] = ".";
		path_count = 1;
	}

	// 결과 전송 시작 신호 송신
	if (send_data(sock, 0, NULL, STAT_START) != SEND_OK) {
		SAFE_FREE(path);
		SAFE_FREE(directory_contents);
		return SEND_ERR;
	}

	// 모든 경로들에 대해서 탐색 후 결과를 전송
	for (i = 0; i < path_count; i++) {
		// ls 결과 가져오기
		directory_contents = cmd_ls(path[i], has_option_a);

		// ls 명령 결과가 없으면(어떤 directory나 file도 없다면
		// 다음 경로에 대한 결과 탐색
		if (directory_contents == NULL) {
			continue;
		}

		// 결과 전송 시작	
		if (send_big_str(sock, directory_contents) == SEND_ERR) {
			SAFE_FREE(path);
			SAFE_FREE(directory_contents);
			return SEND_ERR;
		}
	}
	SAFE_FREE(directory_contents);
	SAFE_FREE(path);
	
	// 결과 전송 완료 신호 송신
	if (send_data(sock, 0, NULL, STAT_END) != SEND_OK) {
		return SEND_ERR;
	}
	return OK;
}

int send_big_str(int sock, char *str)
{
	size_t data_size = 0;
	char buf[BUF_SIZE] = {0, };
	char *remaining_str = str;
	while (1) {
		// 만약 buf의 크기보다 커서 한번에 보내지 못하면
		// 잘라서 보낸다.
		if (strlen(remaining_str) + 1 > BUF_SIZE - 1) {
			strncpy(buf, remaining_str, BUF_SIZE - 1);
			buf[BUF_SIZE - 1] = '\0';
			if (send_data(sock, BUF_SIZE, buf, STAT_DOING) != SEND_OK) {
				return SEND_ERR;
			}
			remaining_str += BUF_SIZE;
			continue;
		}
		// 한번에 보낼 수 있으면 한번에 보내고 탈출
		data_size = strlen(remaining_str) + 1;
		strncpy(buf, remaining_str, strlen(remaining_str));
		buf[strlen(remaining_str)] = '\0';
		if (send_data(sock, data_size, buf, STAT_DOING) != SEND_OK) {
			return SEND_ERR;
		}
		break;
	}
	return OK;
}
