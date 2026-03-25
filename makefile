CC = gcc
CFLAGS = -Wall -Wextra -D_GNU_SOURCE
LDFLAGS = -lsqlite3 -lcurl -lssl -lcrypto -larchive

SRCS = main.c utils.c cmd/gc.c cmd/help.c cmd/init.c cmd/install.c cmd/list.c cmd/remove.c
TARGET = mpkg

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)