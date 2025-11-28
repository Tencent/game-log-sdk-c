# TGLog 测试环境搭建指南

本文档介绍如何快速搭建 TGLog 测试环境。

## 环境要求

- **服务器数量**: 2 台
- **磁盘空间**: 每台至少 100GB 可用空间
- **操作系统**: Linux
- **网络**: 客户端能访问服务器的 20001 端口（TCP）

## 服务端部署

在**两台服务器**上分别执行以下操作。

### 1. 创建部署目录

```bash
sudo mkdir -p /data/tglog
sudo chown $USER:$USER /data/tglog
```

### 2. 上传服务端文件

将项目中的 `server` 目录上传到服务器：

```bash
# 在本地项目目录执行
scp -r server/ user@server1:/data/tglog/
scp -r server/ user@server2:/data/tglog/
```

### 3. 设置执行权限

```bash
cd /data/tglog/server
chmod +x bin/*.sh bin/tglog
```

### 4. 启动服务

```bash
cd /data/tglog/server/bin
./start.sh
```

启动成功后会看到类似输出：

```
{"level":"warn","ts":"2025-11-28T14:35:55.094+0800","msg":"using local config","path":"../cfg/simplest.yaml"}

pid is 1207
```

## 客户端配置

### 1. 获取服务器 IP 地址

在服务器上执行：

```bash
ip addr show | grep "inet " | grep -v 127.0.0.1
```

假设获取到的 IP 地址为：
- 服务器 1: `192.168.1.10`
- 服务器 2: `192.168.1.11`

### 2. 配置 hosts 文件

在**客户端机器**上编辑 `/etc/hosts` 文件：

**Linux/macOS**:

```bash
sudo vi /etc/hosts
```

### 3. 验证连接

```bash
# 测试域名解析
ping dev.tglog.com

# 测试端口连通
telnet dev.tglog.com 20001
```

## 测试验证

### 发送测试日志

```bash
# 使用 telnet
telnet dev.tglog.com 20001
# 输入: testlog|field1|field2|field3

# 或使用 nc
echo "testlog|field1|field2|field3" | nc dev.tglog.com 20001

# 或
echo "testlog|field1|field2|field3" > /dev/tcp/dev.tglog.com/20001
```

### 查看服务端日志

在服务器上查看接收到的日志：

```bash
cd /data/tglog/server/data
tail -f all-*.log
```

---

**说明**:
- 服务监听端口: 20001 (TCP)
- 日志存储目录: `/data/tglog/server/data/`
- 启动/停止脚本: `/data/tglog/server/bin/start.sh` 和 `stop.sh`
