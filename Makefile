CC = gcc
CFLAGS = -O3 -pthread -lm
TARGET = pisa_merda_pi_project
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS)

clean:
	rm -f $(TARGET) matches.txt checkpoint.txt
