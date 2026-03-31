LIBDIR:=../lib ../thirdparty/event/lib ../thirdparty/jemalloc/lib
STATIC_LIBS :=tglog event event_pthreads jemalloc
ifeq ($(shell uname -s),Darwin)
    DYNAMIC_LIBS :=m z pthread
else
    DYNAMIC_LIBS :=m z rt dl pthread
endif
INCLUDES:=. ../tglog ../common ../thirdparty/event/include

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

# 为每个 .c 文件生成一个可执行文件名
EXECUTABLES = $(SOURCES:.c=)

# 默认目标
all: $(EXECUTABLES)

# 定义库文件路径作为依赖
LIB_DEPS = ../lib/libtglog.a

# 规则：编译每个 .c 文件到一个可执行文件，依赖于库文件
%: %.c $(LIB_DEPS)
	@if [ "$(shell uname -s)" = "Darwin" ]; then \
		$(CC) $(CFLAGS) -g -o $@ $< $(addprefix -L,$(LIBDIR)) $(addprefix -l,$(STATIC_LIBS)) $(addprefix -l,$(DYNAMIC_LIBS)); \
	else \
		$(CC) $(CFLAGS) -g -o $@ $< $(addprefix -L,$(LIBDIR)) -Wl,-dn $(addprefix -l,$(STATIC_LIBS)) -Wl,-dy $(addprefix -l,$(DYNAMIC_LIBS)); \
	fi

# 清理生成的文件
clean:
	rm -f $(EXECUTABLES)

# 安装目标
install: all
	@echo "Installing test programs..."
	@mkdir -p ../dist/bin
	@for exe in $(EXECUTABLES); do \
		if [ -f "$$exe" ] && [ -x "$$exe" ]; then \
			cp -f "$$exe" ../dist/bin/; \
		fi; \
	done

.PHONY: all clean install