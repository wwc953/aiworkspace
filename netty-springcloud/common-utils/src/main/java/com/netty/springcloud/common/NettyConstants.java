package com.netty.springcloud.common;

/**
 * Netty相关常量定义
 */
public class NettyConstants {

    // 默认端口
    public static final int DEFAULT_PORT = 8080;
    public static final int DEFAULT_SSL_PORT = 8443;
    public static final int EUREKA_SERVER_PORT = 8761;
    public static final int CONFIG_SERVER_PORT = 8888;
    public static final int GATEWAY_PORT = 9000;

    // Netty配置
    public static final int DEFAULT_BOSS_THREADS = 1;
    public static final int DEFAULT_WORKER_THREADS = Runtime.getRuntime().availableProcessors() * 2;
    public static final int DEFAULT_SO_BACKLOG = 1024;
    public static final int DEFAULT_SO_TIMEOUT = 30000;
    public static final int DEFAULT_MAX_FRAME_LENGTH = 65536;
    public static final int DEFAULT_HEARTBEAT_INTERVAL = 30;

    // 消息类型
    public static final String MESSAGE_TYPE_HEARTBEAT = "HEARTBEAT";
    public static final String MESSAGE_TYPE_REQUEST = "REQUEST";
    public static final String MESSAGE_TYPE_RESPONSE = "RESPONSE";
    public static final String MESSAGE_TYPE_PUSH = "PUSH";

    // 协议类型
    public static final String PROTOCOL_HTTP = "HTTP";
    public static final String PROTOCOL_WEBSOCKET = "WEBSOCKET";
    public static final String PROTOCOL_TCP = "TCP";
    public static final String PROTOCOL_UDP = "UDP";

    private NettyConstants() {
        throw new AssertionError("Cannot instantiate utility class");
    }
}