#source file
ver=debug

# 源文件，自动找所有 .c 和 .cpp 文件，并将目标定义为同名 .o 文件
SOURCE  := $(wildcard *.c) $(wildcard *.cpp)
OBJS    := $(patsubst %.c,%.o,$(patsubst %.cpp,%.o,$(SOURCE)))
 
#target you can change pp_monitor_server to what you want
# 目标文件名，输入任意你想要的执行文件名
TARGET  := epoll_high_concurrency

#compile and lib parameter
# 编译参数
CC      := gcc
LIBS    :=
LDFLAGS:= 
DEFINES:=
INCLUDE:= -I.
CXXFLAGS:= $(CFLAGS) -DHAVE_CONFIG_H

ifeq ($(ver), debug)
CFLAGS =  -g -Ddebug $(DEFINES) $(INCLUDE)
else
CFLAGS =  -O3 $(DEFINES) $(INCLUDE)
endif
 
#i think you should do anything here
# 下面的基本上不需要做任何改动了

.PHONY : everything objs clean veryclean rebuild

everything : $(TARGET)

all : $(TARGET)

objs : $(OBJS)

rebuild: veryclean everything
               
clean :
	rm -fr *.so
	rm -fr *.o 
	rm -fr epoll_high_concurrency
   
veryclean : clean
	rm -fr $(TARGET)

$(TARGET) : $(OBJS) 
	$(CC) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS) $(LIBS)
