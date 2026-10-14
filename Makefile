CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99 -D_WIN32_WINNT=0x0601
LDFLAGS = -lws2_32 -lpsapi -liphlpapi

TARGET = adaptive_scheduler.exe

SRCS = backend/main.c \
       backend/system_telemetry.c \
       backend/calibration.c \
       backend/workload_generator.c \
       backend/scheduler_fcfs.c \
       backend/scheduler_adaptive.c \
       backend/experiment_runner.c \
       backend/http_server.c \
       backend/json_builder.c

OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
