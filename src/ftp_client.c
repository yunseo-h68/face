#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

int process_cmd_ls(int sock);
int process_cmd_cd(int sock, char **work_path);
struct request_command* send_command(int sock, const char* str);

int main(int argc, char* argv[])
{
	int i = 0;
	int sock = 0;
	int err_chk = 0;
	char c = 0;
	char buf[BUF_DATA_SIZE] = {0,};
	char *work_path = (char*)malloc(2);
	size_t data_size = 0;
	struct request_command* req_cmd = NULL;
	strcpy(work_path, "~");

	if (argc != 3) {
		printf("Usage : %s <IP> <Port>\n", argv[0]);
		exit(1);
	}

	// 서버와 연결된 socket 생성
	sock = open_connection(argv[1], atoi(argv[2]));
	err_handling(sock, SOCKET_ERR, "socket() error");
	err_handling(sock, CONNECT_ERR, "connect() error");

	printf("Welcome to Face\nConnected to %s:%s\n\n", argv[1], argv[2]);

	while (1) {
		// 서버 정보 출력
		printf("\033[1;32m%s:%s:", argv[1], argv[2]);

		// 현재 작업 경로를 출력한다.
		if (work_path != NULL) {
			printf("\033[1;34m%s", work_path);
		}

		// 명령을 입력 받는다.
		fputs("$\033[0m ", stdout);
		memset(buf, 0, BUF_DATA_SIZE);
		for (i = 0; (c = getchar()) != '\n' && i < (int)BUF_DATA_SIZE; i++) {
			buf[i] = c;
		}
		buf[i] = '\0';
		if (strlen(buf) <= 0) continue;

		// 명령을 전송한다.
		req_cmd = send_command(sock, buf);
		if (req_cmd->cmd == SOCK_CLOSED) {
			puts("\n\033[0mClosed");
			exit(0);
		} else if (req_cmd->cmd == COMMAND_NOT_FOUND) {
			err_print("Command not found");
			continue;
		} else if(req_cmd->cmd == ARG_SIZE_OVER) {
			sprintf(buf, "The maximum length of the argument is %ld.", BUF_DATA_SIZE);
			err_print(buf);
			continue;
		}else if (req_cmd->cmd == SEND_ERR) {
			SAFE_FREE(work_path);
			err_panic("send_data() error");
		}

		// 명령을 처리한다.
		switch (req_cmd->cmd) {
			case CMD_CD:
				err_chk = process_cmd_cd(sock, &work_path);
				if (err_chk == RECV_ERR) {
					SAFE_FREE(work_path);
					err_panic("recv_data() error");
				} else if (err_chk == START_ERR) {
					SAFE_FREE(work_path);
					err_panic("START 신호를 수신하는 데 실패함");
				} else if (err_chk == CD_ERR) {
					err_print("존재 하지 않는 경로");
				}
				break;
			case CMD_LS:
				err_chk = process_cmd_ls(sock);
				if (err_chk == RECV_ERR) {
					SAFE_FREE(work_path);
					err_panic("recv_data() error");
				} else if (err_chk == START_ERR) {
					SAFE_FREE(work_path);
					err_panic("START 신호를 수신하는 데 실패함");
				}
				break;
			case CMD_GET:
				for (i = 0; i < req_cmd->argc; i++) {
					file_download(sock, "./");
				}
				break;
			case CMD_PUT:
				for (i = 0; i < req_cmd->argc; i++) {
					file_upload(sock, req_cmd->argv[i]);
				}
				break;
			case CMD_MGET:
				for (i = 0; i < req_cmd->argc; i++) {
					c = getchar();
					rewind(stdin);
					if (c == '\n') {
						c = 'y';
					}
					if (c != 'y' && c != 'n') {
						fputs("y 또는 n를 입력해주세요.\n", stdout);
						i--;
						continue;
					}
					data_size = sizeof(c);
					if (send_data(sock, data_size, &c, STAT_OK) != SEND_OK) {
						SAFE_FREE(work_path);
						err_panic("파일 다운로드 여부를 전송하는 데 실패함");
					}
					if (c == 'y') {
						printf("%s를 다운로드합니다...\n", req_cmd->argv[i]);
						file_download(sock, "./");
					}
				}
				break;
			case CMD_MPUT:
				for (i = 0; i < req_cmd->argc; i++) {
					c = getchar();
					rewind(stdin);
					if (c == '\n') {
						c = 'y';
					}
					if (c != 'y' && c != 'n') {
						fputs("y 또는 n를 입력해주세요.\n", stdout);
						i--;
						continue;
					}
					data_size = sizeof(c);
					if (send_data(sock, data_size, &c, STAT_OK) != SEND_OK) {
						SAFE_FREE(work_path);
						err_panic("파일 업로드 여부를 전송하는 데 실패함");
					}
					if (c == 'y') {
						printf("%s를 업로드합니다...\n", req_cmd->argv[i]);
						file_upload(sock, req_cmd->argv[i]);
					}
				}
				break;
			default:
				err_panic("Not reachable");
		}
	}
}

int process_cmd_ls(int sock)
{
	char data_status = 0;
	char buf[BUF_SIZE] = {0, };

	// 결과 전송 시작 신호 수신
	if (recv_stat(sock) != STAT_START) {
		return START_ERR;
	}

	// 수행 결과 수신 후 출력
	while (1) {
		memset(buf, 0, BUF_SIZE);
		if (recv_data(sock, buf) == -1) {
			return RECV_ERR;
		}
		data_status = get_packet_stat(buf);
		if (data_status != STAT_DOING && data_status != STAT_END) {
			return RECV_ERR;
		}
		if (data_status == STAT_END) {
			// 전송 완료 신호가 수신되면 탈출
			break;
		}
		// 수신한 값을 출력함
		fputs((buf + OFFSET_DATA), stdout);
	}
	fputs("\n", stdout);

	return 0;
}

int process_cmd_cd(int sock, char **work_path)
{
	char data_status = 0;
	char buf[BUFSIZ] = {0, };
	char temp_buf[BUF_SIZE] = {0, };

	// 결과 전송 시작 신호 수신
	if (recv_stat(sock) != STAT_START) {
		return START_ERR;
	}

	// 작업 경로 수신
	while(1) {
		if (recv_data(sock, temp_buf) == -1) {
			return RECV_ERR;
		}
		data_status = get_packet_stat(temp_buf);
		if (data_status != STAT_DOING && data_status != STAT_END) {
			return RECV_ERR;
		}
		if (data_status == STAT_END) {
			// 전송 완료 신호가 수신되면 탈출
			break;
		}
		strcat(buf, (temp_buf + OFFSET_DATA));
	}
	if (strlen(buf) == 0) {
		return CD_ERR;
	}

	SAFE_FREE(*work_path);
	*work_path = (char *)malloc(strlen(buf) + 1);
	strcpy(*work_path, buf);
	
	return 0;
}

struct request_command* send_command(int sock, const char* str)
{
	int i = 0;
	size_t data_size = 0;
	char *cmd_str = NULL;
	struct request_command* req_cmd = NULL;

	if (str == NULL) {
		return NULL;
	}
	cmd_str = (char *)malloc(strlen(str) + 1);
	strcpy(cmd_str, str);

	// 문자열로 된 명령을 request_command 구조체로 변환
	req_cmd = new_request_command(cmd_str);
	SAFE_FREE(cmd_str);
	if (req_cmd == NULL) {
		return NULL;
	}

	// 존재하지 않는 명령이면 오류
	if (req_cmd->cmd == CMD_NONE) {
		req_cmd->cmd = COMMAND_NOT_FOUND;
		return req_cmd;
	}

	// 명령 전송
	data_size = sizeof(req_cmd->cmd);
	if (send_data(sock, data_size, &(req_cmd->cmd), STAT_OK) == SEND_ERR) {
		req_cmd->cmd = SEND_ERR;
		return req_cmd;
	}

	// 만약 명령 코드가 CMD_CLOSE라면 소켓 종료
	if (req_cmd->cmd == CMD_CLOSE) {
		close(sock);
		req_cmd->cmd = SOCK_CLOSED;
		return req_cmd;
	}

	// 인자의 개수를 전송한다.
	data_size = sizeof(req_cmd->argc);
	if (send_data(sock, data_size, &(req_cmd->argc), STAT_OK) == SEND_ERR) {
		req_cmd->cmd = SEND_ERR;
		return req_cmd;
	}
	
	// 인자 전송 신호
	if (send_data(sock, 0, NULL, STAT_START) == SEND_ERR) {
		req_cmd->cmd = SEND_ERR;
		return req_cmd;
	}

	// 인자들을 순서대로 전송한다.
	for (i = 0; i < req_cmd->argc; i++) {
		data_size = strlen(req_cmd->argv[i]) + 1;
		if (send_data(sock, data_size, req_cmd->argv[i], STAT_DOING) == SEND_ERR) {
			req_cmd->cmd = SEND_ERR;
			return req_cmd;
		}
	}

	// 인자 전송 완료 신호
	if (send_data(sock, 0, NULL, STAT_END) == SEND_ERR) {
		req_cmd->cmd = SEND_ERR;
		return req_cmd;
	}
	
	return req_cmd;
}
