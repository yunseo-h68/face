#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"

int send_big_str(int sock, char *str);
int process_cmd_cd(int sock, const char* base_path, struct request_command *req_cmd);
int process_cmd_ls(int sock);
struct request_command* recv_command(int sock);

int main(int argc, char* argv[])
{
	int i = 0;
	int err_chk = 0;
	int serv_sock = 0;
	int clnt_sock = 0;
	char base_path[BUFSIZ] = {0, };

	socklen_t clnt_addr_size;
	struct sockaddr_in clnt_addr;
	struct request_command *req_cmd = NULL;

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

	fputs("Connected to client\n", stdout);

	while (1) {
		// 명령 수신
		req_cmd = recv_command(clnt_sock);
		if (req_cmd == NULL) {
			err_panic("recv_command() error");
		} else if (req_cmd->cmd == CMD_CLOSE) {
			fputs("Disconnected from client\n", stdout);
			close(clnt_sock);
			close(serv_sock);
			break;
		}
		printf("Request : %s(%d)\n", cmd_to_str(req_cmd->cmd), req_cmd->cmd);
		printf("Args(%d) : ", req_cmd->argc);
		for (i = 0; i < req_cmd->argc; i++) {
			fputs(req_cmd->argv[i], stdout);
			fputs(" ", stdout);
		}
		fputs("\n", stdout);


		// 명령을 처리한다.
		switch (req_cmd->cmd) {
			case CMD_CD:
				err_chk = process_cmd_cd(clnt_sock, base_path, req_cmd);
				if (err_chk == SEND_ERR) {
					err_panic("send() error");
				} else if (err_chk == -1) {
					err_panic("failed to change directory");
				}
				break;
			case CMD_LS:
				err_chk = process_cmd_ls(clnt_sock);
				if (err_chk == SEND_ERR) {
					err_panic("send() error");
				}
				break;
			case CMD_GET:
			case CMD_MGET:
				for (i = 0; i < req_cmd->argc; i++) {
					file_upload(clnt_sock, req_cmd->argv[i]);
				}
				break;
			case CMD_PUT:
			case CMD_MPUT:
				for (i = 0; i < req_cmd->argc; i++) {
					file_download(clnt_sock, "./");
				}
				break;
			default:
				err_panic("Not reachable");
		}
	}
	free_request_command(req_cmd);
	return 0;
}

int process_cmd_cd(int sock, const char* base_path, struct request_command *req_cmd)
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
	return 0;
}

int process_cmd_ls(int sock)
{
	char *directory_contents = NULL;

	// ls 명령 수행 결과를 가져온다.
	// 현재 디렉토리에서 -a 옵션을 적용한 상태로 결과를 가져온다.
	directory_contents = cmd_ls(".", 1);

	// 결과 전송 시작 신호 송신
	if (send_data(sock, 0, NULL, STAT_START) != SEND_OK) {
		SAFE_FREE(directory_contents);
		return SEND_ERR;
	}

	// ls 명령 결과가 없으면(어떤 directory나 file도 없다면
	// 결과 전송 완료 신호 송신
	if (directory_contents == NULL) {
		if (send_data(sock, 0, NULL, STAT_END) != SEND_OK) {
			return SEND_ERR;
		}
	}

	// 결과 전송 시작	
	if (send_big_str(sock, directory_contents) == SEND_ERR) {
		SAFE_FREE(directory_contents);
		return SEND_ERR;
	}

	// 결과 전송 완료 신호 송신
	if (send_data(sock, 0, NULL, STAT_END) != SEND_OK) {
		SAFE_FREE(directory_contents);
		return SEND_ERR;
	}
	SAFE_FREE(directory_contents);
	return 0;
}

int send_big_str(int sock, char *str)
{
	size_t data_size = 0;
	char buf[BUF_DATA_SIZE] = {0, };
	char *remaining_str = str;
	while (1) {
		// 만약 buf의 크기보다 커서 한번에 보내지 못하면
		// 잘라서 보낸다.
		if (strlen(remaining_str) + 1 > BUF_DATA_SIZE - 1) {
			strncpy(buf, remaining_str, BUF_DATA_SIZE - 1);
			buf[BUF_DATA_SIZE - 1] = '\0';
			if (send_data(sock, BUF_DATA_SIZE, buf, STAT_DOING) != SEND_OK) {
				return SEND_ERR;
			}
			remaining_str += BUF_DATA_SIZE;
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
	return 0;
}

struct request_command* recv_command(int sock)
{
	int i = 0;
	struct request_command *req_cmd = (struct request_command*)malloc(sizeof(struct request_command));
	memset(req_cmd, 0, sizeof(struct request_command));
	req_cmd->cmd = 0;
	req_cmd->argc = 0;
	req_cmd->argv = NULL;

	// 명령 코드 수신
	req_cmd->cmd = recv_int(sock);
	if (req_cmd->cmd == 0) {
		return NULL;
	} else if (req_cmd->cmd == CMD_CLOSE) {
		return req_cmd;
	}

	// 인자의 개수 수신
	req_cmd->argc = recv_int(sock);
	if (req_cmd->argc == -1) {
		SAFE_FREE(req_cmd);
		return NULL;
	}

	// request_command 동적할당 후 초기화
	req_cmd->argv = (char**)malloc(sizeof(char*) * req_cmd->argc);
	for (i = 0; i < req_cmd->argc; i++) {
		req_cmd->argv[i] = NULL;
	}

	// 인자 전송 신호 수신
	if (recv_stat(sock) != STAT_START) {
		free_request_command(req_cmd);
		return NULL;
	}

	// 인자 값 수신
	for (i = 0; i < req_cmd->argc; i++) {
		req_cmd->argv[i] = recv_str(sock);
		if (req_cmd->argv[i] == NULL) {
			free_request_command(req_cmd);
			return NULL;
		}
	}

	// 인자 전송 완료 신호 수신
	if (recv_stat(sock) != STAT_END) {
		free_request_command(req_cmd);
		return NULL;
	}

	return req_cmd;
}
