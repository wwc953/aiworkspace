# MySQL UDF HTTP 插件

此目录用于存放 MySQL UDF HTTP 插件文件。

## 插件文件要求

- 将编译好的 `http_udf.so` (Linux) 或 `http_udf.dll` (Windows) 文件放入此目录
- 确保文件具有正确的权限（644）

## 加载插件

MySQL 容器启动时会自动加载此目录中的插件，您也可以在 MySQL 中手动加载：

```sql
-- 手动加载插件
INSTALL PLUGIN http_udf SONAME 'http_udf.so';

-- 检查插件状态
SELECT * FROM mysql.plugin WHERE name = 'http_udf';
```

## 插件功能

mysql-udf-http 插件提供以下功能：
- HTTP 请求函数（http_get, http_post等）
- REST API 调用
- Web 服务集成
- JSON 数据处理

## 参考文档

- [MySQL UDF HTTP 插件官方文档](https://github.com/maechler/mysqludf_http)
- [UDF 开发指南](https://dev.mysql.com/doc/refman/5.7/en/adding-user-defined-functions.html)