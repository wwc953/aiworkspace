# MySQL 5.7 with UDF HTTP Plugin

这是一个使用 Docker Compose 部署的 MySQL 5.7 环境，包含了初始化脚本和配置。

## 快速开始

1. **启动服务**
   ```bash
   docker-compose up -d
   ```

2. **连接到 MySQL**
   ```bash
   # 使用 root 用户
   mysql -h 127.0.0.1 -P 3306 -u root -p

   # 或使用 testuser 用户
   mysql -h 127.0.0.1 -P 3306 -u testuser -p
   ```

3. **验证连接**
   ```sql
   USE testdb;
   SELECT * FROM users;
   ```

## 连接信息

- **主机**: localhost
- **端口**: 3306
- **root 密码**: rootpassword
- **testuser 密码**: testpass
- **数据库**: testdb

## 目录结构

```
mysqludf/
├── docker-compose.yml          # Docker Compose 配置文件
├── init-scripts/
│   └── setup-udf.sql          # 初始化 SQL 脚本
├── mysql-data/                # MySQL 数据卷（自动创建）
└── README.md                  # 此文件
```

## 停止服务

```bash
docker-compose down
```

## 清理数据

如果需要完全清理数据（包括持久化的数据库文件）：

```bash
docker-compose down -v
```

## MySQL UDF HTTP 插件说明

本配置为基础 MySQL 5.7 环境，如需添加 mysql-udf-http 插件：

1. 创建自定义 Dockerfile
2. 在容器启动时加载插件
3. 或者通过 SQL 命令动态加载（需要编译好的 .so/.dll 文件）

## 故障排除

- **连接问题**: 确保 MySQL 容器已完全启动后再尝试连接
- **权限问题**: 检查用户权限和防火墙设置
- **数据持久化**: 所有数据都保存在 `mysql-data` 卷中，重启不会丢失