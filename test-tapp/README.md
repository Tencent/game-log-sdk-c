# TGLog TAPP 集成测试程序

## 概述

本测试程序演示了如何在 TAPP 框架中集成 tglog-st（单线程版本）SDK，并提供了与传统 TLog API 的性能对比测试。

TAPP 是腾讯游戏服务器框架，提供了统一的服务器应用程序接口。本示例展示了如何在 TAPP 应用中使用 tglog-st 进行高性能日志发送。

## 性能对比V1

在相同测试条件下（发送 10,000,000 条300字节左右日志，每proc发送500条），两种方案的性能对比如下：

### TGLog-ST 性能

| 协议 | 耗时 | CPU 占用 | 内存占用 | 丢失 |
|------|------|----------|----------|----------|
| TCP  | ~28秒 | 25% | 86MB | 0条 |
| UDP  | ~28秒 | 25% | 86MB | 0条 |

### TLog API 性能

| 协议 | 耗时 | CPU 占用 | 内存占用 | 丢失 |
|------|------|----------|----------|----------|
| TCP  | ~87秒 | 55% | 79MB | 0条 |
| UDP  | ~87秒 | 74% | 79MB | 0条 |

### 性能总结

- **耗时提升**: TGLog-ST 比 TLog API 快约 **3.1 倍**
- **CPU 效率**: TGLog-ST 的 CPU 占用显著更低
  - TCP 模式下降低 **54%** (从 55% 降至 25%)
  - UDP 模式下降低 **66%** (从 74% 降至 25%)
- **内存占用**: 两者相当，TGLog-ST 略高 9% (86MB vs 79MB)

TGLog-ST 在保持低内存占用的同时，提供了更快的日志发送速度和更低的 CPU 占用，特别适合高并发游戏服务器场景。

## 性能对比V2

在相同测试条件下（发送 10,000,000 条日志，95%小于1K，5%在2K~64K之间，每proc发送500条），两种方案的性能对比如下：

### TGLog-ST 性能

| 协议 | 耗时  | CPU 占用 | 内存占用 | 丢失 |
| ---- | ----- | -------- | -------- | ---- |
| TCP  | ~47秒 | 58%      | 87MB     | 0条  |
| UDP  | ~45秒 | 54%      | 87MB     | 18条 |

### TLog API 性能

| 协议 | 耗时   | CPU 占用 | 内存占用 | 丢失 |
| ---- | ------ | -------- | -------- | ---- |
| TCP  | ~95秒  | 79%      | 79MB     | 0条  |
| UDP  | ~109秒 | 81%      | 79MB     | 0条  |

### 性能总结

- **耗时提升**: TGLog-ST 比 TLog API 快约 **2.0 倍**
- **CPU 效率**: TGLog-ST 的 CPU 占用显著更低
  - TCP 模式下降低 **26%** (从 79% 降至 58%)
  - UDP 模式下降低 **33%** (从 81% 降至 54%)，但是有少量丢失去
- **内存占用**: 两者相当，TGLog-ST 略高 10% (87MB vs 79MB)

TGLog-ST 在保持低内存占用的同时，提供了更快的日志发送速度和更低的 CPU 占用，特别适合高并发游戏服务器场景。

## 目录结构

```
test-tapp/
├── tglog_tapp.c              # 主程序源代码
├── tglog_tapp_def.xml        # TDR 元数据定义（配置结构）
├── tglog_tapp_def.tdr        # TDR 编译后的二进制元数据
├── tglog_tapp_def.h          # TDR 生成的头文件
├── tglog_tapp_def.c          # TDR 生成的实现文件
├── tglog_tapp_cfg.xml        # 应用程序配置文件
├── tlog_cfg.xml              # TLog 日志配置文件
├── start.sh                  # 启动脚本
├── stop.sh                   # 停止脚本
├── CMakeLists.txt            # CMake 构建配置
└── tsf4g/                    # TSF4G 框架库和头文件
```

## 配置说明

### 1. 应用程序配置 (tglog_tapp_cfg.xml)

```xml
<TestTappCfg __version="1">
    <Name>test-tapp</Name>              <!-- 应用名称 -->
    <Network>udp</Network>              <!-- 网络协议: tcp/udp -->
    <Server>dev.tglog.com</Server>      <!-- TGLog 服务器地址 -->
    <Port>20001</Port>                  <!-- TGLog 服务器端口 -->
    <Count>10000000</Count>             <!-- 发送日志总数 -->
    <Batch>500</Batch>                  <!-- 每次 proc 调用发送的日志数 -->
    <UseTLog>0</UseTLog>                <!-- 0=使用TGLog-ST, 1=使用TLog API -->
</TestTappCfg>
```

**配置参数说明**:

- **Name**: 应用程序名称标识
- **Network**: 网络传输协议
  - `tcp`: 使用 TCP 协议（可靠传输）
  - `udp`: 使用 UDP 协议（高性能）
- **Server**: TGLog 日志服务器地址
- **Port**: TGLog 日志服务器端口
- **Count**: 测试发送的日志总数（默认 1000 万条）
- **Batch**: 每次 `svr_proc` 调用发送的日志批次大小
  - 建议值: 100-1000
  - 值越大，发送越快，但会占用更多 CPU
- **UseTLog**: 选择使用的日志 API
  - `0`: 使用 TGLog-ST SDK（推荐，性能更好）
  - `1`: 使用传统 TLog API（用于性能对比）

### 2. TLog 配置 (tlog_cfg.xml)

配置文件定义了两个主要的日志类别：

- **tglog_tapp_net**: 网络日志输出（用于 TLog API 发送日志）
  - 输出到网络地址，格式：`<protocol>://<server>:<port>`
  - 最大消息大小: 65507 字节
  - **重要**: 使用 TCP 协议时，URL 必须以 `tcp://` 开头；使用 UDP 协议时，URL 必须以 `udp://` 开头

  ```xml
  <CategoryList type="TLOGCategory">
      <Name>tglog_tapp_net</Name>
      <!-- ... 其他配置 ... -->
      <Device>
          <Type>NET</Type>
          <Device>
              <Net>
                  <!-- TCP 协议示例 -->
                  <Url>tcp://dev.tglog.com:20001</Url>
  
                  <!-- UDP 协议示例 -->
                  <!-- <Url>udp://dev.tglog.com:20001</Url> -->
              </Net>
          </Device>
      </Device>
  </CategoryList>
  ```

  **注意**: 当修改 `tglog_tapp_cfg.xml` 中的 `<Network>` 参数时，必须同步修改 `tlog_cfg.xml` 中 `tglog_tapp_net` 类别的 `<Url>` 协议前缀，否则 TLog API 将无法正常发送日志。

- **tglog_tapp_debug**: 调试日志输出（用于 TGLog-ST 的调试信息）
  - 输出到本地文件: `log/tglog_tapp_debug.log`
  - 日志轮转: 10MB，保留 20 个文件

## 构建方法

### 使用 CMake 构建

```bash
# 在项目根目录
mkdir build
cd build
cmake ..
make test-tapp

# 或者构建所有目标（包括 test-tapp）
make -j4
```

### 使用 Makefile 构建

```bash
# 在项目根目录
make test-tapp
```

构建成功后，可执行文件 `tglog_tapp` 将生成在 `test-tapp/` 目录下。

## 运行测试

### 1. 启动测试程序

```bash
cd test-tapp
./start.sh
```

启动脚本会以守护进程模式运行程序，参数说明：
- `--conf-file`: 应用配置文件
- `--tlogconf`: TLog 配置文件
- `--pid-file`: PID 文件路径
- `--timer`: Tick 定时器间隔（毫秒），默认 5ms
- `--daemon`: 以守护进程模式运行

### 2. 查看运行日志

```bash
# 查看调试日志（包含性能统计）
tail -f log/tglog_tapp_debug.log

# 实时监控日志输出
watch -n 1 "tail -20 log/tglog_tapp_debug.log"
```

### 3. 停止测试程序

```bash
./stop.sh
```

## 性能测试步骤

### 测试 TGLog-ST 性能

1. 编辑 `tglog_tapp_cfg.xml`，设置 `<UseTLog>0</UseTLog>`
2. 选择网络协议：`<Network>tcp</Network>` 或 `<Network>udp</Network>`
3. 启动程序：`./start.sh`
4. 观察日志输出，记录性能数据
5. 停止程序：`./stop.sh`

### 测试 TLog API 性能

1. 编辑 `tglog_tapp_cfg.xml`，设置 `<UseTLog>1</UseTLog>`
2. 选择网络协议：`<Network>tcp</Network>` 或 `<Network>udp</Network>`
3. **重要**: 编辑 `tlog_cfg.xml`，修改 `tglog_tapp_net` 类别的 `<Url>` 协议前缀：
   - 使用 TCP: `<Url>tcp://dev.tglog.com:20001</Url>`
   - 使用 UDP: `<Url>udp://dev.tglog.com:20001</Url>`
4. 启动程序：`./start.sh`
5. 观察日志输出，记录性能数据
6. 停止程序：`./stop.sh`

### 性能指标说明

程序完成后会在日志中输出以下信息：

```
Sent 10000000 logs in 28123456 microseconds. Success: 10000000, Fail: 0, UseTLog: 0
```

- **Sent**: 发送的日志总数
- **microseconds**: 耗时（微秒）
- **Success**: 成功发送的日志数
- **Fail**: 失败的日志数
- **UseTLog**: 使用的 API (0=TGLog-ST, 1=TLog)

同时可以使用 `top` 或 `htop` 命令监控 CPU 和内存占用：

```bash
# 监控进程资源占用
top -p $(cat tglog_tapp.pid)
```

## 代码集成说明

### 初始化 TGLog-ST 客户端

```c
// 在 svr_init 中初始化
tglog_conf conf = {0};
tglog_conf_set_default(&conf);

// 设置调试日志回调（可选）
conf.write_log = tglog_client_debug_log;

// 配置服务器地址和协议
tglog_network network = TGLOG_NETWORK_TCP; // 或 TGLOG_NETWORK_UDP
tglog_conf_set_server(&conf, network, "dev.tglog.com", 20001);

// 创建客户端
g_app_context.client = tglog_client_create(&conf);
```

### 发送日志

```c
// 在 svr_proc 中发送日志
tglog_client_send(
    client,                    // 客户端实例
    log_message,               // 日志内容
    tglog_callback,            // 回调函数（可选）
    user_data,                 // 用户数据
    TGLOG_MSG_F_COPY          // 标志：复制消息内容
);
```

### 更新客户端状态

```c
// 在 svr_tick 中定期调用（建议 5ms 间隔）
tglog_client_update(client);
```

### 清理资源

```c
// 在 svr_fini 中清理
tglog_client_destroy(client);
```

## 关键实现细节

### 1. 单线程集成

TGLog-ST 是单线程版本，需要应用程序主动调用 `tglog_client_update()` 来处理网络 I/O 和发送队列。在 TAPP 框架中，这个调用放在 `svr_tick` 函数中，每 5ms 执行一次。

```c
int svr_tick(TAPPCTX *framework_ctx, void *user_data) {
    // 每 5ms 调用一次
    tglog_client_update(app_ctx->client);
    return 0;
}
```

### 2. 异步发送与回调

TGLog-ST 使用异步发送模式，通过回调函数通知发送结果：

```c
void tglog_callback(const char *payload, int err_code,
                    const char *err_msg, void *user_data) {
    if (err_code != TGLOG_ERR_OK) {
        // 处理发送失败
        printf("Log failed: %s (code: %d)\n", err_msg, err_code);
    } else {
        // 发送成功
    }
}
```

### 3. 消息标志

发送日志时使用 `TGLOG_MSG_F_COPY` 标志，让 SDK 复制消息内容。这样可以在发送后立即释放或重用消息缓冲区。

```c
tglog_client_send(client, log_message, callback, user_data, TGLOG_MSG_F_COPY);
```

### 4. 批量发送优化

为了提高性能，程序在每次 `svr_proc` 调用中批量发送多条日志：

```c
for (int i = 0; i < batch_size; i++) {
    tglog_client_send(client, log_message, callback, user_data, TGLOG_MSG_F_COPY);
}
```

批量大小通过配置文件的 `Batch` 参数控制。

### 5. 调试日志集成

TGLog-ST 的内部日志通过回调函数输出到 TAPP 的 TLog 系统：

```c
void tglog_client_debug_log(void *user_data, tglog_level level,
                            const char *file, const int line,
                            const char *func, const char *format, ...) {
    // 格式化日志消息
    char log_buf[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(log_buf, sizeof(log_buf), format, args);
    va_end(args);

    // 根据日志级别输出到 TLog
    switch (level) {
        case TGLOG_LEVEL_DEBUG:
            tlog_debug(g_app_context.app_debug_log, 0, 0, "%s", log_buf);
            break;
        case TGLOG_LEVEL_ERROR:
            tlog_error(g_app_context.app_debug_log, 0, 0, "%s", log_buf);
            break;
        // ... 其他级别
    }
}
```

## 性能监控

### 实时指标输出

程序每发送 50 万条日志会输出一次性能指标：

```c
if (sent_count % 500000 == 0) {
    char buf[4096];
    tglog_client_print_metrics(client, buf, sizeof(buf), true);
    tlog_info(app_debug_log, 0, 0, "%s", buf);
}
```

### 内存池统计

测试完成后会输出内存池使用情况：

```c
char buf[8192];
tglog_client_print_mem_pool_stats(client, buf, sizeof(buf), false);
tlog_info(app_debug_log, 0, 0, "%s", buf);
```

## 故障排除

### 1. 程序无法启动

**检查项**:
- 确认配置文件存在：`tglog_tapp_cfg.xml` 和 `tlog_cfg.xml`
- 确认 TDR 文件已生成：`tglog_tapp_def.tdr`
- 检查日志目录权限：`mkdir -p log`

### 2. 无法连接到服务器

**检查项**:
- 确认服务器地址和端口正确
- 检查网络连接：`ping dev.tglog.com`
- 查看调试日志中的连接错误信息

### 3. 发送失败率高

**可能原因**:
- 网络不稳定（UDP 模式）
- 服务器负载过高
- 批量大小过大，导致队列溢出

**解决方案**:
- 降低 `Batch` 参数值
- 使用 TCP 模式以提高可靠性
- 检查服务器状态

### 4. TLog API 无法发送日志

**症状**:
- 使用 TLog API (`UseTLog=1`) 时，日志无法发送或连接失败
- 调试日志中出现连接错误

**可能原因**:
- `tlog_cfg.xml` 中的 URL 协议前缀与 `tglog_tapp_cfg.xml` 中的 `Network` 配置不匹配

**解决方案**:
- 检查 `tglog_tapp_cfg.xml` 中的 `<Network>` 配置（tcp 或 udp）
- 确保 `tlog_cfg.xml` 中 `tglog_tapp_net` 类别的 `<Url>` 使用正确的协议前缀：
  - 如果 `Network` 是 `tcp`，URL 必须是 `tcp://dev.tglog.com:20001`
  - 如果 `Network` 是 `udp`，URL 必须是 `udp://dev.tglog.com:20001`

### 5. CPU 占用过高

**可能原因**:
- `Batch` 参数设置过大
- `timer` 间隔设置过小

**解决方案**:
- 适当降低 `Batch` 值（建议 100-500）
- 增加 `timer` 间隔（建议 5-10ms）

## 注意事项

1. **单线程模型**: TGLog-ST 必须在单线程环境中使用，不要在多个线程中调用同一个客户端实例
2. **定期更新**: 必须定期调用 `tglog_client_update()`，建议间隔 5-10ms
3. **消息复制**: 使用 `TGLOG_MSG_F_COPY` 标志可以避免消息生命周期管理问题
4. **回调处理**: 回调函数在 `update()` 调用时同步执行，避免在回调中执行耗时操作
5. **资源清理**: 程序退出前务必调用 `tglog_client_destroy()` 释放资源

## 相关文档

- [TGLog SDK 主文档](../README.md)
- [TGLog-ST API 文档](../tglog-st/)
- [CMake 构建指南](../CMAKE_BUILD_GUIDE.md)

## 技术支持

如有问题或建议，请查看项目主 README 或提交 Issue。
