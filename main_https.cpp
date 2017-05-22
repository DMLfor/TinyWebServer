#include "ServerHttps.hpp"

#include "Handler.hpp"
using namespace WebServer;
int main()
{
	Server<HTTPS> server(23333, 4, "server.crt", "server.key");
	start_server<Server<HTTPS>>(server);
	return 0;
}
