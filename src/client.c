#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "common.h"

int process_cmd_ls(int sock);
int process_cmd_cd(int sock, char **work_path);

int main(int argc, char* argv[])
{
	int i = 0;
	int sock = 0;
	int err_chk = 0;
	char c = 0;
	char buf[BUFSIZ] = {0,};
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
		memset(buf, 0, BUFSIZ);
		for (i = 0; (c = getchar()) != '\n' && i < (int)BUFSIZ; i++) {
			buf[i] = c;
		}
		buf[i] = '\0';
		if (strlen(buf) <= 0) continue;

		// 문자열로 된 명령을 request_command 구조체로 변환
		req_cmd = new_request_command(buf);
		if (req_cmd == NULL) {
			err_print("명령을 파싱하는 데 실패하였습니다.");
			continue;
		} else if (req_cmd->cmd == CMD_NONE) {
			err_print("Command not found");
			continue;
		} else if (req_cmd->cmd == ARG_SIZE_OVER) {
			sprintf(buf, "The maximum length of the argument is %d.", ARG_MAX_SIZE);
			err_print(buf);
			continue;
		}

		// 명령을 전송한다.
		if (send(sock, req_cmd, sizeof(struct request_command), 0) == -1) {
			SAFE_FREE(work_path);
			err_panic("send() error");
		}
		if (req_cmd->cmd == CMD_CLOSE) {
			puts("\n\033[0mClosed");
			exit(0);
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
					printf("%s를 다운받으시겠습니까?(Y or n) : ", req_cmd->argv[i]);
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
					printf("%s를 업로드하시겠습니까?(Y or n) : ", req_cmd->argv[i]);
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
	struct packet pack;

	// 결과 전송 시작 신호 수신
	if (recv_stat(sock) != STAT_START) {
		return START_ERR;
	}

	// 수행 결과 수신 후 출력
	while (1) {
		memset(&pack, 0, sizeof(struct packet));
		if (recv_data(sock, &pack) == -1) {
			return RECV_ERR;
		}
		if (pack.status != STAT_DOING && pack.status != STAT_END) {
			return RECV_ERR;
		}
		// 전송 완료 신호가 수신되면 탈출
		if (pack.status == STAT_END) {
			break;
		}
		// 수신한 값을 출력함
		fputs(pack.data, stdout);
	}
	fputs("\n", stdout);

	return 0;
}

int process_cmd_cd(int sock, char **work_path)
{
	char buf[BUFSIZ] = {0, };
	struct packet pack;

	// 결과 전송 시작 신호 수신
	if (recv_stat(sock) != STAT_START) {
		return START_ERR;
	}

	// 작업 경로 수신
	while(1) {
		memset(&pack, 0, sizeof(struct packet));
		if (recv_data(sock, &pack) == -1) {
			return RECV_ERR;
		}
		if (pack.status != STAT_DOING && pack.status != STAT_END) {
			return RECV_ERR;
		}
		// 전송 완료 신호가 수신되면 탈출
		if (pack.status == STAT_END) {
			break;
		}
		strcat(buf, pack.data);
	}
	if (strlen(buf) == 0) {
		return CD_ERR;
	}

	SAFE_FREE(*work_path);
	*work_path = (char *)malloc(strlen(buf) + 1);
	strcpy(*work_path, buf);
	
	return 0;
}
