# VeloZ 部署和运维文档

## 1. 部署概述

VeloZ 是一个面向加密货币市场的工业级量化交易框架，提供统一的回测/模拟/实盘交易运行时模型和可演进的工程架构。本文档将介绍各种部署方式的详细步骤和注意事项，以及系统监控、故障排除、备份和恢复、升级和维护等运维任务的最佳实践。

## 2. 系统要求

### 2.1 硬件要求

- **CPU**：至少 8 核，推荐 16 核或更多（支持 AVX2 或 AVX512 指令集）
- **内存**：至少 16GB，推荐 32GB 或更多
- **存储**：至少 100GB SSD 可用空间（推荐 NVMe SSD）
- **网络**：稳定的网络连接，推荐 1Gbps 或更快
- **磁盘 I/O**：至少 1000 IOPS（推荐 NVMe SSD）

### 2.2 软件要求

- **操作系统**：Linux（推荐 Ubuntu 22.04 LTS 或 CentOS 8），Windows 或 macOS（仅用于开发环境）
- **编译器**：GCC 11 或更高版本，支持 C++23 标准
- **构建工具**：CMake 3.26 或更高版本
- **依赖管理**：vcpkg 或 Conan（推荐使用）
- **Python**：Python 3.10 或更高版本
- **数据库**：
  - Redis 7.0 或更高版本（用于策略配置和状态存储）
  - PostgreSQL 14 或更高版本（用于历史数据存储）
  - ClickHouse 23.0 或更高版本（用于高频数据存储）

## 3. 本地部署

### 3.1 环境准备

#### 3.1.1 安装依赖

```bash
# Ubuntu/Debian
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    git \
    curl \
    python3-pip \
    libssl-dev \
    libcurl4-openssl-dev \
    libjsoncpp-dev \
    redis-server \
    postgresql \
    postgresql-contrib \
    libpq-dev

# CentOS/RHEL
sudo yum install -y \
    gcc gcc-c++ \
    make \
    cmake \
    git \
    curl \
    python3-pip \
    openssl-devel \
    libcurl-devel \
    jsoncpp-devel \
    redis \
    postgresql \
    postgresql-devel
```

#### 3.1.2 安装 vcpkg（推荐）

```bash
git clone https://github.com/microsoft/vcpkg
cd vcpkg
./bootstrap-vcpkg.sh
./vcpkg install nlohmann-json:${VCPKG_TARGET_TRIPLET} spdlog:${VCPKG_TARGET_TRIPLET} boost:${VCPKG_TARGET_TRIPLET}
```

#### 3.1.3 安装 ClickHouse（可选）

```bash
# Ubuntu/Debian
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv E0C56BD4
echo "deb http://repo.yandex.ru/clickhouse/deb/stable/ main/" | sudo tee /etc/apt/sources.list.d/clickhouse.list
sudo apt update && sudo apt install -y clickhouse-server clickhouse-client

# CentOS/RHEL
sudo yum install -y yum-utils
sudo rpm --import https://repo.clickhouse.com/CLICKHOUSE-KEY.GPG
sudo yum-config-manager --add-repo https://repo.clickhouse.com/rpm/stable/x86_64
sudo yum install -y clickhouse-server clickhouse-client
```

### 3.2 构建项目

#### 3.2.1 克隆代码

```bash
git clone https://github.com/your-username/VeloZ.git
cd VeloZ
```

#### 3.2.2 配置 CMake

```bash
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
```

#### 3.2.3 构建项目

```bash
cmake --build . --config Release -j$(nproc)
```

### 3.3 配置系统

#### 3.3.1 配置 Redis

```bash
# 启动 Redis 服务器
sudo systemctl start redis-server
sudo systemctl enable redis-server

# 验证 Redis 连接
redis-cli ping
```

#### 3.3.2 配置 PostgreSQL

```bash
# 启动 PostgreSQL 服务器
sudo systemctl start postgresql
sudo systemctl enable postgresql

# 创建数据库和用户
sudo -u postgres createdb veloz
sudo -u postgres createuser -P veloz
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE veloz TO veloz;"
```

#### 3.3.3 配置 ClickHouse（可选）

```bash
# 启动 ClickHouse 服务器
sudo systemctl start clickhouse-server
sudo systemctl enable clickhouse-server

# 验证 ClickHouse 连接
clickhouse-client
```

#### 3.3.4 配置 API 密钥

```bash
# 创建 API 密钥配置文件
mkdir -p ~/.veloz
cat > ~/.veloz/api_keys.json << 'EOF'
{
    "binance": {
        "api_key": "your_binance_api_key",
        "api_secret": "your_binance_api_secret"
    },
    "okx": {
        "api_key": "your_okx_api_key",
        "api_secret": "your_okx_api_secret",
        "passphrase": "your_okx_passphrase"
    }
}
EOF

# 设置文件权限
chmod 600 ~/.veloz/api_keys.json
```

### 3.4 启动系统

#### 3.4.1 启动核心引擎

```bash
cd /path/to/VeloZ/build
./veloz_core --config ../config/core_config.json
```

#### 3.4.2 启动策略管理服务

```bash
cd /path/to/VeloZ/build
./veloz_strategy --config ../config/strategy_config.json
```

#### 3.4.3 启动市场数据服务

```bash
cd /path/to/VeloZ/build
./veloz_market --config ../config/market_config.json
```

#### 3.4.4 启动 API 服务器

```bash
cd /path/to/VeloZ
python3 -m pip install -r requirements.txt
python3 api_server.py --config config/api_config.json
```

### 3.5 验证系统

```bash
# 验证 API 服务器是否正常运行
curl -X GET http://localhost:8080/api/v1/health

# 验证核心引擎是否正常运行
curl -X GET http://localhost:8080/api/v1/core/status

# 验证策略管理服务是否正常运行
curl -X GET http://localhost:8080/api/v1/strategy/status

# 验证市场数据服务是否正常运行
curl -X GET http://localhost:8080/api/v1/market/status
```

## 4. Docker 容器部署

### 4.1 构建 Docker 镜像

```bash
cd /path/to/VeloZ
docker build -t veloz:latest -f infra/docker/Dockerfile .
```

### 4.2 运行 Docker 容器

```bash
# 创建网络
docker network create veloz-network

# 启动 Redis 容器
docker run -d --name veloz-redis --network veloz-network redis:7.0

# 启动 PostgreSQL 容器
docker run -d --name veloz-postgres --network veloz-network \
    -e POSTGRES_DB=veloz \
    -e POSTGRES_USER=veloz \
    -e POSTGRES_PASSWORD=veloz123 \
    postgres:14

# 启动 ClickHouse 容器（可选）
docker run -d --name veloz-clickhouse --network veloz-network \
    -e CLICKHOUSE_DB=veloz \
    -e CLICKHOUSE_USER=veloz \
    -e CLICKHOUSE_PASSWORD=veloz123 \
    yandex/clickhouse-server:23.0

# 启动 VeloZ 核心引擎容器
docker run -d --name veloz-core --network veloz-network \
    --env-file .env \
    -v /path/to/veloz/config:/app/config \
    -v /path/to/veloz/logs:/app/logs \
    veloz:latest \
    ./veloz_core --config /app/config/core_config.json

# 启动 VeloZ 策略管理服务容器
docker run -d --name veloz-strategy --network veloz-network \
    --env-file .env \
    -v /path/to/veloz/config:/app/config \
    -v /path/to/veloz/logs:/app/logs \
    veloz:latest \
    ./veloz_strategy --config /app/config/strategy_config.json

# 启动 VeloZ 市场数据服务容器
docker run -d --name veloz-market --network veloz-network \
    --env-file .env \
    -v /path/to/veloz/config:/app/config \
    -v /path/to/veloz/logs:/app/logs \
    veloz:latest \
    ./veloz_market --config /app/config/market_config.json

# 启动 VeloZ API 服务器容器
docker run -d --name veloz-api --network veloz-network \
    --env-file .env \
    -v /path/to/veloz/config:/app/config \
    -v /path/to/veloz/logs:/app/logs \
    -p 8080:8080 \
    veloz:latest \
    python3 -m pip install -r requirements.txt && python3 api_server.py --config /app/config/api_config.json
```

### 4.3 验证系统

```bash
# 验证 API 服务器是否正常运行
curl -X GET http://localhost:8080/api/v1/health

# 验证核心引擎是否正常运行
curl -X GET http://localhost:8080/api/v1/core/status
```

## 5. Kubernetes 部署

### 5.1 准备 Kubernetes 集群

```bash
# 安装 kubectl（如果尚未安装）
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x ./kubectl
sudo mv ./kubectl /usr/local/bin/kubectl

# 验证集群连接
kubectl cluster-info
```

### 5.2 部署 VeloZ

```bash
cd /path/to/VeloZ/infra/k8s
kubectl apply -f veloz-namespace.yaml
kubectl apply -f veloz-redis.yaml
kubectl apply -f veloz-postgres.yaml
kubectl apply -f veloz-clickhouse.yaml  # 可选
kubectl apply -f veloz-core.yaml
kubectl apply -f veloz-strategy.yaml
kubectl apply -f veloz-market.yaml
kubectl apply -f veloz-api.yaml
```

### 5.3 验证部署

```bash
# 查看部署状态
kubectl get deployments -n veloz

# 查看服务状态
kubectl get services -n veloz

# 查看 Pod 状态
kubectl get pods -n veloz

# 验证 API 服务器是否正常运行
kubectl port-forward service/veloz-api 8080:8080 -n veloz
curl -X GET http://localhost:8080/api/v1/health
```

## 6. 系统监控

### 6.1 日志监控

```bash
# 查看核心引擎日志
tail -f /path/to/VeloZ/logs/core.log

# 查看策略管理服务日志
tail -f /path/to/VeloZ/logs/strategy.log

# 查看市场数据服务日志
tail -f /path/to/VeloZ/logs/market.log

# 查看 API 服务器日志
tail -f /path/to/VeloZ/logs/api.log
```

### 6.2 性能监控

```bash
# 查看系统资源使用情况
top

# 查看网络连接
ss -tuln

# 查看磁盘使用情况
df -h

# 查看内存使用情况
free -h

# 查看磁盘 I/O 情况
iostat -x 1

# 查看网络流量
iftop
```

### 6.3 健康检查

```bash
# 检查 API 服务器健康状态
curl -X GET http://localhost:8080/api/v1/health

# 检查核心引擎健康状态
curl -X GET http://localhost:8080/api/v1/core/status

# 检查策略管理服务健康状态
curl -X GET http://localhost:8080/api/v1/strategy/status

# 检查市场数据服务健康状态
curl -X GET http://localhost:8080/api/v1/market/status
```

### 6.4 指标监控

```bash
# 查看系统指标（使用 Prometheus 和 Grafana）
# 1. 安装 Prometheus 和 Grafana
# 2. 配置 Prometheus 抓取 VeloZ 指标
# 3. 导入 VeloZ 监控仪表盘
```

## 7. 故障排除

### 7.1 常见问题

#### 7.1.1 服务无法启动

```bash
# 检查日志文件
tail -f /path/to/VeloZ/logs/core.log
tail -f /path/to/VeloZ/logs/api.log

# 检查配置文件
cat /path/to/VeloZ/config/core_config.json
cat /path/to/VeloZ/config/api_config.json

# 检查端口是否被占用
ss -tuln | grep 8080

# 检查依赖是否安装
ldd /path/to/VeloZ/build/veloz_core
```

#### 7.1.2 无法连接到交易所

```bash
# 检查网络连接
ping api.binance.com

# 检查 API 密钥配置
cat ~/.veloz/api_keys.json

# 检查防火墙设置
sudo ufw status

# 检查代理设置
echo $HTTP_PROXY
echo $HTTPS_PROXY
```

#### 7.1.3 策略无法正常运行

```bash
# 检查策略配置
cat /path/to/VeloZ/config/strategy_config.json

# 检查策略状态
curl -X GET http://localhost:8080/api/v1/strategies

# 检查策略日志
tail -f /path/to/VeloZ/logs/strategy.log
```

#### 7.1.4 数据库连接失败

```bash
# 检查 Redis 连接
redis-cli ping

# 检查 PostgreSQL 连接
psql -h localhost -U veloz -d veloz -c "SELECT 1;"

# 检查 ClickHouse 连接（可选）
clickhouse-client -h localhost -u veloz -d veloz -p veloz123 -q "SELECT 1;"
```

### 7.2 日志分析

```bash
# 分析错误日志
grep -i "error" /path/to/VeloZ/logs/*.log

# 分析警告日志
grep -i "warning" /path/to/VeloZ/logs/*.log

# 分析时间戳在特定范围内的日志
grep "2023-06-01" /path/to/VeloZ/logs/core.log

# 使用 ELK Stack 分析日志（可选）
# 1. 安装 Elasticsearch、Logstash 和 Kibana
# 2. 配置 Logstash 收集 VeloZ 日志
# 3. 在 Kibana 中可视化和分析日志
```

## 8. 备份和恢复

### 8.1 备份策略

```bash
# 备份策略配置
cp /path/to/VeloZ/config/strategy_config.json /path/to/backup/strategy_config.json

# 备份策略参数
cp /path/to/VeloZ/config/strategy_parameters.json /path/to/backup/strategy_parameters.json

# 备份策略状态
cp /path/to/VeloZ/data/strategy_state.json /path/to/backup/strategy_state.json
```

### 8.2 备份数据

```bash
# 备份历史数据（CSV 文件）
cp -r /path/to/VeloZ/data/historical /path/to/backup/historical

# 备份交易记录（PostgreSQL）
pg_dump -h localhost -U veloz -d veloz -t trades > /path/to/backup/trades.sql

# 备份风险评估数据（ClickHouse，可选）
clickhouse-client -h localhost -u veloz -d veloz -p veloz123 -q "SELECT * FROM risk_assessment" > /path/to/backup/risk_assessment.tsv
```

### 8.3 恢复策略

```bash
# 恢复策略配置
cp /path/to/backup/strategy_config.json /path/to/VeloZ/config/strategy_config.json

# 恢复策略参数
cp /path/to/backup/strategy_parameters.json /path/to/VeloZ/config/strategy_parameters.json

# 恢复策略状态
cp /path/to/backup/strategy_state.json /path/to/VeloZ/data/strategy_state.json

# 重启策略管理服务
sudo systemctl restart veloz-strategy
```

### 8.4 恢复数据

```bash
# 恢复历史数据
cp -r /path/to/backup/historical /path/to/VeloZ/data/historical

# 恢复交易记录
psql -h localhost -U veloz -d veloz -f /path/to/backup/trades.sql

# 恢复风险评估数据（ClickHouse，可选）
clickhouse-client -h localhost -u veloz -d veloz -p veloz123 -q "INSERT INTO risk_assessment FORMAT TSV" < /path/to/backup/risk_assessment.tsv

# 重启市场数据服务
sudo systemctl restart veloz-market
```

## 9. 升级和维护

### 9.1 升级步骤

```bash
# 备份当前版本
cp -r /path/to/VeloZ /path/to/VeloZ.backup

# 拉取最新代码
cd /path/to/VeloZ
git pull

# 重新构建项目
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release -j$(nproc)

# 重启服务
sudo systemctl restart veloz-core
sudo systemctl restart veloz-strategy
sudo systemctl restart veloz-market
sudo systemctl restart veloz-api
```

### 9.2 维护任务

```bash
# 清理临时文件
rm -rf /path/to/VeloZ/build/*
rm -rf /path/to/VeloZ/logs/*.log.*
rm -rf /path/to/VeloZ/data/*.tmp

# 压缩日志文件
gzip /path/to/VeloZ/logs/*.log

# 清理历史数据（保留最近 365 天）
find /path/to/VeloZ/data/historical -name "*.csv" -type f -mtime +365 -delete

# 优化数据库
sudo -u postgres psql -d veloz -c "VACUUM ANALYZE;"  # PostgreSQL
clickhouse-client -h localhost -u veloz -d veloz -p veloz123 -q "OPTIMIZE TABLE trades FINAL"  # ClickHouse（可选）
```

## 10. 安全最佳实践

### 10.1 访问控制

```bash
# 限制 API 访问
ufw allow from 192.168.1.0/24 to any port 8080
ufw deny from any to any port 8080

# 配置防火墙
ufw enable
ufw status

# 限制 SSH 访问
sudo sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin no/' /etc/ssh/sshd_config
sudo sed -i 's/#PasswordAuthentication yes/PasswordAuthentication no/' /etc/ssh/sshd_config
sudo systemctl restart ssh
```

### 10.2 API 密钥安全

```bash
# 生成强密码
openssl rand -hex 32

# 使用环境变量存储 API 密钥
export BINANCE_API_KEY="your_binance_api_key"
export BINANCE_API_SECRET="your_binance_api_secret"

# 使用加密存储（可选）
gpg --symmetric ~/.veloz/api_keys.json
rm -f ~/.veloz/api_keys.json
```

### 10.3 数据加密

```bash
# 加密配置文件
gpg --symmetric /path/to/VeloZ/config/core_config.json
gpg --symmetric /path/to/VeloZ/config/api_config.json

# 加密日志文件
gpg --symmetric /path/to/VeloZ/logs/core.log
gpg --symmetric /path/to/VeloZ/logs/api.log

# 使用加密磁盘分区（可选）
sudo cryptsetup luksFormat /dev/sdb1
sudo cryptsetup luksOpen /dev/sdb1 veloz-data
sudo mkfs.ext4 /dev/mapper/veloz-data
sudo mount /dev/mapper/veloz-data /path/to/VeloZ/data
```

### 10.4 网络安全

```bash
# 配置 TLS（HTTPS）
# 1. 生成 SSL 证书
# 2. 配置 API 服务器使用 HTTPS
# 3. 配置负载均衡器使用 HTTPS

# 配置防火墙规则
ufw default deny incoming
ufw default allow outgoing
ufw allow ssh
ufw allow 8080/tcp
ufw enable
```

## 11. 性能优化

### 11.1 系统优化

```bash
# 调整内核参数
echo "net.core.rmem_max = 16777216" >> /etc/sysctl.conf
echo "net.core.wmem_max = 16777216" >> /etc/sysctl.conf
echo "net.core.rmem_default = 16777216" >> /etc/sysctl.conf
echo "net.core.wmem_default = 16777216" >> /etc/sysctl.conf
sysctl -p

# 调整文件描述符限制
echo "* soft nofile 65536" >> /etc/security/limits.conf
echo "* hard nofile 65536" >> /etc/security/limits.conf

# 调整内存分配
echo "vm.swappiness = 10" >> /etc/sysctl.conf
sysctl -p

# 调整 CPU 亲和性（可选）
taskset -cp 0-3 /path/to/VeloZ/build/veloz_core
```

### 11.2 应用优化

```bash
# 调整核心引擎参数
sed -i 's/"max_connections": 100/"max_connections": 500/' /path/to/VeloZ/config/core_config.json

# 调整策略管理服务参数
sed -i 's/"strategy_thread_count": 4/"strategy_thread_count": 8/' /path/to/VeloZ/config/strategy_config.json

# 调整市场数据服务参数
sed -i 's/"market_update_interval": 100/"market_update_interval": 50/' /path/to/VeloZ/config/market_config.json

# 调整数据库参数
# PostgreSQL：调整 shared_buffers、work_mem、maintenance_work_mem 等参数
# ClickHouse：调整 max_memory_usage、max_threads 等参数
```

### 11.3 存储优化

```bash
# 使用 SSD 或 NVMe 存储
# 配置磁盘 RAID（可选）
# 调整文件系统参数
```

## 12. 总结

VeloZ 提供了多种部署方式，包括本地部署、Docker 容器部署和云部署（如 Kubernetes）。本文档介绍了各种部署方式的详细步骤和注意事项，以及系统监控、故障排除、备份和恢复、升级和维护等运维任务的最佳实践。

通过遵循这些步骤，可以确保系统的稳定运行和高效维护，从而为量化交易策略的开发和部署提供可靠的支持。
