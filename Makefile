TARGET    := bin/procs
SRCS			:= src/saver.c src/loader.c src/sender.c src/utils/common.c src/procs.c
OBJS			:= $(SRCS:.c=.o)
DEPS			:= $(OBJS) $(TARGET_VIRTUALIZER)
LIBS			:=

TARGET_DAEMON   := bin/procsd
SRCS_DAEMON 		:= src/saver.c src/loader.c src/sender.c src/utils/common.c src/balancer.c
OBJS_DAEMON			:= $(SRCS_DAEMON:.c=.o)
DEPS_DAEMON			:= $(DEPS) $(OBJS_DAEMON)
LIBS_DAEMON   	:= -lpthread

TARGET_VIRTUALIZER 	:= bin/virtualizer
SRCS_VIRTUALIZER		:= src/utils/common.c src/virtualizer.c
OBJS_VIRTUALIZER		:= $(SRCS_VIRTUALIZER:.c=.o)
DEPS_VIRTUALIZER		:= $(OBJS_VIRTUALIZER)
LIBS_VIRTUALIZER		:=


CC			:= gcc
ARCH := $(shell getconf LONG_BIT)
CCFLAGS := -DX$(ARCH) -Werror -Wfatal-errors -g -Wall -c 
LDFLAGS := 

all: $(TARGET) $(TARGET_DAEMON) $(TARGET_VIRTUALIZER) 

$(TARGET): $(DEPS)
	mkdir bin
	$(CC) $(LDFLAGS) $(LIBS) $(OBJS) -o $(TARGET)
$(TARGET_DAEMON): $(DEPS_DAEMON)
	$(CC) $(LDFLAGS) $(LIBS_DAEMON) $(OBJS_DAEMON) -o $(TARGET_DAEMON)
$(TARGET_VIRTUALIZER): $(DEPS_VIRTUALIZER)
	$(CC) $(LDFLAGS) $(LIBS_VIRTUALIZER) $(OBJS_VIRTUALIZER) -o $(TARGET_VIRTUALIZER)

.c.o:
	$(CC) $(CCFLAGS) -c $< -o $@

install: 
	mkdir /etc/procs
	chmod 777 /etc/procs
	cp bin/* /usr/bin

clean:
	rm -f $(OBJS) $(TARGET) $(OBJS_DAEMON) $(TARGET_DAEMON) $(OBJS_VIRTUALIZER) $(TARGET_VIRTUALIZER)
