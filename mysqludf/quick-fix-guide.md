# 快速修复指南

## 问题：Docker 连接 Docker Hub 失败

**错误信息**: `Error response from daemon: Get "https://registry-1.docker.io/v2/": EOF`

## 快速解决方案（按优先级排序）

### 方案 1: 最简单的修复（推荐）

**修改 docker-compose.yml，使用标准镜像标签**

```yaml
version: '3.8'

services:
  mysql:
    image: mysql:5.7  # 改为这个简单版本
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
```

**然后执行：**
```bash
# 清理现有配置
docker-compose down -v
docker system prune -f

# 启动服务
docker-compose up -d
```

### 方案 2: 国内镜像源配置

**创建或编辑 ~/.docker/config.json:**

```json
{
  "proxies": {
    "default": {
      "httpProxy": "http://http.docker.internal:3128",
      "httpsProxy": "http://http.docker.internal:3128"
    }
  },
  "registry-mirrors": [
    "https://docker.mirrors.ustc.edu.cn",
    "https://hub-mirror.c.163.com",
    "https://xx4bwyg2.mirror.aliyuncs.com"
  ]
}
```

### 方案 3: 临时网络测试

```bash
# 关闭所有代理（临时）
export no_proxy=localhost,127.0.0.1,docker.internal
export NO_PROXY=$no_proxy

# 清理Docker
docker system prune -a --volumes

# 测试基础连接
docker pull hello-world

# 如果成功，再试MySQL
docker pull mysql:5.7
```

## 验证步骤

### 检查网络连接
```bash
# 测试DNS解析
nslookup registry-1.docker.io

# 测试HTTPS连接
curl -I https://registry-1.docker.io/v2/
```

### 测试镜像拉取
```bash
# 先测试简单的镜像
docker pull hello-world

# 确认可以下载基础镜像后
docker-compose up -d
```

## 如果仍然失败？

### 1. 检查Docker状态
```bash
# 重启Docker服务
# Windows: 在任务栏右键Docker图标 -> Restart
# macOS: 重启Docker Desktop应用

# 或者命令行重启
pkill docker
dockerd &
```

### 2. 重置Docker到初始状态
```bash
# 这会删除所有容器、网络和镜像
docker system prune -a --volumes

# 重新配置Docker Desktop
# 设置 -> Reset to factory defaults
```

### 3. 使用替代方案
如果Docker持续有问题，考虑：

**直接使用MySQL安装包:**
- 下载 MySQL Installer for Windows
- 选择"Server only"选项
- 安装后配置UDF插件

**使用WSL2（Windows用户）:**
```bash
# 启用WSL2
wsl --set-default-version 2

# 安装Ubuntu
wsl --install -d Ubuntu

# 在Ubuntu中安装MySQL
sudo apt update && sudo apt install mysql-server
```

## 常见问题和解决方案

| 问题 | 解决方案 |
|------|----------|
| DNS解析失败 | 检查网络连接，尝试切换DNS服务器 |
| HTTPS连接超时 | 配置HTTP代理或使用国内镜像 |
| 权限被拒绝 | 以管理员身份运行Docker Desktop |
| 磁盘空间不足 | 清理Docker缓存 `docker system prune` |

## 紧急备用方案

如果上述方法都无效，您可以：

1. **手动安装MySQL**
   - 从官网下载MySQL Installer
   - 安装MySQL 5.7 Server
   - 配置并测试UDF功能

2. **使用云数据库**
   - 阿里云RDS MySQL 5.7
   - 腾讯云MySQL数据库
   - AWS RDS for MySQL

3. **等待网络恢复**
   - Docker Hub可能正在维护
   - 稍后再试通常能解决

## 总结

**最快解决方案**: 修改 docker-compose.yml 使用 `mysql:5.7` 标准镜像，移除复杂的插件加载命令。

**最可靠方案**: 配置国内镜像源 + 简化配置。

**最彻底方案**: 如果Docker环境问题持续，考虑使用本地MySQL安装或云服务。

选择最适合您当前情况的方案开始尝试。