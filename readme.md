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
cd build
./server 7788
Server started on port 7788
Press Enter to stop the server..
```

在浏览器访问server，你可以看到index.html中的内容被正确返回。

## TODO List (优先级从高到低)

- [x] 引入spdlog, 添加日志打印
- [x] 添加Makefile
- [x] 功能完善：不直接关闭连接，而是等客户端主动关闭或错误发生。
- [x] 添加对http协议的解析
- [x] 实现router，将http请求路由到对应的处理程序
- [x] 编写测试程序计算qps
- [x] 优化测试程序：使用条件变量控制所有工作线程准备好后一起开始
- [x] 对socket封装，依据RAII设计思想
- [ ] 监听线程使用非阻塞的accept 替换 select
- [x] 添加信号处理 Ctrl C
- [ ] 编写测试脚本tests/test.sh，在Makefile中调用以执行所有测试
- [ ] Makefile 支持 debug 和 release
- [ ] 参考[proxygen](https://github.com/facebook/proxygen)优化解析器和路由的设计
- [ ] 打印 std::function 包装的函数的名称，以调试使用