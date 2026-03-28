-- ==================================================
-- MySQL UDF 高并发测试环境初始化脚本
-- 版本: 2.0 (修复 Error 1442 和 415 错误)
-- ==================================================


DROP FUNCTION IF EXISTS http_get;
DROP FUNCTION IF EXISTS http_post;
DROP FUNCTION IF EXISTS http_put;
DROP FUNCTION IF EXISTS http_delete;

create function http_get returns string soname 'mysql-udf-http.so';
create function http_post returns string soname 'mysql-udf-http.so';
create function http_put returns string soname 'mysql-udf-http.so';
create function http_delete returns string soname 'mysql-udf-http.so';

-- 1. 创建数据库
CREATE DATABASE IF NOT EXISTS udf_test_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE udf_test_db;

-- 2. 清理旧环境 (防止报错)
DROP TRIGGER IF EXISTS trg_enqueue_order;
DROP TABLE IF EXISTS orders;
DROP TABLE IF EXISTS orders_queue;

-- 3. 创建业务表 (orders)
-- 用于存储订单核心数据
CREATE TABLE orders (
    id BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '主键ID',
    order_no VARCHAR(64) NOT NULL COMMENT '订单编号',
    amount DECIMAL(10, 2) NOT NULL DEFAULT 0.00 COMMENT '订单金额',
    status VARCHAR(20) DEFAULT 'PENDING' COMMENT '状态: PENDING, SENT, FAILED',
    udf_response TEXT COMMENT 'UDF 调用返回的 JSON 结果',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
    INDEX idx_order_no (order_no),
    INDEX idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='订单业务表';

-- 4. 创建队列表 (orders_queue)
-- 用于解耦 Trigger 和 UDF 调用，避免 Error 1442
CREATE TABLE orders_queue (
    id BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '队列ID',
    order_id BIGINT NOT NULL COMMENT '关联 orders 表的 ID',
    attempt_count INT DEFAULT 0 COMMENT '重试次数',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '入队时间',
    INDEX idx_order_id (order_id),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='UDF 异步处理队列表';

-- 5. 创建触发器 (trg_enqueue_order)
-- 逻辑：仅将新订单 ID 插入队列表，不调用 UDF，不更新原表
DELIMITER $$

CREATE TRIGGER trg_enqueue_order
AFTER INSERT ON orders
FOR EACH ROW
BEGIN
    -- 异步入队，操作非常快，不会阻塞主业务
    INSERT INTO orders_queue (order_id, attempt_count) 
    VALUES (NEW.id, 0);
END$$

DELIMITER ;

-- 6. 验证安装
SELECT '✅ Database setup complete.' AS status;
SELECT '✅ Tables created: orders, orders_queue.' AS status;
SELECT '✅ Trigger installed: trg_enqueue_order.' AS status;
SELECT '⚠️  Next step: Start worker.py to process the queue.' AS status;