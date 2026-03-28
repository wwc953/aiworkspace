-- 创建测试数据库和用户
CREATE DATABASE IF NOT EXISTS testdb;
USE testdb;

-- 创建测试用户（如果不存在）
CREATE USER IF NOT EXISTS 'testuser'@'%' IDENTIFIED BY 'testpass';
GRANT ALL PRIVILEGES ON testdb.* TO 'testuser'@'%';
FLUSH PRIVILEGES;

-- 创建简单的测试表
CREATE TABLE IF NOT EXISTS users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    email VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 插入一些测试数据
INSERT INTO users (name, email) VALUES
('张三', 'zhangsan@example.com'),
('李四', 'lisi@example.com'),
('王五', 'wangwu@example.com')
ON DUPLICATE KEY UPDATE name=name;

-- 查看MySQL版本信息
SELECT VERSION();


DROP FUNCTION IF EXISTS http_get;
DROP FUNCTION IF EXISTS http_post;
DROP FUNCTION IF EXISTS http_put;
DROP FUNCTION IF EXISTS http_delete;

create function http_get returns string soname 'mysql-udf-http.so';
create function http_post returns string soname 'mysql-udf-http.so';
create function http_put returns string soname 'mysql-udf-http.so';
create function http_delete returns string soname 'mysql-udf-http.so';


