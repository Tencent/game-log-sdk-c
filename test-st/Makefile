LIBDIR:=../lib ../thirdparty/event/lib ../thirdparty/jemalloc/lib
STATIC_LIBS :=tglog-st event jemalloc
ifeq ($(shell uname -s),Darwin)
    DYNAMIC_LIBS :=m z pthread
else
    DYNAMIC_LIBS :=m z rt dl pthread
endif
INCLUDES:=. ../tglog-st ../common ../thirdparty/event/include

CC = gcc
CFLAGS = -Wall -Werror -Wno-typedef-redefinition -std=c99 -D_GNU_SOURCE $(addprefix -I,$(INCLUDES))

# Build optimization
ifdef RELEASE
    CFLAGS += -O2 -DNDEBUG
else
    CFLAGS += -g -O0 -DDEBUG
endif

# 获取所有的 .c 文件
SOURCES = $(wildcard *.c)

# 为每个 .c 文件生成一个可执行文件名（添加 _st 后缀）
EXECUTABLES = $(SOURCES:.c=_st)

# 默认目标
all: $(EXECUTABLES)

# 定义库文件路径作为依赖
LIB_DEPS = ../lib/libtglog-st.a

# 规则：编译每个 .c 文件到一个可执行文件，依赖于库文件
%_st: %.c $(LIB_DEPS)
	@if [ "$(shell uname -s)" = "Darwin" ]; then \
		$(CC) $(CFLAGS) -g -o $@ $< $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(STATIC_LIBS)) $(addprefix -l,$(DYNAMIC_LIBS)); \
	else \
		$(CC) $(CFLAGS) -g -o $@ $< $(addprefix -L,$(LIBDIR)) -Wl,-dn $(addprefix -l,$(STATIC_LIBS)) -Wl,-dy $(addprefix -l,$(DYNAMIC_LIBS)); \
	fi

# 运行测试
run: $(EXECUTABLES)
	@echo "Running single-threaded tests..."
	@for test in $(EXECUTABLES); do \
		if [ -x "./$$test" ]; then \
			echo "Running $$test..."; \
			./$$test || true; \
		fi; \
	done

# 清理生成的文件
clean:
	rm -f $(EXECUTABLES)

# 安装目标
install: all
	@echo "Installing single-threaded test programs..."
	@mkdir -p ../dist/bin
	@for exe in $(EXECUTABLES); do \
		if [ -f "$$exe" ] && [ -x "$$exe" ]; then \
			cp -f "$$exe" ../dist/bin/; \
		fi; \
	done

.PHONY: all clean run install