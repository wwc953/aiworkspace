package com.netty.springcloud.common.exception;

/**
 * Netty相关异常基类
 */
public class NettyException extends RuntimeException {

    private static final long serialVersionUID = 1L;

    public NettyException(String message) {
        super(message);
    }

    public NettyException(String message, Throwable cause) {
        super(message, cause);
    }

    public NettyException(Throwable cause) {
        super(cause);
    }
}