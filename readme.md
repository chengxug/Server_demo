# Server Demo

此项目是一个个人学习项目，旨在从基本的多线程服务器开始，一步步完善，直到实现一个现代的服务器。

## 服务器目前已实现的功能

1. 监听在指定端口，返回给客户端它所收到的消息并关闭连接。

## Install

```sh
git clone https://github.com/chengxug/Server_demo.git
cd Server_demo
make
```
## 使用示例

启动服务器，你会看到如下输出
```sh
./server 7788
Server started on port 7788
Press Enter to stop the server..
```
用telnet连接服务器，输入任意内容，你可以看到服务器返回你输入的内容并关闭连接
```sh
❯ telnet localhost 7788
Trying 127.0.0.1...
Connected to localhost.
Escape character is '^]'.
Hello # 用户输入
Hello # 服务器响应
Connection closed by foreign host.
```

## TODO List (优先级从高到低)

- [x] 引入spdlog, 添加日志打印
- [x] 添加Makefile
- [ ] 功能完善：不直接关闭连接，而是等客户端主动关闭或错误发生。
- [ ] 对socket封装，符合RAII设计思想
- [ ] 监听线程使用非阻塞的accept 替换 select
- [ ] 添加信号处理 Ctrl C
- [ ] 添加对http协议的解析
