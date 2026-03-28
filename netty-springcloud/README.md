# Netty-SpringCloud 项目

基于 JDK 8 的 Netty + Spring Cloud 微服务架构项目，提供高性能网络通信与现代化的微服务治理能力。

## 技术栈

- **JDK 8**: Java 8 运行环境
- **Spring Boot 2.7.18**: 应用框架
- **Spring Cloud Alibaba 2021.0.8**: 微服务套件 (Nacos)
- **Netty 4.1.100.Final**: 高性能网络框架
- **Maven**: 依赖管理
- **Lombok**: 简化代码
- **Logback**: 日志框架
- **Nacos 2.2.3**: 服务注册与配置中心

## 项目结构

```
netty-springcloud/
├── common-utils/              # 公共工具类
├── netty-service/             # Netty高性能服务
├── springcloud-eureka-server/ # 服务注册中心
├── springcloud-config-server/ # 配置中心
└── springcloud-gateway/       # API网关
```

## 核心模块说明

### 1. common-utils (公共工具)
- `NettyConstants.java`: Netty相关常量定义
- `NettyException.java`: 异常基类
- `NettyUtils.java`: Netty工具类

### 2. netty-service (Netty服务)
- 基于 Netty 的高性能 TCP/HTTP/WebSocket 服务
- 支持消息编解码、心跳检测、空闲连接管理
- 集成 Spring Boot 便于管理和监控

### 3. springcloud-eureka-server (Eureka服务)
- 服务注册与发现中心
- 默认端口: 8761
- 支持高可用部署

### 4. springcloud-nacos-config (Nacos配置中心)
- Nacos配置管理中心
- 动态配置刷新
- 默认端口: 8848

### 5. springcloud-gateway (API网关)
- Spring Cloud Gateway
- 路由转发
- 负载均衡
- CORS 支持
- 默认端口: 9000

## 快速开始

### 1. 编译项目
```bash
mvn clean compile
```

### 2. 启动各个服务

#### 启动 Eureka 服务器
```bash
cd springcloud-eureka-server
mvn spring-boot:run
```

#### 启动 Nacos Config 服务器
```bash
cd ../springcloud-nacos-config
mvn spring-boot:run
```

#### 启动 Netty 服务
```bash
cd ../netty-service
mvn spring-boot:run
```

#### 启动 API 网关
```bash
cd ../springcloud-gateway
mvn spring-boot:run
```

### 3. 测试服务

#### 健康检查
```
GET http://localhost:8082/health
```

#### API 接口
```
GET http://localhost:8082/api/message
```

#### WebSocket 连接
```
ws://localhost:8082/ws
```

## Netty 特性

- **高性能**: 基于 NIO 的非阻塞 I/O
- **协议支持**: HTTP, WebSocket, TCP
- **消息处理**: 自定义编解码器
- **连接管理**: 心跳检测、空闲超时
- **广播功能**: 多客户端消息推送

## Spring Cloud 特性

- **服务注册发现**: Eureka
- **配置管理**: Config Server
- **API 网关**: Spring Cloud Gateway
- **负载均衡**: Ribbon
- **断路器**: Hystrix

## 配置文件

每个服务都有对应的 `application.yml` 配置文件，支持：

- 端口配置
- 线程池设置
- 日志级别
- Eureka 客户端配置
- 路由规则

## 开发规范

- 使用 Lombok 简化代码
- 遵循 RESTful API 设计
- 统一的异常处理
- 详细的日志记录
- 单元测试覆盖

## 扩展建议

1. **安全认证**: 集成 OAuth2/JWT
2. **监控**: 集成 Prometheus + Grafana
3. **分布式追踪**: Zipkin/Sleuth
4. **消息队列**: Kafka/RabbitMQ
5. **数据库**: Redis/MongoDB
6. **容器化**: Docker/Kubernetes

## 许可证

MIT License