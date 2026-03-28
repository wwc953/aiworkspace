-- Nacos Database Initialization Script

CREATE DATABASE IF NOT EXISTS nacos_devtest CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE nacos_devtest;

-- User table for Nacos authentication
CREATE TABLE IF NOT EXISTS users (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(50) NOT NULL UNIQUE,
    password VARCHAR(200) NOT NULL,
    enabled BOOLEAN DEFAULT TRUE
);

-- Insert default user
INSERT INTO users (username, password, enabled) VALUES
('nacos', 'e10adc3949ba59abbe56e057f20f883e', TRUE) -- MD5 of "nacos"
ON DUPLICATE KEY UPDATE password=VALUES(password), enabled=VALUES(enabled);

-- Service registry tables
CREATE TABLE IF NOT EXISTS config (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    data_id VARCHAR(255) NOT NULL,
    group_id VARCHAR(255),
    content TEXT,
    md5 VARCHAR(32),
    gmt_create DATETIME,
    gmt_modified DATETIME,
    src_user VARCHAR(128),
    app_name VARCHAR(128),
    tenant_id VARCHAR(128) DEFAULT '',
    encrypted_data_key TEXT
);

CREATE INDEX idx_config_dataid ON config(data_id);
CREATE INDEX idx_config_group ON config(group_id);

-- Configuration change history
CREATE TABLE IF NOT EXISTS config_history (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    data_id VARCHAR(255) NOT NULL,
    group_id VARCHAR(255),
    beta_ips VARCHAR(1024),
    info_md5 VARCHAR(32),
    content TEXT,
    op_type INT,
    src_app_name VARCHAR(128),
    src_user VARCHAR(128),
    gmt_create DATETIME,
    tenant_id VARCHAR(128) DEFAULT ''
);

CREATE INDEX idx_history_dataid ON config_history(data_id);

-- Service discovery tables
CREATE TABLE IF NOT EXISTS service (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    service_name VARCHAR(255) NOT NULL,
    protect_threshold DOUBLE DEFAULT 0.9,
    metadata TEXT,
    gmt_create DATETIME DEFAULT CURRENT_TIMESTAMP,
    gmt_modified DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    namespace_id VARCHAR(255)
);

CREATE TABLE IF NOT EXISTS instance (
    id BIGINT PRIMARY KEY AUTO_INCREMENT,
    service_name VARCHAR(255) NOT NULL,
    ip VARCHAR(50) NOT NULL,
    port INT NOT NULL,
    weight DOUBLE DEFAULT 1.0,
    healthy BOOLEAN DEFAULT TRUE,
    cluster_name VARCHAR(255) DEFAULT 'DEFAULT',
    ephemeral BOOLEAN DEFAULT TRUE,
    metadata TEXT,
    gmt_create DATETIME DEFAULT CURRENT_TIMESTAMP,
    gmt_modified DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    namespace_id VARCHAR(255),
    enable BOOLEAN DEFAULT TRUE,
    protect_threshold DOUBLE DEFAULT 0.9
);

CREATE INDEX idx_instance_service ON instance(service_name);
CREATE INDEX idx_instance_ip_port ON instance(ip, port);

-- Grant permissions
GRANT ALL PRIVILEGES ON nacos_devtest.* TO 'nacos'@'%';
FLUSH PRIVILEGES;