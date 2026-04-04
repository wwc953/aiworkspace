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

-- 测试
select CAST(http_get('http://www.baidu.com') AS CHAR CHARACTER SET utf8) as result;

/**
    多文件上传(字节流)
*/
SELECT 
  http_post_multipart_multi (
    'http://172.29.224.1:9000/file/uploadList2',-- url
    'auto_token: Bearer your-token\nvi: vvvvvvvvvvvvvv',-- 请求头
    '10000',-- 超时时间ms
    'custID=custID;uuid=uuid',-- 字段
    'file', -- 文件属性
    '1.txt',-- 文件名称
    CAST('1111' AS BINARY),-- 文件字节
    'file',
    '2.txt',
    CAST('你好阿斯顿撒旦阿萨德阿斯顿撒的阿萨德 萨达 你好阿斯顿撒旦 ' AS BINARY)
  ) ;