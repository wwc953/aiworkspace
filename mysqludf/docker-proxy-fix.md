# Docker 代理配置解决方案

## HTTP Proxy 配置说明

**当前配置**: `HTTP Proxy: http.docker.internal:3128`

这意味着：
- **代理地址**: `http.docker.internal`
- **端口**: `3128`
- **协议**: `HTTP` (不是 HTTPS)

## 错误原因分析

您遇到的错误：
```
Error response from daemon: Get "https://registry-1.docker.io/v2/": EOF
```

这通常是由于以下原因之一：
1. **代理配置问题** - HTTP 代理无法正确处理 HTTPS 连接
2. **网络连接中断** - 代理服务器不可用或超时
3. **镜像拉取失败** - Docker Hub 连接不稳定

## 解决方案

### 方案 1: 修改代理配置 (推荐)

编辑 Docker 配置，使用 HTTPS 代理：

```json
{
  "proxies": {
    "default": {
      "httpProxy": "http://http.docker.internal:3128",
      "httpsProxy": "http://http.docker.internal:3128"
    }
  }
}
```

### 方案 2: 使用国内镜像源

在 Docker Compose 文件中添加镜像配置：

```yaml
version: '3.8'

services:
  mysql:
    image: mysql:5.7
    # ... 其他配置 ...
    environment:
      - DOCKER_REGISTRY_MIRROR=https://hub-mirror.c.163.com
```

### 方案 3: 临时关闭代理测试

```bash
# 停止Docker服务
docker stop docker

# 编辑配置文件，注释掉代理设置
# 然后重新启动Docker服务

# 或者创建一个新的配置文件覆盖原有设置
mkdir -p ~/.docker
cat > ~/.docker/config.json << 'EOF'
{
  "proxies": {}
}
EOF
```

### 方案 4: 使用 Daocloud 镜像加速

```bash
# 配置Daocloud加速器
curl -sSL https://get.daocloud.io/docker | sh
sudo usermod -aG docker $USER

# 配置镜像加速器
sudo mkdir -p /etc/docker
sudo tee /etc/docker/daemon.json <<-'EOF'
{
  "registry-mirrors": ["https://f1361db2.m.daocloud.io"]
}
EOF
sudo systemctl daemon-reload
sudo systemctl restart docker
```

## 验证解决方案

### 检查代理状态
```bash
# 查看当前代理配置
docker info | grep -A 5 -B 5 proxy

# 测试网络连接
curl -v https://registry-1.docker.io/v2/
```

### 测试镜像拉取
```bash
# 尝试拉取镜像
docker pull mysql:5.7

# 如果成功，启动您的服务
docker-compose up -d
```

## 推荐的最终配置

### 1. 更新 docker-compose.yml

```yaml
version: '3.8'

services:
  mysql:
    image: mysql:5.7
    container_name: mysql-57-udf
    ports:
      - "3306:3306"
    environment:
      MYSQL_ROOT_PASSWORD: rootpassword
      MYSQL_DATABASE: testdb
      MYSQL_USER: testuser
      MYSQL_PASSWORD: testpass
      # 添加镜像加速器
      - DOCKER_CONTENT_TRUST=1
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
```

### 2. 清理和重建

```bash
# 清理现有容器和镜像
docker-compose down -v
docker system prune -a

# 重新拉取并启动
docker-compose up -d --build
```

## 故障排除

### 如果仍然无法连接

1. **检查网络连接**
   ```bash
   ping registry-1.docker.io
   telnet registry-1.docker.io 443
   ```

2. **重置Docker网络**
   ```bash
   # 重置Docker到初始状态
   docker system prune -a --volumes
   ```

3. **检查防火墙设置**
   ```bash
   # Windows Defender 或其他防火墙可能阻止连接
   ```

### 日志分析

```bash
# 查看详细日志
docker logs --tail 100 mysql-57-udf

# 检查Docker守护进程日志
journalctl -u docker.service --since "1 hour ago"
```

## 总结

HTTP Proxy 配置是导致连接问题的根本原因。建议：
1. 使用国内镜像源替代 Docker Hub
2. 确保代理配置正确（特别是HTTPS支持）
3. 考虑使用 Daocloud 等国内加速器

选择最适合您网络环境的方案进行配置。