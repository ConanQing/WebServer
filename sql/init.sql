-- ============================================================
--  ConanWebServer 数据库初始化
--  执行方式：sudo mysql --defaults-file=/etc/mysql/debian.cnf < sql/init.sql
-- ============================================================

-- ---------- 1. 建库 ----------
CREATE DATABASE IF NOT EXISTS conan_db
    DEFAULT CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

-- ---------- 2. 建项目专用账号（最小权限：只能动 conan_db）----------
CREATE USER IF NOT EXISTS 'conan'@'localhost' IDENTIFIED BY 'Conan@2026';
ALTER USER 'conan'@'localhost' IDENTIFIED BY 'Conan@2026';
GRANT ALL PRIVILEGES ON conan_db.* TO 'conan'@'localhost';
FLUSH PRIVILEGES;

-- ---------- 3. 切到新库 ----------
USE conan_db;

-- ---------- 4. 建 users 表 ----------
CREATE TABLE IF NOT EXISTS users (
    id            INT AUTO_INCREMENT PRIMARY KEY,
    username      VARCHAR(50)  NOT NULL UNIQUE,
    password_hash CHAR(64)     NOT NULL,          -- SHA-256 的十六进制，固定 64 字符
    created_at    TIMESTAMP    DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

-- ---------- 5. 插一个测试用户：conan / 123456 ----------
DELETE FROM users WHERE username = 'conan';
INSERT INTO users (username, password_hash) VALUES ('conan', SHA2('123456', 256));

-- ---------- 6. 测试用户（方便手工验证）----------
INSERT INTO users (username, password_hash) VALUES
    ('alice', SHA2('alice123', 256)),
    ('bob',   SHA2('bob456',   256))
ON DUPLICATE KEY UPDATE password_hash = VALUES(password_hash);
