# Netty-SpringCloud Docker部署指南

## 🎯 概述

本指南详细介绍了如何使用Docker容器化部署Netty-SpringCloud微服务架构。通过Docker Compose，您可以一键启动完整的生产环境部署。

## 📋 系统要求

### 硬件要求
- **CPU**: 4核以上
- **内存**: 8GB以上（推荐16GB）
- **存储**: 20GB可用空间
- **网络**: 稳定互联网连接

### 软件要求
- **Docker Engine**: 20.10+
- **Docker Compose**: v2.0+
- **操作系统**: Linux/macOS/Windows (WSL2)

## 🚀 快速开始

### 1. 克隆项目并构建镜像

```bash
# 克隆项目
git clone <your-repo-url>
cd netty-springcloud

# 编译所有模块
mvn clean package

# 构建Docker镜像
docker-compose build
```

### 2. 启动完整部署

```bash
# 后台启动所有服务
docker-compose up -d

# 查看启动状态
docker-compose ps

# 查看实时日志
docker-compose logs -f
```

### 3. 验证部署

```bash
# 检查服务健康状态
curl http://localhost:9000/actuator/health

# 查看Nacos控制台
open http://localhost:8848/nacos/

# 查看Grafana监控面板
open http://localhost:3000

# 查看Prometheus
open http://localhost:9090
```

## 📊 服务端口映射

| 服务 | 容器端口 | 主机端口 | 说明 |
|------|---------|---------|------|
| Nacos Server | 8848 | 8848 | 主服务端口 |
| Nacos Discovery | 8848 | 8848 | 服务发现 |
| Nacos Config | 8848 | 8848 | 配置管理 |
| Netty Service | 8082 | 8082 | TCP服务 |
| API Gateway | 9000 | 9000 | HTTP网关 |
| Redis | 6379 | 6379 | 缓存服务 |
| MySQL | 3306 | 3306 | 数据持久化 |
| Prometheus | 9090 | 9090 | 指标收集 |
| Grafana | 3000 | 3000 | 可视化监控 |

## 🔧 配置文件详解

### docker-compose.yml 关键配置

#### 服务依赖关系
```yaml
depends_on:
  nacos:
    condition: service_healthy
  netty-service:
    condition: service_started
```

#### 健康检查
```yaml
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:8848/nacos/"]
  interval: 30s
  timeout: 10s
  retries: 3
```

#### JVM调优参数
```yaml
environment:
  JAVA_OPTS: "-Xms512m -Xmx1024m -XX:+UseG1GC"
```

## 🗄️ 数据存储

### 日志目录结构
```
netty-springcloud/
├── logs/
│   ├── nacos-discovery/
│   ├── nacos-config/
│   ├── netty-service/
│   └── api-gateway/
├── mysql/data/        # MySQL数据文件
├── redis/data/        # Redis数据文件
├── nacos/logs/        # Nacos日志
└── nacos/data/        # Nacos数据
```

### 数据持久化配置
```yaml
volumes:
  - ./nacos/logs:/home/nacos/logs
  - ./nacos/data:/home/nacos/data
  - ./mysql/data:/var/lib/mysql
  - ./redis/data:/data
```

## 🛠️ 开发调试

### 进入容器
```bash
# 进入Netty服务容器
docker-compose exec netty-service bash

# 进入API网关容器
docker-compose exec api-gateway sh

# 查看容器日志
docker-compose logs netty-service
```

### 修改配置
```bash
# 重新构建特定服务
docker-compose build netty-service

# 重启服务
docker-compose restart netty-service

# 重新创建服务
docker-compose up -d --build netty-service
```

## 📈 性能监控

### Prometheus指标采集

#### 关键监控指标
- **JVM内存使用** (heap, non-heap)
- **线程池状态** (active threads, queued tasks)
- **HTTP请求统计** (rate, duration, error rate)
- **数据库连接池** (active connections, wait time)
- **消息队列** (pending messages, throughput)

#### 自定义指标
```java
@RestController
public class MetricsController {
    @GetMapping("/metrics/custom")
    public Map<String, Object> getCustomMetrics() {
        // 添加业务相关指标
        return Map.of(
            "message_count", messageService.getMessageCount(),
            "active_connections", channelHandler.getActiveConnections()
        );
    }
}
```

### Grafana仪表板

#### 预置仪表板
1. **Spring Boot Applications**
   - JVM Memory Usage
   - HTTP Request Rate
   - Database Connection Pool

2. **Nacos Cluster Health**
   - Service Registration Status
   - Configuration Sync Status
   - Node Health Metrics

3. **Netty Performance**
   - Active Connections
   - Message Throughput
   - Channel Read/Write Statistics

## 🔐 安全配置

### Nacos认证
```bash
# 默认用户名密码
Username: nacos
Password: nacos

# 通过环境变量修改
environment:
  NACOS_AUTH_ENABLE: "true"
  NACOS_AUTH_TOKEN: "自定义密钥"
```

### 网络安全
```yaml
networks:
  netty-network:
    driver: bridge
    name: netty-springcloud-network
    # 可选：IP范围限制
    ipam:
      config:
        - subnet: 172.20.0.0/16
```

### 数据加密
```yaml
# 在application.yml中
spring:
  cloud:
    nacos:
      discovery:
        ssl-enabled: true
        server-addr: https://nacos:8849
```

## 🚨 故障排除

### 常见问题

#### Q: 服务无法启动
A: 检查容器日志和依赖服务状态
```bash
docker-compose logs service-name
docker-compose ps
```

#### Q: Nacos注册失败
A: 确认网络连通性和认证配置
```bash
# 测试网络连通性
docker network inspect netty-network

# 检查认证配置
docker exec -it nacos-discovery cat application.yml
```

#### Q: 配置不生效
A: 确认DataID命名规范和分组设置
```bash
# 查看配置详情
curl "http://localhost:8848/nacos/v1/cs/configs?dataId=netty-service&group=DEFAULT_GROUP"

# 检查应用日志中的配置加载信息
tail -f logs/netty-service/*.log | grep "Refresh"
```

### 性能问题诊断

#### JVM调优
```bash
# 查看JVM参数
docker stats netty-service

# 调整内存配置
JAVA_OPTS="-Xms1g -Xmx2g -XX:+UseZGC"
```

#### 网络延迟
```bash
# 测试容器间网络
docker exec netty-service ping nacos-discovery

# 检查DNS解析
docker exec netty-service nslookup nacos-discovery
```

## 📦 生产环境部署

### 生产环境优化

#### 资源限制
```yaml
deploy:
  resources:
    limits:
      cpus: '2.0'
      memory: 2G
    reservations:
      cpus: '1.0'
      memory: 1G
```

#### 高可用性
```yaml
version: '3.8'
services:
  netty-service:
    deploy:
      replicas: 3
      update_config:
        parallelism: 1
        delay: 10s
      restart_policy:
        condition: on-failure
```

### 蓝绿部署
```bash
# 创建新版本
docker-compose -f docker-compose-v2.yml up -d

# 流量切换
docker-compose stop old-version

# 清理旧版本
docker-compose down --rmi all
```

## 🧹 维护操作

### 清理操作
```bash
# 停止并移除所有服务
docker-compose down

# 删除所有未使用的资源
docker system prune -a

# 清理特定服务
docker-compose rm -f netty-service
```

### 备份恢复
```bash
# 备份MySQL数据
docker exec netty-springcloud-mysql mysqldump -u root -p nacos_devtest > backup.sql

# 恢复MySQL数据
docker exec -i netty-springcloud-mysql mysql -u root -p nacos_devtest < backup.sql
```

## 📚 参考文档

- [Docker官方文档](https://docs.docker.com/)
- [Docker Compose Reference](https://docs.docker.com/compose/reference/)
- [Spring Boot Actuator](https://docs.spring.io/spring-boot/docs/current/reference/html/actuator.html)
- [Prometheus Monitoring](https://prometheus.io/docs/introduction/overview/)

---

🎉 **恭喜！您的Netty-SpringCloud项目现在已经完全容器化了！**

您现在拥有一个完整的、生产就绪的Docker部署方案，包括：
- **一键部署** 所有微服务
- **完整的监控体系** (Prometheus + Grafana)
- **数据持久化** 和备份机制
- **高可用架构** 支持水平扩展
- **安全加固** 和性能优化

立即开始您的容器化之旅吧！