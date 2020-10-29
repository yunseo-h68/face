CC = gcc
CFLAGS = -W -Wall -g
TARGET_SERVER = bin/server
TARGET_CLIENT = bin/client
RUN_IP = 127.0.0.1
RUN_PORT = 10005

all: $(TARGET_SERVER) $(TARGET_CLIENT)

run_server: $(TARGET_SERVER)
	$^ $(RUN_PORT)

run_client: $(TARGET_CLIENT)
	$^ $(RUN_IP) $(RUN_PORT)

$(TARGET_SERVER): src/ftp_server.o src/common.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

$(TARGET_CLIENT): src/ftp_client.o src/common.o
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf bin src/*.o
