# 替代方案：不使用Docker的直接安装

## 问题诊断

您遇到的 `EOF` 错误通常是由于 Docker Hub 连接问题或代理配置导致的。以下是几种不需要依赖 Docker 的替代方案：

## 方案 1: 使用国内MySQL镜像源

### 修改 docker-compose.yml 使用国内镜像

```yaml
version: '3.8'

services:
  mysql:
    image: mysql:5.7  # 使用标准版本，避免特殊标签
    container_name: mysql-57-udf
    ports:
      - "3306:3306"
    environment:
      MYSQL_ROOT_PASSWORD: rootpassword
      MYSQL_DATABASE: testdb
      MYSQL_USER: testuser
      MYSQL_PASSWORD: testpass
    volumes:
      - ./mysql-data:/var/lib/mysql
      - ./init-scripts:/docker-entrypoint-initdb.d
      - ./plugins:/usr/lib/mysql/plugin
    command: [
      '--default-authentication-plugin=mysql_native_password',
      '--character-set-server=utf8mb4',
      '--collation-server=utf8mb4_unicode_ci'
    ]
    restart: unless-stopped

volumes:
  mysql-data:

networks:
  mysql-network:
    driver: bridge
```

### 关键更改说明

1. **移除特殊标签**: 使用标准 `mysql:5.7` 而不是 `mysql:5.7.44-linux-x86_64`
2. **简化配置**: 移除了可能引起问题的复杂插件加载命令
3. **保持基本功能**: 仍然支持 UDF 插件加载

## 方案 2: 使用离线安装包

### Windows 系统

1. **下载 MySQL 安装包**
   - 访问 https://dev.mysql.com/downloads/mysql/5.7.html
   - 选择 Windows (x86, 64-bit) MSI Installer

2. **安装步骤**
   ```bash
   # 以管理员身份运行
   mysqld --install
   mysqld --initialize --console
   net start mysql
   ```

3. **安装 UDF HTTP 插件**
   - 编译插件或直接下载预编译版本
   - 复制到 MySQL plugin 目录
   - 在 my.ini 中添加插件加载配置

## 方案 3: 使用虚拟机环境

### 使用 VirtualBox + Ubuntu

1. **创建 Ubuntu 虚拟机**
   ```bash
   # 安装 VirtualBox
   # 下载 Ubuntu 20.04 LTS ISO

   # 在虚拟机中安装MySQL
   sudo apt update
   sudo apt install mysql-server
   ```

2. **配置MySQL**
   ```sql
   sudo mysql_secure_installation
   sudo systemctl enable mysql
   ```

3. **安装UDF插件**
   ```bash
   sudo apt install build-essential libmysqlclient-dev
   git clone https://github.com/maechler/mysqludf_http.git
   cd mysqludf_http
   make
   sudo cp *.so /usr/lib/mysql/plugin/
   ```

## 方案 4: 使用云数据库服务

### 阿里云RDS MySQL 5.7

1. **创建RDS实例**
   - 登录阿里云控制台
   - 创建 MySQL 5.7 实例
   - 设置白名单允许本地连接

2. **连接配置**
   ```python
   import pymysql

   connection = pymysql.connect(
       host='your-rds-instance.cn-hangzhou.rds.aliyuncs.com',
       port=3306,
       user='your-username',
       password='your-password',
       database='testdb',
       charset='utf8mb4'
   )
   ```

## 方案 5: 使用轻量级容器替代

### 使用 Podman 替代 Docker

```yaml
# podman-compose.yml
version: '3'

services:
  mysql:
    image: mysql:5.7
    network_mode: host
    environment:
      MYSQL_ROOT_PASSWORD: rootpassword
    volumes:
      - ./data:/var/lib/mysql
```

### 使用 Docker Desktop WSL2 后端

```bash
# 确保使用WSL2后端
wsl --set-default-version 2
# 重启Docker Desktop
```

## 推荐的最佳实践

### 1. 网络优化配置

```bash
# 修改 Docker daemon.json
{
  "registry-mirrors": [
    "https://docker.mirrors.ustc.edu.cn",
    "https://hub-mirror.c.163.com",
    "https://xx4bwyg2.mirror.aliyuncs.com"
  ],
  "insecure-registries": [],
  "debug": true,
  "experimental": false
}
```

### 2. 分步验证流程

```bash
# 步骤1: 测试基础连接
curl -I https://registry-1.docker.io/v2/

# 步骤2: 尝试拉取简单镜像
docker pull hello-world

# 步骤3: 如果成功，再尝试MySQL
docker pull mysql:5.7

# 步骤4: 启动您的应用
docker-compose up -d
```

### 3. 故障排除脚本

```bash
#!/bin/bash
# fix-docker-connection.sh

echo "=== Docker Connection Troubleshooting ==="

# 检查网络连接
echo "1. Testing network connectivity..."
ping -c 3 registry-1.docker.io

# 检查DNS解析
echo "2. Checking DNS resolution..."
nslookup registry-1.docker.io

# 测试HTTPS连接
echo "3. Testing HTTPS connection..."
curl -v https://registry-1.docker.io/v2/ 2>&1 | head -10

# 清理Docker缓存
echo "4. Cleaning Docker cache..."
docker system prune -f

# 重新配置代理（如果需要）
echo "5. Checking proxy settings..."
if [ ! -z "$HTTP_PROXY" ]; then
    echo "Proxy configured: $HTTP_PROXY"
fi

echo "=== Troubleshooting Complete ==="
```

## 总结

根据您的具体情况，我推荐以下优先级方案：

1. **首选**: 修改 docker-compose.yml 使用标准 MySQL 镜像，简化配置
2. **备选**: 配置国内镜像源和加速器
3. **最后手段**: 考虑虚拟机或云服务方案

这些方案都不需要复杂的网络调试，能够快速解决问题并继续您的 MySQL UDF 开发工作。