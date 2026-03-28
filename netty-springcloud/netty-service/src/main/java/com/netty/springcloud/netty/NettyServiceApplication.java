package com.netty.springcloud.netty;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.cloud.client.discovery.EnableDiscoveryClient;

@SpringBootApplication
@EnableDiscoveryClient
public class NettyServiceApplication {

    public static void main(String[] args) {
        SpringApplication.run(NettyServiceApplication.class, args);
    }
}