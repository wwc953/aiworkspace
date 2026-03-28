# MySQL UDF HTTP 插件安装指南

## 快速安装步骤

### 1. 准备插件文件
将编译好的 `http_udf.so` (Linux) 或 `http_udf.dll` (Windows) 文件复制到 `plugins/` 目录。

### 2. 启动服务
```bash
docker-compose up -d
```

### 3. 验证插件加载
```sql
-- 连接到 MySQL
mysql -h 127.0.0.1 -P 3306 -u root -p

-- 检查插件是否已加载
SHOW PLUGINS;

-- 或者查询特定插件
SELECT * FROM mysql.plugin WHERE name LIKE '%http%';

-- 手动加载插件（如果未自动加载）
INSTALL PLUGIN http_udf SONAME 'http_udf.so';
```

### 4. 测试插件功能
```sql
-- 测试 HTTP GET 请求
SELECT http_get('https://api.github.com/users/octocat');

-- 测试 HTTP POST 请求
SELECT http_post('https://httpbin.org/post', '{"test": "data"}');

-- 查看函数定义
SHOW FUNCTION STATUS WHERE db = 'testdb';
```

## 常见问题解决

### 插件无法加载
1. **检查文件权限**
   ```bash
   chmod 644 plugins/http_udf.so
   ```

2. **查看错误日志**
   ```bash
   docker logs mysql-57-udf
   ```

3. **验证插件兼容性**
   - 确保插件与 MySQL 5.7 版本兼容
   - 检查系统架构匹配（x86_64, arm64等）

### 连接问题
1. **确保容器已完全启动**
   ```bash
   docker-compose ps
   ```

2. **检查端口映射**
   ```bash
   netstat -tlnp | grep 3306
   ```

3. **验证网络连接**
   ```bash
   telnet 127.0.0.1 3306
   ```

## 开发环境搭建

如需编译 mysql-udf-http 插件：

### Ubuntu/Debian
```bash
apt-get update
apt-get install build-essential libmysqlclient-dev git

git clone https://github.com/maechler/mysqludf_http.git
cd mysqludf_http
make
sudo cp *.so /usr/lib/mysql/plugin/
```

### CentOS/RHEL
```bash
yum install gcc mysql-devel git

git clone https://github.com/maechler/mysqludf_http.git
cd mysqludf_http
make
cp *.so /usr/lib64/mysql/plugin/
```

## 性能优化建议

1. **连接池配置**
   ```ini
   [mysqld]
   max_connections=200
   thread_cache_size=10
   ```

2. **HTTP 超时设置**
   ```sql
   SET GLOBAL http_timeout=30;
   ```

3. **缓存策略**
   ```sql
   -- 启用查询缓存
   SET GLOBAL query_cache_type=1;
   SET GLOBAL query_cache_size=67108864; -- 64MB
   ```

## 安全注意事项

1. **限制插件权限**
   ```sql
   -- 创建专用用户
   CREATE USER 'udf_user'@'%' IDENTIFIED BY 'secure_password';
   GRANT SELECT ON testdb.* TO 'udf_user'@'%';
   ```

2. **防火墙配置**
   ```bash
   # 允许 MySQL 端口
   iptables -A INPUT -p tcp --dport 3306 -j ACCEPT

   # 限制外部访问
   iptables -A INPUT -p tcp --dport 3306 -s 127.0.0.1 -j ACCEPT
   iptables -A INPUT -p tcp --dport 3306 -j DROP
   ```

3. **SSL 加密**
   ```ini
   [mysqld]
   ssl-ca=/path/to/ca-cert.pem
   ssl-cert=/path/to/server-cert.pem
   ssl-key=/path/to/server-key.pem
   ```

## 故障排除命令

```bash
# 查看容器状态
docker-compose ps

# 查看日志
docker-compose logs mysql-57-udf

# 进入 MySQL 容器
docker exec -it mysql-57-udf bash

# 检查 MySQL 错误日志
tail -f /var/log/mysql/error.log

# 重新加载配置
docker-compose restart mysql
```