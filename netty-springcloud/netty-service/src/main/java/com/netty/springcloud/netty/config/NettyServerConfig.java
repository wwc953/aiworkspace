package com.netty.springcloud.netty.config;

import com.netty.springcloud.common.NettyConstants;
import io.netty.bootstrap.ServerBootstrap;
import io.netty.channel.ChannelFuture;
import io.netty.channel.EventLoopGroup;
import io.netty.channel.nio.NioEventLoopGroup;
import io.netty.channel.socket.nio.NioServerSocketChannel;
import lombok.Data;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.stereotype.Component;

import javax.annotation.PostConstruct;
import javax.annotation.PreDestroy;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Netty服务器配置
 */
@Data
@Component
@ConfigurationProperties(prefix = "netty.server")
public class NettyServerConfig {

    private int port = NettyConstants.DEFAULT_PORT;
    private int bossThreads = NettyConstants.DEFAULT_BOSS_THREADS;
    private int workerThreads = NettyConstants.DEFAULT_WORKER_THREADS;
    private int backlog = NettyConstants.DEFAULT_SO_BACKLOG;
    private boolean sslEnabled = false;
    private String host = "0.0.0.0";

    private EventLoopGroup bossGroup;
    private EventLoopGroup workerGroup;
    private ServerBootstrap bootstrap;
    private ExecutorService executorService;
    private ChannelFuture serverChannelFuture;

    @PostConstruct
    public void init() {
        this.bossGroup = new NioEventLoopGroup(bossThreads);
        this.workerGroup = new NioEventLoopGroup(workerThreads);
        this.executorService = Executors.newCachedThreadPool();

        this.bootstrap = new ServerBootstrap();
        this.bootstrap.group(bossGroup, workerGroup)
                .channel(NioServerSocketChannel.class)
                .childHandler(new NettyServerInitializer())
                .option(io.netty.channel.ChannelOption.SO_BACKLOG, backlog);

        Runtime.getRuntime().addShutdownHook(new Thread(this::destroy));
    }

    /**
     * 启动Netty服务器
     */
    public void start() throws Exception {
        serverChannelFuture = bootstrap.bind(port).sync();
        System.out.println("Netty服务器启动成功，端口: " + port);
    }

    /**
     * 停止Netty服务器
     */
    public void stop() {
        if (serverChannelFuture != null) {
            try {
                serverChannelFuture.channel().close().sync();
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    @PreDestroy
    public void destroy() {
        shutdownGracefully();
    }

    /**
     * 优雅关闭
     */
    public void shutdownGracefully() {
        try {
            if (executorService != null && !executorService.isShutdown()) {
                executorService.shutdown();
            }

            if (workerGroup != null) {
                workerGroup.shutdownGracefully();
            }

            if (bossGroup != null) {
                bossGroup.shutdownGracefully();
            }

            System.out.println("Netty服务器已关闭");
        } catch (Exception e) {
            System.err.println("关闭Netty服务器时发生异常: " + e.getMessage());
        }
    }
}