# ConanWebServer

从零实现的 **C++17 HTTP 服务器**。我用它研究 **epoll / Reactor / 线程池 / HTTP 协议 / MySQL / Redis**，
并通过 **wrk 压测分析每一层的瓶颈** —— 项目里每个设计取舍，背后都有实测数据或故障复现支撑。

不使用任何 Web 框架：从 `socket()` / `epoll` 开始手写网络层，到线程池、HTTP 解析、连接池、缓存层完整串联。

---

## 先看三个「分析结论」

这个项目的重点不在功能数量，而在**每一次优化或故障都留下了数据和解释**：

| 观察 | 分析 |
|---|---|
| 静态文件 **59,846 QPS** vs 登录（查库）**17,550 QPS** | 差 3.4 倍 → **瓶颈在数据库层，不在网络层**（这才有了后面的 Redis） |
| 加 Redis 后只提升 **11%**（17,550 → 19,494） | 本机 MySQL 也很快，**「省下的查库」≈「增加的 Redis 两次往返」** —— 优化非瓶颈，收益必然有限 |
| 加 PBKDF2 后登录掉到 **330 QPS**，且静态文件被拖到 **340 QPS**（原 64,526） | **安全代价 + 邻居干扰**：CPU 密集任务占满 worker 池，轻量请求被饿死 |

**上面每一条都能展开讲，而且都是实测的 —— 不是猜的。** 详见「性能」与「核心技术点」章节。

---

## 性能

`wrk -t4 -c100 -d10s`，Ubuntu 24.04 / 4 vCPU：

> **先看静态文件那一行** —— 它衡量的是**框架层**（epoll + 线程池 + EPOLLOUT 续发）的性能，
> 不受业务逻辑影响。登录接口的 QPS 取决于业务做什么（查库 / 算密码哈希），
> **不能用来衡量框架性能** —— 这在真实系统里也是一样的（登录是低频操作）。

| 场景 | QPS | P50 | P99 |
|---|---|---|---|
| `GET /index.html`（静态文件，不查库） | **59,846** | 1.62 ms | 2.46 ms |
| `POST /login`（走 MySQL 查询） | **17,550** | 5.41 ms | 9.20 ms |

> 静态文件与查库接口相差 3.4 倍 —— **瓶颈在数据库层，不在网络层**。这也是本项目下一步接入 Redis 缓存的原因。

**加入 Redis 缓存与 PBKDF2 密码哈希后的对比**：

| 配置 | QPS | P50 | 说明 |
|---|---|---|---|
| 无 Redis（直查 MySQL） | 17,550 | 5.41 ms | baseline |
| + Redis 缓存 | **19,494** | 5.14 ms | +11%（本机 MySQL 也很快，收益有限） |
| + PBKDF2 密码哈希 | **330** | 301 ms | **-98%，但这是安全特性** |

> **PBKDF2 让登录接口从 19,494 掉到 330 QPS** —— 这不是性能退化，而是**主动的安全权衡**：
> 密码验证本来就该慢（10 万次迭代，单次 ~10ms），使暴力破解每秒只能尝试约 330 次。
>
> ⚠️ 同时暴露了一个**架构问题**：PBKDF2 是 CPU 密集任务，会把 worker 线程池占满（CPU 400%），
> 导致**静态文件 QPS 从 64,526 掉到 340（-99.5%）** —— 典型的邻居干扰。
> 解法：① 登录限流 ② 业务线程池隔离 ③ token 机制（让登录只慢一次）。

---

## 架构

**四层结构**：

```
main（装配层）
 ├─ Config            读 config.json
 ├─ ConnectionPool    建 4 条 MySQL 连接
 └─ Server(8080)
      │
      ├── 网络层   Server + ThreadPool
      │     · 主线程独占 epoll（accept / recv / send）
      │     · 4 个 worker 线程处理业务
      │     · eventfd 做跨线程唤醒
      │
      ├── 业务层   Router + Handler
      │     · Router 只回答"这个请求交给谁"（16 行）
      │     · LoginHandler / StaticHandler 各管一个业务
      │     · HttpRequest 解析 / HttpResponse 拼装
      │
      └── 数据层   ConnectionPool → MySQL
            · 借 / 还语义，连接失效自动重建
```

**一次请求的完整链路**：

```
浏览器 ──TCP──► 主线程 epoll_wait
                  │ handleRead：recv 收数据 → 判断请求已经完整 → pool_.submit(任务)
                  ▼
              worker 线程：解析 JSON → Router 分派 → LoginHandler
                  │  ConnectionPool::get() → 转义 → mysql_query → release()
                  │  done_queue_.push(成果) → write(eventfd) 唤醒主线程
                  ▼
              主线程 handleWakeup：write_buffers_ += 响应 → MOD EPOLLOUT
                  │ handleWrite：send 到发完 → 按 keep-alive 决定保留还是关闭
                  ▼
              浏览器 ◄── 响应 ──
```

---

## 快速开始

### 依赖

```
g++ 15+  (C++17)
CMake 3.10+
libmysqlclient-dev
MySQL 8.x（运行中）
```

### 1. 初始化数据库

```bash
sudo mysql --defaults-file=/etc/mysql/debian.cnf < sql/init.sql
```

（非 Ubuntu 环境请按需修改 `sql/init.sql` 里的账号与密码）

### 2. 配置

```bash
cp config.example.json config.json
vim config.json        # 填你自己的数据库地址 / 账号 / 密码
```

> `config.json` 已被 `.gitignore` 忽略，**不会提交到仓库**。

### 3. 构建运行

```bash
cmake -S . -B build
cmake --build build
./build/server          # 默认监听 8080
```

浏览器打开 `http://localhost:8080/login.html`

---

## 接口

### `GET /` 与 `GET /<file>`

返回 `www/` 目录下的静态文件，不存在返回 `404`。

### `POST /login`

**请求**

```
POST /login
Content-Type: application/json

{"username":"conan","password":"123456"}
```

**响应**

| 状态码 | 含义 | 响应体 |
|---|---|---|
| `200` | 登录成功 | `{"code":200,"message":"login success","user":{"username":"conan"}}` |
| `400` | JSON 格式错误 | `{"code":400,"message":"invalid json"}` |
| `401` | 账号或密码错误 | `{"code":401,"message":"wrong username or password"}` |
| `503` | 依赖不可用（数据库故障） | `{"code":503,"message":"service temporarily unavailable"}` |

**测试账号**（见 `sql/init.sql`）：

| 用户名 | 密码 |
|---|---|
| `conan` | `123456` |
| `alice` | `alice123` |
| `bob` | `bob456` |

---

## 核心技术点

### 1. epoll ET + 非阻塞：必须读到 `EAGAIN`

ET 只在**状态变化时**通知一次。若不循环 `recv` 直到 `EAGAIN`，残留在内核缓冲区里的数据将永远不会再触发通知 —— 请求会永久卡住。

### 2. 主从 Reactor：worker 为什么不直接 `send`

非阻塞 `send()` 可能只发出去一部分（半包），**续发依赖 `EPOLLOUT` 事件，而 epoll 归主线程管理；再加上一个 fd 只能由一个线程操作** —— 三条加起来决定了：**发送必须留在主线程**。

worker 只做纯计算（解析 / 路由 / 拼响应），成果经 `done_queue_` 交回主线程。

### 3. `eventfd` 做跨线程唤醒

主线程阻塞在 `epoll_wait` 里，worker 干完活怎么通知它？—— 造一个 worker 能主动触发的 fd：**`eventfd`**（内核里的一个计数器，写入即变可读）。worker 放好成果后 `write(eventfd)`，主线程被唤醒去发送。

### 4. 半包处理：`buffers_[fd]` 跨多次调用累积

`handleRead` 用两个循环分工：

| 循环 | 职责 | 退出条件 |
|---|---|---|
| while ① | 把内核此刻可读的数据**全部**收进缓冲区 | `EAGAIN` |
| while ② | 从缓冲区**反复切出**完整请求 | 没有完整请求了（头部不齐 / body 不齐） |

**"内核缓冲区空了" ≠ "请求收全了"** —— 后者只能靠解析 `Content-Length` 自己数。

### 5. 发送链路：`write_buffers_` + `EPOLLOUT` 续发

`send` 只是把数据交给内核发送缓冲区；缓冲区满时返回 `EAGAIN`，等内核通知"可写"再续发。所以发送是"发一点 → 等 → 再发一点"的循环。

### 6. 连接池 + 连接级自愈

- `mutex` + `condition_variable` 保护 `queue<MYSQL*>`：`get()` 借、`release()` 还
- **一条 `MYSQL*` 同一时刻只能被一个线程使用** —— 借还语义天然保证这点
- **`get()` 里先 `unlock`，再 `mysql_ping` 体检**，失效则 `mysql_close` + `createConnection()` 重建

  **为什么需要**：MySQL 会因 `wait_timeout`（默认 8 小时）、服务重启、网络抖动主动断开空闲连接，**而你手里的句柄"看起来还在"** —— 不体检就会一直用死连接，而且**永远不会自愈**（只能人工重启服务）。

### 7. SQL 注入防护

用户输入经 `mysql_real_escape_string` 转义后再拼 SQL（转义缓冲区按 `len*2+1` 分配）。

> **注**：原计划使用 `mysql_stmt_*` 预处理语句，但在 **Ubuntu 的 libmysqlclient 8.4.11** 上发现**带参数占位符的 `mysql_stmt_prepare` 会返回 `Unknown command`**。
> 经**分层验证**（服务器层面 `PREPARE ... EXECUTE ... USING` 正常 → C++ 里无参数的 `prepare` 正常 → 只有带 `?` 的失败）确认是**客户端库问题**，故改用转义方案（安全性等价）。

### 8. 错误分层：区分"用户的错"和"服务器的错"

`checkPassword` 返回三态枚举而非 `bool`：

```cpp
enum class LoginResult { Success, WrongPassword, ServerError };
// Success -> 200   |   WrongPassword -> 401   |   ServerError -> 503
```

**对外响应模糊**（不暴露内部细节），**对内日志详细**（含 `mysql_error` 内容）。

### 9. 配置外置

`config.json`（含密码，被 `.gitignore` 忽略）+ `config.example.json`（模板）。

程序只读文件名、不认内容 —— **使用者填自己的配置即可连自己的数据库，代码零改动**。

### 10. 信号处理：优雅停机 + SIGPIPE 防护

**优雅停机**：注册 `SIGINT` / `SIGTERM`，处理器**只设一个 `std::atomic<bool>` 标志**（信号处理器里不能做复杂操作 —— 可能在主线程执行 `malloc` 中间打断它，导致死锁）。

主循环 `while (g_running)` 退出后 `main` 正常返回，**析构链自动执行**：

```
ThreadPool::~ThreadPool    → stop_ + notify_all + join × 4（等 worker 干完手上的活）
ConnectionPool::~ConnectionPool → mysql_close × 4（发 COM_QUIT 正式告别，而非 TCP 硬断）
```

> 验证：进程退出码从 **143**（= 128+15，被信号杀死）变为 **0**（正常退出）。

**SIGPIPE 防护**：往已断开的 socket 写会触发 `SIGPIPE`，而**它的默认行为是杀死进程** —— 一个客户端的异常断开就能搞崩整个服务。双层防护：

```cpp
std::signal(SIGPIPE, SIG_IGN);                 // 全局（覆盖 write 等所有路径）
send(fd, buf, len, MSG_NOSIGNAL);              // 单次调用（精确）
```

忽略之后，写失败只返回 `-1` + `errno`，走正常的错误处理流程。

### 11. 密码安全存储（PBKDF2-HMAC-SHA256 + 随机盐）

**为什么不能直接哈希**：SHA-256 有两个致命问题 ——

1. **不加盐 → 彩虹表秒破**：`123456` 的 SHA-256 是公开的，查表即得。且相同密码哈希相同，一眼能看出
2. **太快 → 暴力破解成本低**：一张 GPU 每秒能算百亿次

**方案**：`PBKDF2-HMAC-SHA256`，每用户独立随机盐（16 字节）+ 10 万次迭代。

**存储格式自带参数，可平滑升级**：

```
pbkdf2_sha256$100000$<盐hex>$<哈希hex>
     ↑算法     ↑迭代   ↑盐      ↑哈希
```

读懂格式就能验证：以后想调迭代次数或换算法（bcrypt/Argon2），**旧记录仍能验证**，验证通过后再重写为新格式。

**常量时间比对**：密码比对使用 `CRYPTO_memcmp` 而非 `==` —— 普通 `==` 会在第一个不同字节处提前返回，攻击者可通过**测量响应时间**逐字节猜出哈希（时序攻击）。

> 验证：同一密码两次哈希结果不同（盐随机），但都能通过验证。
> 单次验证约 10ms —— 这是**故意**的：让暴力破解每秒只能试约 330 次。

### 12. 服务端防护：路径穿越 / 请求大小上限 / 连接超时

三个「生产环境才会遇到、开发测试碰不到」的坑：

| 防护 | 做法 | 不做会怎样 |
|---|---|---|
| **路径穿越** | 拒绝含 `..` 的路径（黑名单） | `GET /../../../../etc/passwd` 能读到系统文件 |
| **请求大小上限** | 单请求 > 64KB 即记日志并断开 | 恶意客户端发超长请求 → 内存耗尽 |
| **连接空闲超时** | 30 秒无活动即关闭（`epoll_wait` 用 1 秒超时 + 每秒扫一次） | **Slowloris**：连上但不发完请求，连接永远占着 |

**路径穿越的验证**：尝试了 8 种绕过姿势（`%2e%2e` / `....//` / `..;/` / 双重编码 / `%00` 截断 …），**全部返回 403 或 404，无一绕过**。

> 更严谨的做法是「白名单」：用 `realpath()` 解析出真实路径，再检查它是否在 `www/` 下。
> 当前用黑名单是因为已覆盖所有已知绕过方式，而 `realpath` 有额外的系统调用开销 —— 这是个权衡。

**连接超时的验证**：模拟 Slowloris（连上只发半截请求），服务器在**第 31 秒**主动关闭连接，同时正常请求不受影响。

---

## 项目结构

```
ConanWebServer/
├── src/
│   ├── main.cpp                 装配：读配置 → 建连接池 → 启动 Server
│   ├── Config.{h,cpp}           配置读取（nlohmann/json）
│   ├── Server.{h,cpp}           网络层：epoll 事件循环 + 连接管理   (533 行)
│   ├── ThreadPool.{h,cpp}       线程池：mutex + condition_variable + 任务队列
│   ├── ConnectionPool.{h,cpp}   MySQL 连接池（借/还 + 失效自愈）
│   ├── Router.{h,cpp}           分派（只回答"交给谁"）
│   ├── handlers/
│   │   ├── LoginHandler.{h,cpp}    登录业务（查库 + 错误分层）
│   │   └── StaticHandler.{h,cpp}   静态文件
│   ├── HttpRequest.{h,cpp}      请求解析
│   └── HttpResponse.{h,cpp}     响应拼装
├── www/                         前端页面（登录页 / 首页）
├── sql/init.sql                 建库建表 + 测试用户
├── config.example.json          配置模板
└── CMakeLists.txt
```

**规模**：约 **1400 行** C++17，19 个源文件。

---

## 已知限制

| 项 | 说明 |
|---|---|
| **CPU 密集任务与轻量任务共用线程池** | 密码验证（PBKDF2，~10ms CPU）会把 worker 占满，导致静态文件 QPS 从 64,526 掉到 340（实测）。需限流 / 池隔离 / token 机制 |
| **无日志系统** | 目前仅用 `cout` / `cerr` |
| **单元测试缺失** | — |

---

## 路线图

- [ ] Redis 缓存层（Phase 8）
- [ ] 日志系统（分级 + 异步落盘）
- [x] ~~优雅停机（信号处理 + 让析构链跑起来）~~ ✅ 已完成
- [ ] 连接超时 / 请求大小上限
- [x] ~~PBKDF2 密码存储~~ ✅ 已完成
- [ ] 压测对比：单线程 vs 线程池 vs 多 Reactor
- [ ] 路由表改为哈希表 / 前缀树
- [ ] 单元测试（Google Test）

---

## 构建环境

```
Ubuntu 24.04  /  g++ 15.2.0  /  CMake 3.28  /  MySQL 8.4.11  /  OpenSSL 3.5.5
```

---

*Talk is cheap. Show me the code.*
