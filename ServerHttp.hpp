#pragma once

#include "ServerBase.hpp"

namespace WebServer
{
	typedef boost::asio::ip::tcp::socket HTTP;
	template<>
	class Server<HTTP> : public ServerBase<HTTP>
	{
	public:
		// 通过端口号,　线程数来构造 Web 服务器, HTTP　服务器比较简单，不需要做相关配置文件的初始化
		Server(unsigned short port, size_t num_threads = 1) :
			ServerBase<HTTP>::ServerBase(port, num_threads) {};
		~Server(){};
	private:
		// 实现 accept() 方法
		void accept()
		{
			// 为当前连接创建一个新的 socket
			// Shared_ptr 用于传递临时对象给匿名函数
			// socket 会被推导为 std::shared_ptr<HTTP> 类型
			auto socket = std::make_shared<HTTP>(m_io_service);

			acceptor.async_accept(*socket, [this, socket](const boost::system::error_code & ec)
					{
						// 立即启动并接受一个连接
						accept();
						// 如果出现错误
						if(!ec) process_request_and_respond(socket);
					});
		}
	};
}
