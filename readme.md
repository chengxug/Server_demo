# Server Demo

此项目是一个个人学习项目，旨在从一个最基本的迭代服务器，完善到现代的服务器模型，以探究在现代服务器中，所使用的各种技术，是为了解决什么问题，定量的探究所用技术带来的性能提升。

## 起因

此项目起源于我之前学习实现的一个httpServer, 在做它的过程中，我了解和使用了包括 Reactor、Epoll在内的很多设计和技术，但是对为什么这么做，这么性能上有多大的提升还是有些模糊。

就我个人的视角，计算机是一门理论和实践结合的工程学科。我们现在所学习的技术大多是计算机在发展过程中遇到的现实问题的解决方案。要明白为什么要这么做，首先需要了解这么做是为了解决什么问题。

所以，诞生了这个想法，以总结历史的视角，看看现在所用的技术是为了解决什么问题，带来的性能提升有多大。

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

## Third-Party Code

本项目使用了以下开源仓库的代码。

- [spdlog](https://github.com/gabime/spdlog)  
  A fast C++ logging library.  
  License: MIT

- [ThreadPool](https://github.com/progschj/ThreadPool)  
  A simple C++11 thread pool implementation.  
  License: Zlib
