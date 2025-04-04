CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS_MQ = -lrt 
TARGET1 = z1
TARGET2 = z2
SRC1 = z1.c
SRC2 = z2.c

all: $(TARGET1) $(TARGET2)

$(TARGET1): $(SRC1)
	$(CC) $(CFLAGS) $(SRC1) -o $(TARGET1)

$(TARGET2): $(SRC2)
	$(CC) $(CFLAGS) $(SRC2) -o $(TARGET2) $(LDFLAGS_MQ)

clean:
	rm -f $(TARGET1) $(TARGET2)
