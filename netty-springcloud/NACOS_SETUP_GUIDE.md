# Nacos 快速部署指南

## 🚀 一键部署脚本

### 1. 下载并启动Nacos Server

#### Linux/Mac
```bash
# 创建安装目录
mkdir -p ~/nacos && cd ~/nacos

# 下载Nacos (版本2.2.3)
wget https://github.com/alibaba/nacos/releases/download/v2.2.3/nacos-server-2.2.3.zip

# 解压
unzip nacos-server-2.2.3.zip
cd nacos/bin

# 单机模式启动
sh startup.sh -m standalone

echo "Nacos服务器已启动，访问 http://localhost:8848/nacos/"
echo "默认用户名密码: nacos/nacos"
```

#### Windows
```cmd
REM 下载Nacos
curl -L -o nacos-server-2.2.3.zip https://github.com/alibaba/nacos/releases/download/v2.2.3/nacos-server-2.2.3.zip

REM 解压
tar -xzf nacos-server-2.2.3.zip

REM 进入目录
cd nacos\bin

REM 启动服务
startup.cmd

echo Nacos服务器已启动，访问 http://localhost:8848/nacos/
```

### 2. 编译和启动微服务

```bash
# 进入项目根目录
cd /workspace/netty-springcloud

# 编译项目
mvn clean compile

# 启动各个服务（按顺序）

# 1. 启动Nacos Discovery Server
cd springcloud-nacos-discovery
mvn spring-boot:run &

# 等待2秒让服务初始化
sleep 2

# 2. 启动Nacos Config Server
cd ../springcloud-nacos-config
mvn spring-boot:run &

# 3. 启动Netty Service
cd ../netty-service
mvn spring-boot:run &

# 4. 启动API Gateway
cd ../springcloud-gateway
mvn spring-boot:run &

echo "所有服务已启动！"
echo "API Gateway: http://localhost:9000"
echo "Nacos控制台: http://localhost:8848/nacos/"
```

### 3. 验证服务状态

```bash
# 检查所有服务的健康状态
curl http://localhost:8082/health

# 查看网关路由
curl http://localhost:9000/actuator/gateway/routes

# 获取配置信息
curl http://localhost:9000/actuator/nacos-discovery
```

## 🔧 配置管理

### 创建配置文件

通过Nacos控制台或API创建配置文件：

```bash
# 创建netty-service配置
curl -X POST "http://localhost:8848/nacos/v1/cs/configs" \
  -d "dataId=netty-service-dev.yaml" \
  -d "group=DEFAULT_GROUP" \
  -d "content=server:\n  port: 8082\nnetty:\n  server:\n    port: 8083"

# 创建common配置
curl -X POST "http://localhost:8848/nacos/v1/cs/configs" \
  -d "dataId=common-dev.yaml" \
  -d "group=DEFAULT_GROUP" \
  -d "content=app:\n  name: netty-springcloud\n  version: 1.0.0"
```

## 📊 监控面板

### Nacos控制台
- **URL**: http://localhost:8848/nacos/
- **用户**: nacos
- **密码**: nacos

### 主要页面
1. **服务列表**: Services > Service Management
2. **配置管理**: Configurations > Data ID Management
3. **集群状态**: 实时显示节点状态
4. **监控指标**: 性能和使用情况统计

### Spring Boot Actuator
```bash
# 查看应用健康状态
curl http://localhost:9000/actuator/health

# 查看环境变量
curl http://localhost:9000/actuator/env

# 查看Beans
curl http://localhost:9000/actuator/beans
```

## 🧪 功能测试

### 1. 服务发现测试
```bash
# 测试网关路由
curl -v http://localhost:9000/netty/health

# 预期响应
{"status":"UP","service":"netty-service"}
```

### 2. 配置热更新测试
```bash
# 修改Nacos中的配置文件
# 观察应用日志中的配置刷新信息
tail -f target/logs/*.log | grep "Refresh"

# 测试配置变化是否生效
curl http://localhost:9000/netty/api/message
```

### 3. WebSocket连接测试
```javascript
// 浏览器控制台或Postman
const ws = new WebSocket('ws://localhost:8082/ws');
ws.onopen = () => {
  console.log('WebSocket连接成功');
  ws.send('Hello Netty!');
};
```

## 🛠️ 开发调试

### 开启详细日志
```bash
# 在application.yml中设置
logging:
  level:
    com.alibaba.nacos: DEBUG
    com.alibaba.cloud.nacos: DEBUG
    org.springframework.cloud.gateway: DEBUG
```

### 常见问题排查

#### 服务注册失败
```bash
# 检查网络连接
ping localhost

# 检查端口占用
netstat -an | grep 8848

# 查看应用日志
tail -f target/logs/application.log
```

#### 配置不生效
```bash
# 检查配置文件格式
# 确保使用正确的yaml/json格式

# 检查DataID命名
# 格式: ${prefix}-${spring.profiles.active}.${file-extension}
```

## 📈 性能调优

### JVM参数优化
```bash
export JAVA_OPTS="-Xms512m -Xmx1024m -XX:+UseG1GC"
mvn spring-boot:run
```

### Nacos参数优化
```yaml
# application.yml
spring:
  cloud:
    nacos:
      discovery:
        heartbeat:
          time-to-live-seconds: 10
        client:
          beat-thread-count: 1
      config:
        bootstrap:
          enable: true
          log-enable: false
```

## 🚢 生产部署

### Docker Compose
```yaml
# docker-compose.yml
version: '3'
services:
  nacos:
    image: nacos/nacos-server:latest
    environment:
      - MODE=standalone
    ports:
      - "8848:8848"
    volumes:
      - ./nacos/logs:/home/nacos/logs

  netty-service:
    build: .
    depends_on:
      - nacos
    environment:
      - SPRING_PROFILES_ACTIVE=prod
    ports:
      - "8082:8082"
```

### Kubernetes部署
```yaml
# k8s-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: netty-service
spec:
  replicas: 3
  selector:
    matchLabels:
      app: netty-service
  template:
    metadata:
      labels:
        app: netty-service
    spec:
      containers:
      - name: netty-service
        image: your-registry/netty-service:latest
        env:
        - name: SPRING_CLOUD_NACOS_DISCOVERY_SERVER_ADDR
          value: "nacos:8848"
```

## 📞 技术支持

### 官方文档
- [Nacos中文文档](https://nacos.io/zh-cn/docs/what-is-nacos.html)
- [Spring Cloud Alibaba](https://github.com/alibaba/spring-cloud-alibaba)

### 问题反馈
- GitHub Issues: https://github.com/alibaba/nacos/issues
- 阿里云社区: https://developer.aliyun.com/article

---

🎉 **恭喜！您的Netty-SpringCloud项目现在已经完整集成了Nacos！**

您现在拥有一个高性能、现代化的微服务架构，具备：
- 统一的服务发现和注册
- 动态配置管理
- 生产级部署能力
- 完整的监控和运维支持