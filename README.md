# 高性能C++用户中心后端服务
高性能 C++ 用户中心后端服务，提供用户注册、登录、认证、资料管理、安全防护等完整功能。基于 Reactor 模型，从零实现 HTTP 协议解析、异步日志、连接池、定时器等核心组件

## 技术栈
1. 语言标准：C++17
2. 网络模型：Reactor + 线程池 (epoll + 非阻塞 I/O)
3. 协议解析：自研 HTTP/1.1 状态机
4. 数据库：MySQL (Connector/C++) + 自研连接池
5. 缓存：Redis (redis-plus-plus) + 连接池
6. 密码加密：bcrypt
7. JSON 处理：nlohmann/json
8. 日志系统：自研异步 Logger
9. 构建工具：CMake
10. 定时器：自研基于 timerfd + 最小堆的定时器

## 核心功能
### 用户认证与授权
1. 用户注册（密码 bcrypt 加密，生成 UUID 对外标识）
2. 用户登录（生成 256 位随机 Token，存储于 Redis，有效期 7 天）
3. Token 认证（Authorization: Bearer <token>）
4. 登出（删除当前 Token）
5. 修改密码（验证旧密码，更新后强制清除所有 Token）

### 用户信息管理
1. 获取用户信息（支持 Redis 缓存，随机 TTL 防雪崩，空值缓存防穿透）
2. 修改用户资料（支持部分字段更新：昵称、头像、简介、性别）

### 安全与可观测性
1. 请求限流（基于 Redis 固定窗口计数器，IP 级别 60 次/分钟）
2. 空闲连接超时（基于 timerfd + 最小堆的定时器，自动关闭空闲连接）
3. 统一错误码（响应格式 {"code":0,"msg":"success","data":{}}）

### 基础设施
1. 线程池 基于C++17 实现
2. MySQL 连接池（线程安全，支持动态扩容）
3. Redis 连接池（redis-plus-plus 内置）
4. 定时器（绝对时间调度，支持一次性/周期性任务，避免漂移）
5. 异步日志（多级别输出，支持日志文件滚动）


## 快速开始
### 环境要求
Linux (Ubuntu 20.04+)
CMake 3.20+
MySQL 8.0+
Redis 6.0+
OpenSSL
bcrypt (libbcrypt)

### MySQL表结构
#### 业务已实现
##### 1. 核心用户表
``` sql
CREATE TABLE `users` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `uuid` CHAR(36) NOT NULL,
    `username` VARCHAR(64) NOT NULL,
    `email` VARCHAR(128) NOT NULL,
    `phone` VARCHAR(20) DEFAULT NULL,
    `password_hash` VARCHAR(255) NOT NULL,
    `status` TINYINT NOT NULL DEFAULT 1,
    `role` VARCHAR(32) NOT NULL DEFAULT 'user',
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    `last_login_at` TIMESTAMP NULL DEFAULT NULL,
    `last_login_ip` VARCHAR(45) NULL DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_uuid` (`uuid`),
    UNIQUE KEY `uk_username` (`username`),
    UNIQUE KEY `uk_email` (`email`),
    UNIQUE KEY `uk_phone` (`phone`),
    KEY `idx_status` (`status`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

##### 2. 用户资料表
``` sql
CREATE TABLE `user_profiles` (
    `user_id` INT UNSIGNED NOT NULL,
    `nickname` VARCHAR(64) DEFAULT NULL,
    `avatar_url` VARCHAR(256) DEFAULT NULL,
    `bio` TEXT,
    `birthday` DATE DEFAULT NULL,
    `gender` TINYINT NOT NULL DEFAULT 0,
    `location` VARCHAR(128) DEFAULT NULL,
    PRIMARY KEY (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

#### 后续待实现
##### 1. 第三方登录表
``` sql
CREATE TABLE `user_oauth` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `user_id` INT UNSIGNED NOT NULL,
    `provider` VARCHAR(32) NOT NULL,
    `openid` VARCHAR(128) NOT NULL,
    `unionid` VARCHAR(128) DEFAULT NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_provider_openid` (`provider`, `openid`),
    KEY `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```
##### 2. 登录日志表
``` sql
CREATE TABLE `user_login_logs` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `user_id` INT UNSIGNED NOT NULL,
    `login_time` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `ip` VARCHAR(45) NOT NULL,
    `user_agent` VARCHAR(255) DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_user_id_time` (`user_id`, `login_time`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```
##### 3. 关注关系表 
``` sql
CREATE TABLE `user_follows` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `follower_id` INT UNSIGNED NOT NULL,
    `followee_id` INT UNSIGNED NOT NULL,
    `status` TINYINT NOT NULL DEFAULT 1,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_follow` (`follower_id`, `followee_id`),
    KEY `idx_follower` (`follower_id`),
    KEY `idx_followee` (`followee_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```
##### 4. 用户动态表
``` sql
CREATE TABLE `user_posts` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `user_id` INT UNSIGNED NOT NULL,
    `content` TEXT NOT NULL,
    `like_count` INT UNSIGNED NOT NULL DEFAULT 0,
    `comment_count` INT UNSIGNED NOT NULL DEFAULT 0,
    `status` TINYINT NOT NULL DEFAULT 1 COMMENT '1-正常，2-删除',
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_user_id` (`user_id`),
    KEY `idx_created_at` (`created_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```
##### 5. 动态点赞表
``` sql
CREATE TABLE `user_post_likes` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `post_id` BIGINT UNSIGNED NOT NULL,
    `user_id` INT UNSIGNED NOT NULL,
    `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_post_user` (`post_id`, `user_id`),
    KEY `idx_user_id` (`user_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

### 编译与运行
1. git clone https://github.com/ZeroDrea/user-center.git

2. cd user-center

3. mkdir build && cd build

4. cmake .. && make -j$(nproc)

5. ./bin/user_center

### 测试 API
1. 注册用户
curl -X POST http://127.0.0.1:8888/user/register \
  -H "Content-Type: application/json" \
  -d '{"username":"name_test","password":"123456","email":"name_test@example.com"}'

2. 登录获取 Token
TOKEN=$(curl -s -X POST http://127.0.0.1:8888/user/login \
  -H "Content-Type: application/json" \
  -d '{"username":"name_test","password":"123456"}' | jq -r '.data.token')

3. 获取用户信息
curl -H "Authorization: Bearer $TOKEN" http://127.0.0.1:8888/user/info

4. 修改资料
curl -X PATCH http://127.0.0.1:8888/user/profile \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"nickname":"new_nickname","bio":"Hello world"}'

5. 修改密码
curl -X PUT http://127.0.0.1:8888/user/password \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{"old_password":"123456","new_password":"newpass"}'

6. 登出
curl -X POST http://127.0.0.1:8888/user/logout \
  -H "Authorization: Bearer $TOKEN"

## 项目结构
.
├── include/           # 头文件
│   ├── auth/          # Token 管理
│   ├── common/        # 错误码、响应辅助
│   ├── db/            # MySQL 连接池
│   ├── http/          # HTTP 请求/响应、解析器
│   ├── net/           # EventLoop、Channel、Connection、TcpServer
│   ├── router/        # 路由器
│   ├── service/       # 业务逻辑（UserService）
│   └── utils/         # 日志等工具
├── src/               # 源文件（对应子目录）
└── CMakeLists.txt

## 设计模式
1. Reactor 模式：EventLoop + Channel 处理 I/O 事件
2. 对象池模式：MySQL 连接池、Redis 连接池、线程池
3. 状态模式：HTTP 解析状态机
4. 策略模式：Router 动态分发请求
5. 观察者模式：回调机制（closeCallback_、httpRequestCallback_）
6. 单例模式：日志、连接池、TokenManager

## 性能特性
1. 单 Reactor + 多线程模型，I/O 线程非阻塞，业务逻辑提交至线程池
2. Redis 缓存用户信息，命中率 > 90%
3. 定时器使用绝对时间调度，避免漂移
4. epoll_wait 超时 10ms，结合定时器实现低延迟
5. 连接复用（HTTP Keep-Alive），减少 TCP 握手

## 后续计划
1. 实现关注/粉丝功能
2. 动态发布与时间线
3. 第三方登录（微信、QQ）
4. 支持 HTTPS
