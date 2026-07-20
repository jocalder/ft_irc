#include "Server.hpp"

static bool	g_running = true;

void	signalHandler(int signum)
{
	void(signum);
	g_running = false;
}

Server::Server(int port, const std::string& password)
: _port(port), _serverSocket(-1), _password(password), _running(false)
{
	signal(SIGINT, signalHandler);
	setupSocket();
}

Server::~Server()
{
	for (std::map<int, Client*>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		delete it->second;
	_clients.clear();

	for (std::map<std::string, Channel*>::iterator it = _channels.begin(); it != _channels.end(); ++it)
		delete it->second;
	_channels.clear();

	if (_serverSocket >= 0)
		close(_serverSocket);
}

void	Server::setupSocket()
{
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket < 0)
	{
		std::cerr << "Error: Server socket creation failed." << std::endl;
		exit(1);
	}
	int	opt = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		std::cerr << "Error: setsockopt failed." << std::endl;
		close(_serverSocket);
		exit(1);
	}
	if (fcntl(_serverSocket, F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "Error: fcntl failed." << std::endl;
		close(_serverSocket);
		exit(1);
	}
	struct sockaddr_in	serverAddr;
	std::memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(_port);

	if (bind(_serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
	{
		std::cerr << "Error: bind failed on port " << _port << "." << std::endl;
		close(_serverSocket);
		exit(1);
	}
	if (listen(_serverSocket, 128) < 0)
	{
		std::cerr << "Error: listen failed." << std::endl;
		close(_serverSocket);
		exit(1);
	}
	std::cout << "Server listening on port: " << _port << "." << std::endl;
	
}