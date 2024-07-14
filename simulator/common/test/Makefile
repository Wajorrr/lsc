# 定义编译器
CXX=g++
CC=gcc

# 定义编译选项
CXXFLAGS=-g -Wall
CFLAGS=-g -Wall

# 定义源文件
SOURCES_CPP=example.cpp
SOURCES_C=../logging.c

# 定义头文件依赖
HEADERS=../const.h ../logging.h

# 定义对象文件
OBJECTS=$(SOURCES_CPP:.cpp=.o) $(SOURCES_C:.c=.o)

# 定义最终可执行文件的名称
EXECUTABLE=example

# 默认目标
all: $(EXECUTABLE)

# $@ 表示规则中的目标文件名(target)。
# $^ 表示所有的依赖文件列表(dependencies)，不包括重复的依赖。
# $< 表示规则中的第一个依赖文件名(first dependency)。
# 例子：program: main.o utils.o
#    	   gcc -o $@ $^
# 这里，$@代表program（目标文件名），$^代表main.o utils.o（所有的依赖文件）

# 链接对象文件生成最终可执行文件
$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# 从cpp文件编译生成o文件，添加对const.h的依赖
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 从c文件编译生成o文件，添加对const.h的依赖
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# 清理编译生成的文件
clean:
	rm -f $(OBJECTS) $(EXECUTABLE)