# test-tapp Makefile

# 编译器设置
CC := gcc
CFLAGS := -Wall -O0 -g -std=c99 -D_GNU_SOURCE -D_REENTRANT

# 目录设置
ROOT_DIR := ..
TSF4G_DIR := ./tsf4g
TDR_TOOL := $(TSF4G_DIR)/tools/tdr

# 包含目录
INCLUDES := -I. \
            -I$(ROOT_DIR)/tglog-st \
            -I$(ROOT_DIR)/common \
            -I$(ROOT_DIR)/thirdparty/event/include \
            -I$(TSF4G_DIR)/include

# 库目录
LIBDIRS := -L$(ROOT_DIR)/lib \
           -L$(ROOT_DIR)/thirdparty/event/lib \
           -L$(ROOT_DIR)/thirdparty/jemalloc/lib \
           -L$(TSF4G_DIR)/lib

# 静态库
STATIC_LIBS := -ltglog-st -levent -ljemalloc \
               -ltapp -lreadline -lncurses -ltsf4g -lscew -lexpat

# 动态库
DYNAMIC_LIBS := -lm -lz -lrt -ldl -lpthread -lanl -lstdc++

# 链接选项
LDFLAGS := $(LIBDIRS) -Wl,-dn $(STATIC_LIBS) -Wl,-dy $(DYNAMIC_LIBS)

# 源文件
SRCS := tglog_tapp.c tglog_tapp_def.c
OBJS := $(SRCS:.c=.o)

# TDR生成的文件
ALL_DEF_FILES := tglog_tapp_def.tdr tglog_tapp_def_t.xml tglog_tapp_def.c tglog_tapp_def.h

# 可执行文件
TARGET := tglog_tapp

# 默认目标
all: $(TARGET)

# 生成TDR文件
$(ALL_DEF_FILES): tglog_tapp_def.xml
	$(TDR_TOOL) -B tglog_tapp_def.xml -o tglog_tapp_def.tdr
	$(TDR_TOOL) -G -m TestTappCfg tglog_tapp_def.xml -o tglog_tapp_def_t.xml
	$(TDR_TOOL) -C tglog_tapp_def.xml -o tglog_tapp_def.c
	$(TDR_TOOL) -H tglog_tapp_def.xml

# 定义库文件路径作为依赖
LIB_DEPS = $(ROOT_DIR)/lib/libtglog-st.a

# 编译目标文件，依赖于头文件
%.o: %.c tglog_tapp_def.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# 链接可执行文件，依赖于库文件
$(TARGET): $(OBJS) $(LIB_DEPS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# 清理
clean:
	rm -f $(ALL_DEF_FILES) *.d *.o $(TARGET)

# 安装
install: all
	@echo "Installing test-tapp example..."
	@mkdir -p ../dist/examples/test-tapp
	@if [ -f "$(TARGET)" ] && [ -x "$(TARGET)" ]; then \
		cp -f $(TARGET) ../dist/examples/test-tapp/; \
	fi
	@if [ -f "start.sh" ]; then \
		cp -f start.sh ../dist/examples/test-tapp/; \
		chmod +x ../dist/examples/test-tapp/start.sh; \
	fi
	@if [ -f "stop.sh" ]; then \
		cp -f stop.sh ../dist/examples/test-tapp/; \
		chmod +x ../dist/examples/test-tapp/stop.sh; \
	fi
	@if [ -f "tglog_tapp_cfg.xml" ]; then \
		cp -f tglog_tapp_cfg.xml ../dist/examples/test-tapp/; \
	fi
	@if [ -f "tlog_cfg.xml" ]; then \
		cp -f tlog_cfg.xml ../dist/examples/test-tapp/; \
	fi

# 重新构建
rebuild: clean all

# 只生成TDR文件
tdr: $(ALL_DEF_FILES)

# 运行程序
run: $(TARGET)
	./start.sh

# 停止程序
stop:
	./stop.sh

# 显示帮助
help:
	@echo "test-tapp Makefile targets:"
	@echo "  all       - Build executable (default)"
	@echo "  tdr       - Generate TDR files only"
	@echo "  clean     - Remove all generated files"
	@echo "  rebuild   - Clean and rebuild"
	@echo "  run       - Build and run the program"
	@echo "  stop      - Stop the running program"
	@echo "  help      - Show this help"

.PHONY: all clean install rebuild tdr run stop help

