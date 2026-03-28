# Nacos 集成指南

## 🎯 概述

本项目已将 Spring Cloud Netflix (Eureka/Config) 替换为阿里巴巴的 **Nacos**，提供了统一的服务发现、配置管理和服务治理功能。

## 📋 模块变更

### 1. springcloud-eureka-server → springcloud-nacos-discovery
- **功能**: Nacos服务注册与发现中心
- **端口**: 8848（默认Nacos端口）
- **依赖**: `spring-cloud-starter-alibaba-nacos-discovery`

### 2. springcloud-config-server → springcloud-nacos-config
- **功能**: Nacos配置管理中心
- **端口**: 8848（默认Nacos端口）
- **依赖**: `spring-cloud-starter-alibaba-nacos-config`

### 3. netty-service
- **新增**: Nacos客户端依赖
- **功能**: 自动注册到Nacos并获取配置

### 4. springcloud-gateway
- **更新**: Eureka客户端替换为Nacos客户端
- **功能**: 通过Nacos进行服务发现和负载均衡

## 🚀 启动顺序

### 1. 启动Nacos Server
```bash
# 下载并启动Nacos
wget https://github.com/alibaba/nacos/releases/download/v2.2.3/nacos-server-2.2.3.zip
unzip nacos-server-2.2.3.zip
cd nacos/bin
sh startup.sh -m standalone  # Linux/Mac
startup.cmd                  # Windows
```

### 2. 启动各个微服务

#### 启动Nacos Discovery Server
```bash
cd springcloud-nacos-discovery
mvn spring-boot:run
```

#### 启动Nacos Config Server
```bash
cd ../springcloud-nacos-config
mvn spring-boot:run
```

#### 启动Netty Service
```bash
cd ../netty-service
mvn spring-boot:run
```

#### 启动API Gateway
```bash
cd ../springcloud-gateway
mvn spring-boot:run
```

## ⚙️ 配置说明

### Nacos基础配置
```yaml
spring:
  cloud:
    nacos:
      discovery:
        server-addr: localhost:8848  # Nacos服务器地址
        namespace: public            # 命名空间ID
        group: DEFAULT_GROUP         # 分组
        metadata:
          version: 1.0.0
          env: dev
      config:
        server-addr: localhost:8848
        namespace: public
        group: DEFAULT_GROUP
        file-extension: yaml         # 配置文件格式
```

### 配置文件命名规则
```
${prefix}-${spring.profiles.active}.${file-extension}

示例:
- netty-service-dev.yaml
- common-dev.yaml
- api-gateway-dev.yaml
```

## 🔍 Nacos控制台操作

### 1. 服务管理
- **路径**: http://localhost:8848/nacos/
- **查看服务**: Services > Service Management
- **服务列表**: 显示所有注册的微服务

### 2. 配置管理
- **路径**: http://localhost:8848/nacos/
- **创建配置**: Configurations > Data ID Management
- **配置内容**: 支持JSON、YAML、Properties等格式

### 3. 监控面板
- **服务健康**: 实时显示服务状态
- **配置版本**: 配置变更历史
- **访问控制**: IP白名单和权限管理

## 📡 API接口

### 服务发现
```bash
# 查询所有服务
GET http://localhost:9000/actuator/nacos-discovery

# 查看路由信息
GET http://localhost:9000/actuator/gateway/routes
```

### 配置获取
```bash
# 获取配置
GET http://localhost:8848/nacos/v1/cs/configs?dataId=netty-service&group=DEFAULT_GROUP

# 订阅配置变更
POST http://localhost:8848/nacos/v1/cs/configs/listener
```

## 🛠️ 开发指南

### 添加新的微服务

1. **添加Nacos依赖**
```xml
<dependency>
    <groupId>com.alibaba.cloud</groupId>
    <artifactId>spring-cloud-starter-alibaba-nacos-discovery</artifactId>
</dependency>
<dependency>
    <groupId>com.alibaba.cloud</groupId>
    <artifactId>spring-cloud-starter-alibaba-nacos-config</artifactId>
</dependency>
```

2. **配置application.yml**
```yaml
spring:
  application:
    name: your-service-name
  cloud:
    nacos:
      discovery:
        server-addr: localhost:8848
      config:
        server-addr: localhost:8848
```

3. **启动类注解**
```java
@SpringBootApplication
public class YourServiceApplication {
    public static void main(String[] args) {
        SpringApplication.run(YourServiceApplication.class, args);
    }
}
```

### 动态配置刷新

```java
@RefreshScope
@RestController
public class ConfigController {

    @Value("${custom.config.value}")
    private String customConfig;

    @GetMapping("/config")
    public String getConfig() {
        return customConfig;
    }
}
```

## 🔧 高级特性

### 权重路由
```yaml
spring:
  cloud:
    gateway:
      routes:
        - id: weighted-route
          uri: lb://your-service
          predicates:
            - Path=/service/**
          filters:
            - StripPrefix=1
            - Weight=service, 80, service-vip, 20
```

### 灰度发布
```yaml
nacos:
  discovery:
    metadata:
      version: 1.0.0
      weight: 80
      tags: gray
```

### 熔断降级
```yaml
spring:
  cloud:
    sentinel:
      enabled: true
      transport:
        dashboard: localhost:8080
```

## 🐛 常见问题

### Q: 服务无法注册到Nacos
A: 检查网络连接和Nacos服务器状态，确认server-addr配置正确。

### Q: 配置未生效
A: 确保配置文件格式正确，检查dataId和group是否匹配。

### Q: 网关路由失败
A: 确认服务已注册，检查lb://协议是否支持。

## 📈 性能优化

### 连接池配置
```yaml
spring:
  cloud:
    nacos:
      discovery:
        heartbeat:
          time-to-live-seconds: 10
        client:
          beat-thread-count: 1
```

### 缓存配置
```yaml
nacos:
  config:
    cache-dir: /tmp/nacos/cache
    bootstrap:
      enable: true
      log-enable: false
```

## 🔐 安全配置

### 用户名密码认证
```yaml
spring:
  cloud:
    nacos:
      discovery:
        username: nacos
        password: nacos
```

### SSL加密
```yaml
spring:
  cloud:
    nacos:
      discovery:
        ssl-enabled: true
        server-addr: https://localhost:8849
```

## 📊 监控指标

### Prometheus集成
```yaml
management:
  endpoints:
    web:
      exposure:
        include: prometheus
  metrics:
    export:
      prometheus:
        enabled: true
```

### Grafana仪表板
- **Nacos服务状态**: 实时监控服务健康
- **配置变更**: 跟踪配置修改历史
- **性能指标**: 响应时间和成功率统计

## 🚢 部署建议

### Docker部署
```dockerfile
FROM openjdk:8-jre-slim
COPY target/springcloud-nacos-discovery.jar app.jar
ENTRYPOINT ["java", "-jar", "/app.jar"]
```

### Kubernetes部署
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: nacos-discovery
spec:
  replicas: 2
  selector:
    matchLabels:
      app: nacos-discovery
  template:
    metadata:
      labels:
        app: nacos-discovery
    spec:
      containers:
      - name: nacos-discovery
        image: your-registry/nacos-discovery:latest
        ports:
        - containerPort: 8848
```

## 📚 参考文档

- [Nacos官方文档](https://nacos.io/zh-cn/docs/what-is-nacos.html)
- [Spring Cloud Alibaba](https://github.com/alibaba/spring-cloud-alibaba)
- [Nacos GitHub](https://github.com/alibaba/nacos)
- [Spring Cloud Gateway](https://docs.spring.io/spring-cloud-gateway/docs/current/reference/html/)