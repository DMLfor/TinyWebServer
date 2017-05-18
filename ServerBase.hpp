#pragma once
#include <unordered_map>
#include <thread>
#include <regex>
#include <map>
#include <boost/asio.hpp>

namespace WebServer
{
	struct Request
	{
		// 请求方法，POST, GET; 请求路径; HTTP 版本
		std::string method, path, http_version;
		// 对content 使用智能指针进行引用计数
		// 请求的内容会包含在一个istream中
		std::shared_ptr<std::istream> content;
		// hash容器
		std::unordered_map<std::string, std::string> header;
		// 使用正则表达式处理路径匹配
		std::smatch path_match;
	};

	typedef std::map<std::string, std::unordered_map<std::string,
			std::function<void(std::ostream &, Request &)>>> resource_type;

	template <typename socket_type>
	class ServerBase
	{
	public:
		// 用于服务器访问资源处理方式
		resource_type resource;
		// 用于保存默认资源处理方式
		resource_type default_resource;

		ServerBase(unsigned short port, size_t num_threads = 1):
			endpoint(boost::asio::ip::tcp::v4(), port),
			acceptor(m_io_service, endpoint),
			num_threads(num_threads) {}
		
		
		virtual ~ServerBase();
		// 启动服务器
		void start();
	protected:
		// asio库中的io_service 是调度器,所有的异步IO事件都要通过它来分发处理
		// 需要IO的对象的构造函数,都需要传入一个io_service对象
		boost::asio::io_service m_io_service;
		// IP地址,端口号,协议版本号构成一个endpoint
		boost::asio::ip::tcp::endpoint endpoint;
		// tcp::acceptor 对象,并在指定端口上等待连接
		boost::asio::ip::tcp::acceptor acceptor;
		// 服务器线程
		size_t num_threads;
		std::vector<std::thread> threads;

		// 用于内部实现对所有资源的处理
		// 所有的资源及默认资源都会在vector尾部添加, 并在start() 中创建
		std::vector<resource_type::iterator> all_resources;
		// 需要不同的类型的服务器实现这个方法
		virtual void accept() {}
		// 处理请求和回答
		void process_request_and_respond(std::shared_ptr<socket_type> socket) const;
		Request parse_request(std::istream &stream) const;
		void respond(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) const;
	};

	template <typename socket_type>
	class Server : public ServerBase<socket_type> {};

}

template <typename socket_type>
void WebServer::ServerBase<socket_type>::start()
{
	// 默认资源回放在 vector 的末尾，用作默认应答
	// 默认的请求会在找不到匹配请求路径时, 进行访问,故在最后添加
	for(auto it = resource.begin(); it != resource.end(); it++)
		all_resources.push_back(it);

	for(auto it = default_resource.begin(); it != default_resource.end(); it++)
		all_resources.push_back(it);
	// 调用 socket 的连接方式，还需要子类来实现 accept() 逻辑
	accept();
	// 如果 num_threads>1, 那么 m_io_service.run()
	// 将运行 (num_threads-1) 线程成为线程池
	for(size_t c = 1; c<num_threads; c++)
	{
		threads.emplace_back([this]()
				{
					m_io_service.run();
				});
	}
	// 主线程
	m_io_service.run();

	// 等待其他线程, 如果有的话，就等待这些线程的结束
	for(auto &t :threads)
		t.join();
}

template <typename socket_type>
void WebServer::ServerBase<socket_type>::process_request_and_respond(std::shared_ptr<socket_type> socket) const
{
	// 为 async_read_untile() 创建新的读缓存
	// shared_ptr 用于传递临时对象给匿名函数
	// 会被推导为 std::shared_ptr<boost::asio::streambuf>
	auto read_buffer = std::make_shared<boost::asio::streambuf>();

	boost::asio::async_read_until(*socket, *read_buffer, "\r\n\r\n",
	[this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if(!ec)
		{
			// 注意: read_buffer->size() 的大小一定是和 bytes_transferred 相等, Boost的文档中指出:
			// 在 async_read_until 操作成功后, streambuf 在界定符之外可能包含额外的数据
			// 所以较好的做法是直接从流中提取并且解析当前 read_buffer 左边的报头，在拼接async_read_untile
			// 后面的内容
			size_t total = read_buffer->size();
			// 转换到 istream
			std::istream stream(read_buffer.get());
			// 被推导为 std::shared_ptr<Request> 类型
			auto request = std::make_shared<Request>();
			*request = parse_request(stream);

			size_t num_additional_bytes = total - bytes_transferred;

			// 接下来要将 stream 中的请求信息进行解析, 然后保存到 request 对象中
			// 如果满足，同样读取

			if(request->header.count("Content-Length")>0)
			{
				boost::asio::async_read(*socket, *read_buffer,
				boost::asio::transfer_exactly(stoull(request->header["Content-Length"]) - num_additional_bytes),
				[this, socket, read_buffer, request](const boost::system::error_code &ec, size_t bytes_transferred)
				{
					if(!ec)
					{
						// 将指针作为 istream 对象存储到 read_buffer 中
						request->content = std::shared_ptr<std::istream>(new std::istream(read_buffer.get()));
						respond(socket, request);
					}
				});
			}
			else
			{
				respond(socket, request);
			}
		}

	});
}

template <typename socket_type>
WebServer::Request WebServer::ServerBase<socket_type>::parse_request(std::istream &stream) const
{
	Request request;
	// 使用正则表达式对请求报头进行解析
	// 可以解析出请求方法(GET/POST),请求路径以及 HTTP 版本
	std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

	std::smatch sub_match;
	// 从第一行中解析请求的方法, 路径和 HTTP 版本
	std::string line;
	getline(stream, line);
	line.pop_back();
	if(std::regex_match(line, sub_match, e))
	{
		request.method        = sub_match[1];
		request.path		  = sub_match[2];
		request.http_version  = sub_match[3];
		// 解析头部的其他信息
		bool matched = false;
		e = "^([^:]*): ?(.*)$";
		do
		{
			getline(stream, line);
			line.pop_back();
			matched = std::regex_match(line, sub_match, e);
			if(matched)
			{
				request.header[sub_match[1]] = sub_match[2];
			}
		} while(matched);
	}
	return request;
}

template <typename socket_type>
void WebServer::ServerBase<socket_type>::respond(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) const
{
	for(auto res_it : all_resources)
	{
		// 对请求路径和方法进行匹配查找, 并生成响应
		std::regex e(res_it->first);
		std::smatch sm_res;
		if(std::regex_match(request->path, sm_res, e))
		{
			if(res_it->second.count(request->method) > 0)
				request->path_match = move(sm_res);
			// 会被推导为 std::shared_ptr<boost::asio::streambuf>
			auto write_buffer = std::make_shared<boost::asio::streambuf>();
			std::ostream respondse(write_buffer.get());
			res_it->second[request->method](respondse, *request);

			// 在 lambda 中不糊 write_buffer 使其不会再 async_write 完成前被销毁
			boost::asio::async_write(*socket, *write_buffer,
					[this, socket, request, write_buffer](const boost::system::error_code &ec, size_t bytes_transferred)
					{
						// HTTP 持久连接(HTTP 1.1), 递归调用
						if(!ec && stof(request->http_version) > 1.05)
							process_request_and_respond(socket);
					});
			return ;
		}
	}
}

template <typename socket_type>
WebServer::ServerBase<socket_type>::~ServerBase()
{

}

