package com.netty.springcloud.common.util;

import java.net.InetSocketAddress;

/**
 * Netty工具类 - 仅包含非Netty依赖的工具方法
 */
public class NettyUtils {

    /**
     * 从SocketAddress获取IP地址
     */
    public static String getClientIp(InetSocketAddress socketAddress) {
        if (socketAddress == null) {
            return "unknown";
        }

        return socketAddress.getAddress().getHostAddress();
    }

    /**
     * 安全的字符串转换，避免null指针异常
     */
    public static String safeString(Object obj) {
        return obj != null ? obj.toString() : "";
    }

    private NettyUtils() {
        throw new AssertionError("Cannot instantiate utility class");
    }
}