# SimpleWebServer

> 这个一个使用C++实现的简单的WebServer小练习。
> 运行于Windows系统，使用MinGW编译。
> MinGW (g++) + CMake + GNU Make
> 主要有四种并发模型：迭代、线程、线程池、IOCP。

## 快速开始

```bash
./build.sh debug          # 生成 build_debug/
cd build_debug && make    # 编译，产物在 bin/

./bin/webserver.exe 8080 pool 4
#                 端口  模型  工作线程数
```

然后浏览器打开 <http://127.0.0.1:8080/>，或者：

```bash
curl http://127.0.0.1:8080/hello
curl -X POST -d "ping" http://127.0.0.1:8080/echo
```

四种并发模型都可以用第二个参数切换：

```bash
./bin/webserver.exe 8080 iterative   # 迭代式：一次只服务一个连接
./bin/webserver.exe 8080 thread      # 每来一个连接开一个线程
./bin/webserver.exe 8080 pool 8      # 固定大小线程池（默认）
./bin/webserver.exe 8080 iocp 8      # IOCP 完成端口，事件驱动
```

## 3. 项目结构

```
SimpleWebServer/
├── CMakeLists.txt          构建脚本，核心代码编成 sws_core 静态库
├── build.sh                debug / release 一键配置
├── src/
│   ├── main.cpp            入口：解析参数、组装路由、选择并发模型
│   ├── logger.*            带级别和线程 id 的日志
│   ├── socket_ops.*        WinSock 的薄封装（整个项目唯一碰系统 API 的地方之一）
│   ├── buffer.*            应用层收发缓冲区
│   ├── server.*            Server 抽象基类：accept 主循环
│   ├── iterative_server.h  模型一：迭代
│   ├── thread_server.h     模型二：每连接一线程
│   ├── pool_server.h       模型三：线程池
│   ├── thread_pool.*       可复用的线程池
│   ├── iocp_server.*       模型四：IOCP 完成端口
│   ├── http_request.*      请求解析器
│   ├── http_response.*     响应构造与序列化
│   ├── http_connection.*   一条连接的完整生命周期
│   ├── router.*            路由表
│   └── static_file_handler.*  静态文件服务（含路径穿越防护）
├── tests/                  单元测试 + 三组测试
├── bench/                  压测客户端
└── wwwroot/                静态文件根目录
```

## 并发模型

### 迭代式 Iterative

[iterative_server.h](src/iterative_server.h)

```
主线程: accept ─► 处理连接A直到关闭 ─► accept ─► 处理连接B ...
```

代码最简单，但是比较离谱，**同一时刻只能服务一个客户端**。A 还连着，B 就得排队。


### 每连接一线程 Thread

[thread_server.h](src/thread_server.h)

```
主线程:   accept ─► 开线程 ─► accept ─► 开线程 ...
              │              │
工作线程:    处理A          处理B
```

能并发了，连接之间互不阻塞。但每个连接都会创建一个 `std::thread`：
线程创建/销毁有开销，且**连接数 = 线程数**，一万个连接就是一万个线程，
内存和线程调度都会吃不消。连接结束时线程自行退出。

### 线程池 Pool

[pool_server.h](src/pool_server.h) + [thread_pool.cpp](src/thread_pool.cpp)

```
主线程:   accept ─► 把任务丢进队列 ─► accept ...
                       │ 任务队列
          ┌────────────┼────────────┐
       工作线程1     工作线程2     工作线程N   （预先创建，反复复用）
```

线程数固定（默认等于 CPU 核数），连接被抽象成"任务"投递给线程池。
核心是三件套：**互斥锁 + 条件变量 + 任务队列**（见 `thread_pool.cpp`）：

- 工作线程在队列空时 `wait` 睡眠，不白耗 CPU；
- 新任务入队后 `notify_one` 叫醒一个线程；
- 关闭时 `notify_all` 并 `join` 所有线程。

这是工业界最常用的阻塞式模型，用少量线程扛大量连接（靠 Keep-Alive 和"连接空闲时线程可以服务别人"来提高利用率）。

### IOCP（Windows 的事件驱动）

[iocp_server.cpp](src/iocp_server.cpp)

通常工业界用的webserver使用epoll，但是windows没有epoll，所以用IOCP。
前面三种都是"**一个线程死等一个连接的 recv**"。IOCP 的思想完全不同：
发起收发时**不等它完成**，系统在操作真正完成后，把结果塞进一个"完成端口"队列，
少量工作线程统一从队列里取结果处理。

```
┌─────────┐  发起 AcceptEx/WSARecv/WSASend（异步，立刻返回）
│ 主线程   │
└────┬────┘
     │ 所有 socket 和 IO 操作都绑定到
     ▼
┌────────────────────────┐
│  IOCP 完成端口（内核队列）│  ◄── 系统："你要的数据到了，327 字节"
└───────────┬────────────┘
            │ GetQueuedCompletionStatus()
      ┌─────┴─────┐
   工作线程1   工作线程2 ...   谁空闲谁取，一个线程能服务成千上万连接
```

---

## 性能测试

[bench/bench.cpp](bench/bench.cpp) 是一个自带的多线程压测客户端，输出
QPS（每秒请求数）、平均延迟和 P50/P95/P99 分位延迟。

```bash
# 先启动服务器（release 构建）
./bin/webserver.exe 8080 iocp 8

# 另开终端：8 个并发线程，每线程 30000 个请求（Keep-Alive）
./bin/bench.exe 127.0.0.1 8080 8 30000

# 第 5 个参数 newconn：每个请求都新建 TCP 连接（模拟短连接）
./bin/bench.exe 127.0.0.1 8080 8 2000 newconn
```

> **怎么看分位延迟**：P95 = 0.06ms 表示 95% 的请求延迟不超过 0.06ms。
> 平均值容易被少数极端值带偏，高并发下更应关注 P95/P99。

在本机（release，8 工作线程，请求 `GET /hello`）测得的一组参考数据：

**Keep-Alive 长连接**（连接被复用，省掉了握手开销）：

| 模型 | QPS | 平均延迟 | P99 |
|---|---:|---:|---:|
| iterative | ~3.1 万 | 0.233 ms | 0.057 ms |
| thread | ~14.5 万 | 0.053 ms | 0.094 ms |
| pool | ~14.4 万 | 0.055 ms | 0.103 ms |
| **iocp** | **~22.5 万** | **0.035 ms** | **0.073 ms** |

**短连接**（每个请求都新建/关闭 TCP，压力在 accept 与连接管理上）：

| 模型 | QPS | 平均延迟 |
|---|---:|---:|
| iterative | ~700 | 10.8 ms |
| thread | ~855 | 8.5 ms |
| pool | ~400 | 18.7 ms |
| **iocp** | **~1.76 万** | **0.10 ms** |

---

## 单元测试

[tests/mini_test.h](tests/mini_test.h)

没有用GTEST，仿着GTEST的语法的一个单元测试框架。


```bash
cd build_debug
make test_parser     # 只编译这一个测试
./bin/test_parser.exe
make                 # 编译全部
ctest                # 运行全部测试
```