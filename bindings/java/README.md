# TgLog Java演示程序使用说明

本目录包含了基于JNI的TgLog Java演示程序，实现了与C语言版本tglog_test.c相同的功能。

## 文件说明

### 核心类文件
- `TglogJNI.java` - 低级JNI接口类，定义了与tglog库交互的本地方法
- `TglogClient.java` - 高级客户端封装类，提供易用的Java API
- `TglogConfig.java` - 配置管理类，支持链式调用和默认值
- `TglogCallback.java` - 异步回调接口
- `TglogStats.java` - 结构化统计信息类
- `TglogException.java` - 专用异常类，包含错误码和类型判断
- `TglogLogLevel.java` - 日志级别枚举类
- `TglogCanLogCallback.java` - 调试日志过滤回调接口
- `TglogWriteLogCallback.java` - 调试日志写入回调接口

### 测试和示例
- `TglogTest.java` - 性能测试程序，实现了与tglog_test.c相同的功能
- `TglogClientTest.java` - 综合性能测试，演示TglogClient高级API的使用

### 底层实现
- `TglogJNI.c` - JNI的C实现，包含线程安全的回调机制和DirectByteBuffer支持
- `TglogJNI.h` - JNI头文件（使用javac -h自动生成）
- `Makefile` - 编译脚本

## 环境要求

1. **Java开发环境**
   - JDK 8或更高版本
   - 确保JAVA_HOME环境变量设置正确

2. **C编译环境**
   - GCC编译器
   - 系统需要支持JNI头文件

3. **TgLog库**
   - 确保`../lib/libtglog.so`存在且可访问
   - 相关的头文件目录：`../tglog/`, `../ck/include/`, `../event/include/`, `../jemalloc/include/`

## 编译步骤

### 自动编译（推荐）
```bash
# 编译所有文件
make all

# 或者分别编译
make compile_java    # 编译Java源文件
make compile_jni     # 编译JNI动态库
```

### 手动编译
```bash
# 1. 编译Java源文件
javac *.java

# 2. 生成JNI头文件（如果需要更新）
javac -h . TglogJNI.java

# 3. 编译JNI动态库（需要根据你的系统调整JNI头文件路径）
# Linux系统：
gcc -Wall -O2 -fPIC -shared -pthread -std=c99 \
    -I$JAVA_HOME/include -I$JAVA_HOME/include/linux \
    -I../tglog -I../ck/include -I../event/include -I../jemalloc/include \
    -o libtglog_jni.so TglogJNI.c -L../lib -ltglog

# macOS系统：
gcc -Wall -O2 -fPIC -shared -pthread -std=c99 \
    -I$JAVA_HOME/include -I$JAVA_HOME/include/darwin \
    -I../tglog -I../ck/include -I../event/include -I../jemalloc/include \
    -o libtglog_jni.so TglogJNI.c -L../lib -ltglog
```

## 运行程序

### 使用Makefile运行（推荐）
```bash
# 编译并运行基础性能测试
make run_java

# 带参数运行
make run_java_with_args

# 运行TglogClient高级API测试
make run_client_test
```

### 使用脚本运行（推荐）
```bash
# 1. 编译
make all

# 2. 使用提供的脚本运行（自动设置库路径）
./run_test.sh -t 10000000 -r 1000000 -p tcp -i dev.tglog.com -P 20001 -w 8
```

### 手动运行
```bash
# 1. 编译
make all

# 2. 设置库路径环境变量
export DYLD_LIBRARY_PATH=../lib:$DYLD_LIBRARY_PATH  # macOS
export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH      # Linux

# 3. 运行
java -Djava.library.path=. -cp . TglogTest

# 4. 带参数运行
java -Djava.library.path=. -cp . TglogTest -t 1000 -r 100 -p tcp -i 127.0.0.1 -P 20001 -w 8
```

**重要说明**：
- JNI库不链接jemalloc，因为TGLog库已经实现了内存池和对象池优化
- 推荐使用提供的 `run_test.sh` 脚本，它会自动设置正确的库路径

## 命令行参数

### TglogTest 参数
- `-t, --to-send NUM` : 发送消息数量 (默认: 10000000)
- `-r, --rate NUM` : 令牌桶填充速率 (默认: 1000000)
- `-p, --protocol PROTO` : 协议类型 udp 或 tcp (默认: tcp)
- `-i, --ip IP_ADDRESS` : 目标IP地址 (默认: 127.0.0.1)
- `-P, --port PORT` : 目标端口 (默认: 20001)
- `-w, --worker NUM` : 工作线程数 (默认: 8)
- `-d, --direct` : 使用DirectByteBuffer无回调模式（最快）
- `-D, --direct-async` : 使用DirectByteBuffer带回调模式（生产推荐）
- `-v, --verbose` : 启用调试日志（影响性能）
- `-h, --help` : 显示帮助信息

### TglogClientTest 参数
- `-h, --host HOST` : 服务器地址 (默认: 127.0.0.1)
- `-p, --port PORT` : 服务器端口 (默认: 20001)
- `-m, --messages NUM` : 发送消息数量 (默认: 1000000)
- `-w, --workers NUM` : 工作线程数 (默认: 8)
- `--tcp` : 使用TCP协议（默认）
- `--udp` : 使用UDP协议
- `--standard` : 仅测试标准模式（带回调）
- `--performance` : 仅测试性能模式（无调试回调）
- `--direct` : 仅测试DirectByteBuffer模式（无回调）
- `--direct-async` : 仅测试DirectByteBuffer模式（带回调）
- `-v, --verbose` : 显示详细指标
- `--help` : 显示帮助信息

## 性能优化模式

### 四种性能模式对比

| 模式 | 性能 (100万条) | 回调支持 | 适用场景 |
|------|---------------|---------|---------|
| 标准模式 | ~3.5秒 | ✓ | 开发调试，需要完整功能 |
| 性能模式 | ~3.0秒 | ✓ | 生产环境，禁用调试日志 |
| DirectByteBuffer无回调 | ~1.0秒 | ✗ | 极限性能，可接受消息丢失 |
| DirectByteBuffer带回调 | ~1.5-2.0秒 | ✓ | 生产推荐，平衡性能与可靠性 |

### DirectByteBuffer使用示例

```java
// 1. DirectByteBuffer无回调（最快）
try (TglogClient client = new TglogClient(TglogConfig.NETWORK_TCP, "127.0.0.1", 20001)) {
    ByteBuffer buffer = TglogClient.createDirectBuffer("log message\n");
    for (int i = 0; i < 1000000; i++) {
        buffer.position(0);  // 重置位置以复用
        client.sendDirect(buffer, messageLength);
    }
}

// 2. DirectByteBuffer带回调（生产推荐）
try (TglogClient client = new TglogClient(config)) {
    ByteBuffer buffer = TglogClient.createDirectBuffer("log message\n");
    AtomicInteger successCount = new AtomicInteger(0);
    
    TglogCallback callback = new TglogCallback() {
        @Override
        public void onSuccess(String payload) {
            successCount.incrementAndGet();
        }
        
        @Override
        public void onError(String payload, int errorCode, String errorMessage) {
            System.err.println("Failed: " + errorMessage);
        }
    };
    
    for (int i = 0; i < 1000000; i++) {
        buffer.position(0);
        client.sendDirectAsync(buffer, messageLength, callback);
    }
    
    // 等待所有回调完成
    client.waitForCompletion(60, TimeUnit.SECONDS);
    System.out.println("Success: " + successCount.get());
}
```

## 新API使用示例

### 基础使用
```java
// 简单发送
try (TglogClient client = new TglogClient(
    TglogConfig.createTcp("localhost", 8080))) {
    client.sendAsync("my_log|data|timestamp").get();
}

// 带配置和回调
TglogConfig config = TglogConfig.createTcp("localhost", 8080)
    .setWorkerNum(8)
    .setMaxConnLifetime(300);

try (TglogClient client = new TglogClient(config)) {
    CompletableFuture<Void> future = client.sendAsync("log_data", 
        new TglogCallback() {
            public void onSuccess(String payload) { 
                System.out.println("Sent: " + payload); 
            }
            public void onError(String payload, int code, String msg) { 
                System.err.println("Failed: " + msg); 
            }
        });
    future.get(5, TimeUnit.SECONDS);
}
```

### 配置管理
```java
TglogConfig config = new TglogConfig(TglogConfig.NETWORK_TCP, "192.168.1.100", 9999)
    .setWorkerNum(8)                    // 设置工作线程数
    .setConnTimeout(5000)               // 连接超时
    .setSendTimeout(10000)              // 发送超时
    .setBatchingMaxMessages(50)         // 批处理最大消息数
    .setBatchingMaxDelay(20);           // 批处理最大延迟

try (TglogClient client = new TglogClient(config)) {
    // 获取统计信息
    TglogStats stats = client.getStats();
    System.out.println("Success rate: " + stats.getSuccessRate());
    System.out.println("Queue utilization: " + stats.getQueueUtilization());
    
    // 访问单个worker统计
    if (stats.hasWorkerStats()) {
        TglogStats worker0 = stats.getWorkerStats(0);
        System.out.println("Worker 0 requests: " + worker0.getRequestMsgCount());
        
        // 获取所有worker的摘要
        String[] workerSummaries = stats.getWorkerSummaries();
        for (String summary : workerSummaries) {
            System.out.println(summary);
        }
        
        // 对比总计与worker的聚合
        System.out.println(stats.getAggregateComparison());
    }
}
```

### 调试日志配置
```java
// 启用默认调试日志（INFO级别及以上）
TglogConfig config = TglogConfig.createTcp("localhost", 8080)
    .enableDebugLogging();

// 设置特定日志级别
config.enableDebugLogging(TglogLogLevel.DEBUG);

// 自定义日志处理
config.setDebugLogging(
    level -> level.isEnabledFor(TglogLogLevel.WARN),  // 过滤器
    (level, file, line, function, message) -> {       // 写入器
        System.out.printf("[%s] %s:%d %s() - %s%n", 
                         level, file, line, function, message);
    }
);

// 禁用调试日志
config.disableDebugLogging();

// 使用预定义的日志处理器
config.setDebugLogging(TglogLogLevel.ERROR, TglogWriteLogCallback.Default.STDERR);
```

## 命令行示例用法

### TglogTest性能测试
```bash
# 发送1000条消息，使用TCP协议
java -Djava.library.path=. -cp . TglogTest -t 1000 -p tcp

# 发送10000条消息，使用UDP协议，指定服务器地址
java -Djava.library.path=. -cp . TglogTest -t 10000 -p udp -i 192.168.1.100 -P 8080

# 使用4个工作线程，发送速率为500/秒
java -Djava.library.path=. -cp . TglogTest -w 4 -r 500

# 使用DirectByteBuffer无回调模式（最快）
java -Djava.library.path=. -cp . TglogTest -t 1000000 -d

# 使用DirectByteBuffer带回调模式（生产推荐）
java -Djava.library.path=. -cp . TglogTest -t 1000000 -D

# 启用调试日志
java -Djava.library.path=. -cp . TglogTest -t 10000 -v
```

### TglogClientTest综合测试
```bash
# 测试所有模式
java -Djava.library.path=. -cp . TglogClientTest

# 仅测试DirectByteBuffer带回调模式
java -Djava.library.path=. -cp . TglogClientTest --direct-async

# 测试100万条消息并显示详细指标
java -Djava.library.path=. -cp . TglogClientTest -m 1000000 -v

# 使用UDP协议测试
java -Djava.library.path=. -cp . TglogClientTest --udp -m 100000
```

## 程序特点

### 核心特性

#### 性能优化
1. **DirectByteBuffer支持**：零拷贝发送，性能提升3-15倍
2. **线程缓存JNIEnv**：避免重复的AttachCurrentThread/DetachCurrentThread调用
3. **方法ID缓存**：缓存频繁调用的JNI方法ID
4. **四种性能模式**：从开发调试到极限性能的完整选择

#### Java API特性
1. **高级客户端封装**：提供`TglogClient`类，支持自动资源管理
2. **线程安全回调**：使用ThreadLocal存储和AttachCurrentThreadAsDaemon确保线程安全
3. **异步支持**：原生支持`CompletableFuture`和回调接口
4. **配置管理**：链式调用和合理默认值的配置类
5. **结构化统计**：提供`TglogStats`类进行性能监控，包含嵌套的worker级别统计
6. **错误处理**：专用异常类和错误类型判断
7. **调试日志**：支持注入自定义调试日志回调，支持多种日志级别和过滤方式

#### 与C版本兼容性
1. **功能对等**：实现了与tglog_test.c相同的核心功能
2. **令牌桶限流**：Java实现的令牌桶算法进行速率控制
3. **性能监控**：可获取和显示发送性能指标
4. **正确清理**：使用与C版本相同的资源清理方式
5. **原子操作**：使用ck_pr库的原子操作保证线程安全

## 测试说明

### 性能测试 (TglogTest)
```bash
# 基本性能测试
make run_java

# 测试DirectByteBuffer无回调（最快）
java -Djava.library.path=. -cp . TglogTest -t 1000000 -d

# 测试DirectByteBuffer带回调（生产推荐）
java -Djava.library.path=. -cp . TglogTest -t 1000000 -D
```

### 综合测试 (TglogClientTest)
```bash
# 测试所有四种模式
make run_client_test

# 仅测试特定模式
java -Djava.library.path=. -cp . TglogClientTest --direct-async -m 1000000
```

测试模式包括：
- **标准模式**：完整功能，带调试日志和回调
- **性能模式**：禁用调试日志，保留回调
- **DirectByteBuffer无回调**：极限性能，无消息确认
- **DirectByteBuffer带回调**：平衡性能与可靠性

## 故障排除

### 常见问题

1. **找不到JNI库**
   ```
   java.lang.UnsatisfiedLinkError: no tglog_jni in java.library.path
   ```
   解决：确保libtglog_jni.so在当前目录，并使用`-Djava.library.path=.`

2. **找不到tglog库**
   ```
   error while loading shared libraries: libtglog.so: cannot open shared object file
   ```
   解决：设置环境变量
   ```bash
   export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH  # Linux
   export DYLD_LIBRARY_PATH=../lib:$DYLD_LIBRARY_PATH  # macOS
   ```

3. **JNI头文件找不到**
   ```
   fatal error: jni.h: No such file or directory
   ```
   解决：确保JAVA_HOME正确设置

### 调试建议

1. 首先确保C版本的tglog_test能正常运行
2. 检查动态库依赖：
   ```bash
   ldd libtglog_jni.so  # Linux
   otool -L libtglog_jni.so  # macOS
   ```
3. 使用verbose模式查看详细信息：
   ```bash
   java -verbose:jni -Djava.library.path=. -cp . TglogTest
   ```

## 实现说明

### JNI优化技术

1. **线程缓存优化**
   - 使用pthread_key_t存储线程局部JNIEnv指针
   - AttachCurrentThreadAsDaemon避免阻止JVM退出
   - 从15秒优化到3.5秒（处理100万条消息）

2. **DirectByteBuffer零拷贝**
   - GetDirectBufferAddress直接访问内存
   - 避免字符串复制和编码转换
   - 从3.5秒优化到1秒（处理100万条消息）

3. **内存管理**
   - 正确管理GlobalRef生命周期
   - 避免在JVM关闭时调用DetachCurrentThread
   - 使用ck_pr原子操作保证线程安全

### 与C版本的兼容性

1. **原子操作**: 使用ck_pr库的原子操作进行计数
2. **回调机制**: 完整保留C版本的异步回调语义
3. **资源清理**: 按照C版本的顺序和时机进行清理
4. **线程模型**: 与C版本相同的多线程工作模式

这样确保了Java版本与C版本具有相同的稳定性和性能特征。

## 清理

```bash
# 清理编译生成的文件
make clean
```

这将删除所有.class文件和JNI动态库文件。