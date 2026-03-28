package com.netty.springcloud.netty.handler;

import com.netty.springcloud.common.NettyConstants;
import io.netty.channel.ChannelHandlerContext;
import io.netty.channel.SimpleChannelInboundHandler;
import io.netty.handler.codec.http.*;
import io.netty.handler.codec.http.websocketx.TextWebSocketFrame;
import io.netty.handler.timeout.IdleStateEvent;
import io.netty.util.CharsetUtil;
import io.netty.util.ReferenceCountUtil;

/**
 * Netty服务器消息处理器
 */
public class NettyServerHandler extends SimpleChannelInboundHandler<Object> {

    @Override
    protected void channelRead0(ChannelHandlerContext ctx, Object msg) throws Exception {
        if (msg instanceof FullHttpRequest) {
            handleHttpRequest(ctx, (FullHttpRequest) msg);
        } else if (msg instanceof TextWebSocketFrame) {
            handleWebSocketMessage(ctx, (TextWebSocketFrame) msg);
        } else if (msg instanceof String) {
            handleStringMessage(ctx, (String) msg);
        } else {
            System.err.println("收到不支持的消息类型: " + msg.getClass().getName());
            sendErrorMessage(ctx, "不支持的消息类型");
        }
    }

    /**
     * 处理HTTP请求
     */
    private void handleHttpRequest(ChannelHandlerContext ctx, FullHttpRequest request) {
        try {
            // 检查是否为WebSocket升级请求
            if (isWebSocketUpgrade(request)) {
                // WebSocket升级逻辑将在WebSocketHandler中处理
                return;
            }

            String clientIp = getClientIp(ctx);
            String uri = request.uri();

            System.out.println("收到HTTP请求: " + clientIp + " -> " + uri);

            // 简单的路由处理
            if ("/health".equals(uri)) {
                sendHealthResponse(ctx);
            } else if ("/api/message".equals(uri)) {
                sendApiResponse(ctx, request);
            } else {
                sendNotFoundResponse(ctx);
            }

            // 释放HTTP请求对象
            ReferenceCountUtil.release(request);

        } catch (Exception e) {
            System.err.println("处理HTTP请求时发生异常: " + e.getMessage());
            sendErrorMessage(ctx, "内部服务器错误");
        }
    }

    /**
     * 处理WebSocket消息
     */
    private void handleWebSocketMessage(ChannelHandlerContext ctx, TextWebSocketFrame frame) {
        String message = frame.text();
        String clientIp = getClientIp(ctx);

        System.out.println("收到WebSocket消息: " + clientIp + " -> " + message);

        // 广播消息给所有连接的客户端（示例）
        broadcastMessage("[广播] " + message);
    }

    /**
     * 处理字符串消息
     */
    private void handleStringMessage(ChannelHandlerContext ctx, String message) {
        String clientIp = getClientIp(ctx);
        System.out.println("收到字符串消息: " + clientIp + " -> " + message);

        // 处理心跳消息
        if (message.contains(NettyConstants.MESSAGE_TYPE_HEARTBEAT)) {
            sendHeartbeatResponse(ctx);
        } else {
            // 默认响应
            sendDefaultResponse(ctx, message);
        }
    }

    /**
     * 发送健康检查响应
     */
    private void sendHealthResponse(ChannelHandlerContext ctx) {
        String response = "{\"status\":\"UP\",\"service\":\"netty-service\"}";
        sendHttpResponse(ctx.channel(), null, HttpResponseStatus.OK, response);
    }

    /**
     * 发送API响应
     */
    private void sendApiResponse(ChannelHandlerContext ctx, FullHttpRequest request) {
        String response = "{\"message\":\"Hello from Netty Service\",\"timestamp\":\"" +
                         System.currentTimeMillis() + "\"}";
        sendHttpResponse(ctx.channel(), request, HttpResponseStatus.OK, response);
    }

    /**
     * 发送404响应
     */
    private void sendNotFoundResponse(ChannelHandlerContext ctx) {
        sendHttpResponse(ctx.channel(), null, HttpResponseStatus.NOT_FOUND,
                        "{\"error\":\"Not Found\"}");
    }

    /**
     * 发送错误响应
     */
    private void sendErrorMessage(ChannelHandlerContext ctx, String error) {
        sendHttpResponse(ctx.channel(), null, HttpResponseStatus.INTERNAL_SERVER_ERROR, error);
    }

    /**
     * 发送心跳响应
     */
    private void sendHeartbeatResponse(ChannelHandlerContext ctx) {
        String response = "[{\"type\":\"HEARTBEAT_RESPONSE\",\"time\":\"" +
                         System.currentTimeMillis() + "\"}]";
        sendTextMessage(ctx.channel(), response);
    }

    /**
     * 发送默认响应
     */
    private void sendDefaultResponse(ChannelHandlerContext ctx, String message) {
        String response = "[{\"received\":\"" + message + "\",\"processed\":\"success\"}]";
        sendTextMessage(ctx.channel(), response);
    }

    /**
     * 广播消息给所有客户端
     */
    private void broadcastMessage(String message) {
        // 这里可以实现广播逻辑，需要维护一个在线客户端列表
        System.out.println("广播消息: " + message);
    }

    /**
     * 获取客户端IP地址
     */
    private String getClientIp(ChannelHandlerContext ctx) {
        if (ctx == null || !ctx.channel().isActive()) {
            return "unknown";
        }
        return ctx.channel().remoteAddress().toString();
    }

    /**
     * 发送文本消息到客户端
     */
    private void sendTextMessage(io.netty.channel.Channel channel, String message) {
        if (channel != null && channel.isActive()) {
            try {
                channel.writeAndFlush(new TextWebSocketFrame(message));
                System.out.println("发送消息到客户端: " + getClientIp(channel.parent()) + " -> " + message);
            } catch (Exception e) {
                System.err.println("发送消息失败: " + e.getMessage());
            }
        }
    }

    /**
     * 发送HTTP响应
     */
    private void sendHttpResponse(io.netty.channel.Channel channel, FullHttpRequest request, HttpResponseStatus status, String content) {
        if (channel != null && channel.isActive()) {
            try {
                byte[] bytes = content.getBytes(CharsetUtil.UTF_8);
                FullHttpResponse response = new DefaultFullHttpResponse(
                    HttpVersion.HTTP_1_1,
                    status,
                    io.netty.buffer.Unpooled.wrappedBuffer(bytes)
                );
                response.headers().set(HttpHeaderNames.CONTENT_TYPE, "text/plain; charset=UTF-8");
                response.headers().set(HttpHeaderNames.CONTENT_LENGTH, bytes.length);

                channel.writeAndFlush(response);
                System.out.println("发送HTTP响应: " + getClientIp(channel.parent()) + " -> " + status);
            } catch (Exception e) {
                System.err.println("发送HTTP响应失败: " + e.getMessage());
            }
        }
    }

    /**
     * 检查是否为WebSocket升级请求
     */
    private boolean isWebSocketUpgrade(FullHttpRequest request) {
        String upgrade = request.headers().get(HttpHeaderNames.UPGRADE);
        String connection = request.headers().get(HttpHeaderNames.CONNECTION);
        return upgrade != null && upgrade.trim().equalsIgnoreCase("websocket") &&
               connection != null && connection.toLowerCase().contains("upgrade");
    }

    /**
     * 安全关闭Channel
     */
    private void safeCloseChannel(io.netty.channel.Channel channel) {
        if (channel != null && channel.isOpen()) {
            try {
                channel.close().syncUninterruptibly();
            } catch (Exception e) {
                System.err.println("关闭Channel时发生异常: " + e.getMessage());
            }
        }
    }

    /**
     * 从父上下文获取客户端IP
     */
    private String getClientIp(io.netty.channel.Channel channel) {
        if (channel == null) {
            return "unknown";
        }
        ChannelHandlerContext ctx = channel.parent();
        return getClientIp(ctx);
    }

    @Override
    public void userEventTriggered(ChannelHandlerContext ctx, Object evt) throws Exception {
        if (evt instanceof IdleStateEvent) {
            IdleStateEvent event = (IdleStateEvent) evt;
            switch (event.state()) {
                case READER_IDLE:
                    System.out.println("读空闲超时，关闭连接");
                    break;
                case WRITER_IDLE:
                    System.out.println("写空闲超时，关闭连接");
                    break;
                case ALL_IDLE:
                    System.out.println("读写空闲超时，关闭连接");
                    break;
            }
            ctx.close();
        }
    }

    @Override
    public void exceptionCaught(ChannelHandlerContext ctx, Throwable cause) {
        System.err.println("Netty服务异常: " + cause.getMessage());
        safeCloseChannel(ctx.channel());
    }
}