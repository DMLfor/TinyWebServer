# TinyWebServer
一个简易的web服务器

## 基于boost::asio的Web服务器

１．支持HTTP，HTTPS协议．HTTPS协议用asio::ssl::context对象对socket数据流进行加密．
２．支持静态的HTML页面，GET/POST方法,　可以方便的进行横向扩展以及支持新的请求方法．
３．将服务端的请求和响应部分的逻辑抽象出来形成框架，提高可复用性

## OS

Linux

## Complier

G++5及以上

## Library dependencies

>> boost\_system ssl pthread boost\_regex boost\_thread openssl

开发环境的Boost版本为boost.1.64
openssl 用于创建证书文件
## Explain 
１．start()方法，对将资源添加进来，默认资源放置到最后面，同时启用线程，线程起动后需要让线程各自等待，直到整个请求结束

２．process\_request\_and\_respond()方法，通过read\_buffer得到istream对象后,用parse\_request方法通过正则表达式将信息解析并保存到request中，处理后，用respond()方法处理应答.

３．在Boost中, HTTP类型就是普通的socket类型(boost::asio::ip::tcp::socket).

# DEMO
[demo](http://118.89.145.94:23333)

 
