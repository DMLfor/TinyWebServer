#include "ServerHttp.hpp"
#include "Handler.hpp"

using namespace WebServer;

int main()
{
	Server<HTTP> server(23333, 4);
	start_server<Server<HTTP>>(server);
	return 0;
}
