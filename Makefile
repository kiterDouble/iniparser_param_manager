#==============================================================================
# 参数管理模块 Makefile
# 简化版参数管理模块 - 可移植到其他项目
#==============================================================================

# 编译器设置
CC ?= gcc
CXX ?= g++

# 编译选项
CFLAGS = -Wall -Wextra -O2 -g
CFLAGS += -pthread

# 目录设置
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
EXAMPLES_DIR = examples
TESTS_DIR = tests


# 头文件路径
INCLUDES = -I$(INC_DIR)

# 源文件
SRCS = $(SRC_DIR)/param_manager.c \
       $(SRC_DIR)/iniparser.c

# 示例和测试源文件
EXAMPLE_SRC = $(EXAMPLES_DIR)/example.c
TEST_SRC = $(TESTS_DIR)/test_param_manager.c
MODIFIED_TEST_SRC = $(TESTS_DIR)/test_modified.c

# 目标文件
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
EXAMPLE_OBJ = $(BUILD_DIR)/example.o
TEST_OBJ = $(BUILD_DIR)/test_param_manager.o
MODIFIED_TEST_OBJ = $(BUILD_DIR)/test_modified.o

# 目标可执行文件
TARGET = $(BUILD_DIR)/param_example
TEST_TARGET = $(BUILD_DIR)/param_test
MODIFIED_TEST_TARGET = $(BUILD_DIR)/test_modified

# 静态库
STATIC_LIB = $(BUILD_DIR)/libparammanager.a

# 动态库
SHARED_LIB = $(BUILD_DIR)/libparammanager.so

#==============================================================================
# 默认目标：编译示例
#==============================================================================
.PHONY: all clean install lib static shared test dirs

all: dirs $(TARGET)

# 创建目录
dirs:
	@mkdir -p $(BUILD_DIR)

#==============================================================================
# 编译示例程序
#==============================================================================
$(TARGET): $(EXAMPLE_OBJ) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(EXAMPLE_OBJ) $(STATIC_LIB) $(INCLUDES)
	@echo "编译完成: $@"

#==============================================================================
# 编译测试程序
#==============================================================================
$(TEST_TARGET): $(TEST_OBJ) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJ) $(STATIC_LIB) $(INCLUDES)
	@echo "测试程序编译完成: $@"

$(MODIFIED_TEST_TARGET): $(MODIFIED_TEST_OBJ) $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $(MODIFIED_TEST_OBJ) $(STATIC_LIB) $(INCLUDES)
	@echo "Modified测试程序编译完成: $@"

test: dirs $(TEST_TARGET)
	@echo "运行测试..."
	@cd $(BUILD_DIR) && ./$(notdir $(TEST_TARGET))

test-modified: dirs $(MODIFIED_TEST_TARGET)
	@echo "运行Modified测试..."
	@cd $(BUILD_DIR) && ./$(notdir $(MODIFIED_TEST_TARGET))

#==============================================================================
# 编译静态库
#==============================================================================
static: dirs $(STATIC_LIB)

$(STATIC_LIB): $(OBJS)
	ar rcs $@ $^
	@echo "静态库已生成: $@"

#==============================================================================
# 编译动态库
#==============================================================================
shared: dirs $(SHARED_LIB)

$(SHARED_LIB): $(SRC_DIR)/param_manager.c $(SRC_DIR)/iniparser.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $^ $(INCLUDES)
	@echo "动态库已生成: $@"

#==============================================================================
# 编译规则
#==============================================================================
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/example.o: $(EXAMPLES_DIR)/example.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_param_manager.o: $(TESTS_DIR)/test_param_manager.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/test_modified.o: $(TESTS_DIR)/test_modified.c | dirs
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

#==============================================================================
# 清理
#==============================================================================
clean:
	rm -rf $(BUILD_DIR)
	@echo "清理完成"

#==============================================================================
# 安装（可选）
#==============================================================================
install: $(STATIC_LIB) $(SHARED_LIB)
	@echo "安装静态库到 /usr/local/lib"
	@cp $(STATIC_LIB) /usr/local/lib/ 2>/dev/null || echo "需要sudo权限安装"
	@echo "安装头文件到 /usr/local/include"
	@cp $(INC_DIR)/param_manager.h /usr/local/include/ 2>/dev/null || echo "需要sudo权限安装"

#==============================================================================
# 运行示例
#==============================================================================
run: $(TARGET)
	@cd $(BUILD_DIR) && ./$(notdir $(TARGET))

#==============================================================================
# 帮助
#==============================================================================
help:
	@echo "使用方法:"
	@echo "  make         - 编译示例程序"
	@echo "  make test    - 编译并运行测试"
	@echo "  make static  - 编译静态库"
	@echo "  make shared  - 编译动态库"
	@echo "  make run     - 运行示例程序"
	@echo "  make clean   - 清理编译产物"
	@echo "  make install - 安装库和头文件"
