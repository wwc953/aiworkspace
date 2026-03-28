# MySQL UDF 插件放置位置指南

## 当前配置分析

当前 `docker-compose.yml` 配置如下：

```yaml
volumes:
  - ./plugins:/usr/lib/mysql/plugin
```

这意味着：
- **宿主机目录**: `./plugins/` (相对路径，实际是 `/c/workspace/mysqludf/plugins`)
- **容器内目录**: `/usr/lib/mysql/plugin` (MySQL 5.7 的标准插件目录)

## 推荐的插件放置方案

### 方案 1: 使用标准目录结构 (推荐)

```
mysqludf/
├── docker-compose.yml
├── plugins/
│   └── http_udf.so          # 插件文件
├── mysql-data/              # 数据卷
├── init-scripts/
│   └── setup-udf.sql
└── README.md
```

**优点**:
- 目录结构清晰
- 易于管理多个插件
- 符合 Docker Compose 最佳实践

### 方案 2: 使用项目根目录

```
mysqludf/
├── docker-compose.yml
├── mysql-data/
├── init-scripts/
├── plugins/
└── mysql-udf-http/
    └── src/
        └── ...              # 插件源码
```

### 方案 3: 使用绝对路径 (生产环境推荐)

```yaml
volumes:
  - /absolute/path/to/plugins:/usr/lib/mysql/plugin
```

## 插件加载验证

无论选择哪种方案，都需要确保：

1. **文件权限正确**
   ```bash
   chmod 644 plugins/http_udf.so
   ```

2. **MySQL 启动时加载插件**
   ```yaml
   command:
     - --plugin-load-add=http_udf.so
   ```

3. **验证加载成功**
   ```sql
   SHOW PLUGINS;
   SELECT * FROM mysql.plugin WHERE name = 'http_udf';
   ```

## 故障排除

### 常见插件无法加载问题

1. **权限问题**
   ```bash
   # 检查文件权限
   ls -la plugins/http_udf.so

   # 修复权限
   chmod 644 plugins/http_udf.so
   ```

2. **架构不匹配**
   ```bash
   # 检查系统架构
   uname -m

   # 确保插件架构匹配
   file plugins/http_udf.so
   ```

3. **MySQL 版本兼容性**
   ```sql
   -- 检查 MySQL 版本
   SELECT VERSION();

   -- 查看支持的插件
   SHOW PLUGINS;
   ```

### 日志检查

```bash
# 查看 MySQL 容器日志
docker logs mysql-57-udf

# 查看详细的错误信息
docker exec -it mysql-57-udf tail -f /var/log/mysql/error.log
```

## 最佳实践建议

1. **命名规范**
   ```
   plugins/
   ├── http_udf.so
   ├── my_custom_func.so
   └── utility_plugin.so
   ```

2. **版本控制**
   - 将插件文件添加到 `.gitignore` 中
   - 或者创建专门的 `binaries/` 目录用于存储编译产物

3. **自动化脚本**
   ```bash
   # 自动部署脚本示例
   #!/bin/bash
   cp compiled/http_udf.so plugins/
   chmod 644 plugins/http_udf.so
   docker-compose restart mysql
   ```

4. **环境隔离**
   - 开发环境: 使用本地编译的插件
   - 测试环境: 使用预编译的稳定版本
   - 生产环境: 使用经过验证的特定版本

## 推荐的最终结构

基于以上分析，我推荐使用以下结构：

```
mysqludf/
├── docker-compose.yml           # 包含插件目录挂载
├── plugins/                     # 存放所有插件文件
│   ├── http_udf.so             # UDF HTTP 插件
│   └── ...                     # 其他插件
├── mysql-data/                  # 数据持久化
├── init-scripts/                # 初始化脚本
├── README.md                    # 使用说明
├── INSTALL.md                   # 安装指南
└── 64bit-note.md                # 64位说明
```

这样配置既保持了良好的组织结构，又确保了插件能够正确加载和使用。