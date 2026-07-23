#include "Server.hpp"
#include "Utils.hpp"

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
	if (listen(_serverSocket, SOMAXCONN) < 0)
	{
		std::cerr << "Error: listen failed." << std::endl;
		close(_serverSocket);
		exit(1);
	}
	std::cout << "Server listening on port: " << _port << "." << std::endl;
	
	struct pollfd	serverpfd;
	serverpfd.fd = _serverSocket;
	serverpfd.events = POLLIN;
	serverpfd.revents = 0;
	_pollFds.push_back(serverpfd);
}

void	Server::run()
{
	_running = true;
	g_running = true;

	std::cout << "Server running: press Ctrl+C to stop" << std::endl;
	while (_running && g_running)
	{
		int	tmp = poll(&_pollFds[0], _pollFds.size(), -1);
		if (tmp < 0)
		{
			if (errno == EINTR)
				continue;
			std::cout << "Error: poll failed." << std::endl;
			break;
		}
		for (size_t i = 0; i < _pollFds.size(); ++i)
		{
			if (_pollFds[i].revents == 0)
				continue;
			if (_pollFds[i].revents & POLLIN)
			{
				if (_pollFds[i].fd == _serverSocket)
					handleNewConnection();
				else
					handleClientData(_pollFds[i].fd);
			}
			if (_pollFds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
			{
				if (_pollFds[i].fd != _serverSocket)
					removeClient(_pollFds[i].fd);
			}
		}
	}
	std::cout << "Server stopped." << std::endl;
}

void	Server::handleNewConnection()
{
	struct	sockaddr_in	clientAddr;
	socklen_t	addrLen = sizeof(clientAddr);

	int	clientFd = accept(_serverSocket, (struct sockaddr*)&clientAddr, &addrLen);
	if (clientFd < 0)
	{
		std::cerr << "Error: accept failed." << std::endl;
		return;
	}
	if (fcntl(clientFd, F_SETFL, O_NONBLOCK) < 0)
	{
		std::cerr << "Error: fcntl failed.";
		close(clientFd);
		return;
	}
	Client* client = new Client(clientFd, clientAddr);
	_clients[clientFd] = client;

	struct pollfd	pfd;
	pfd.fd = clientFd;
	pfd.events = POLLIN;
	pfd.revents = 0;
	_pollFds.push_back(pfd);
	
	std::cout << "New client connected: fd = " << clientFd << ", ip=" <<
	inet_ntoa(clientAddr.sin_addr) << std::endl;
}

void	Server::handleClientData(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	Client* client = it->second;
	char buffer[512];
	std::memset(buffer, 0, sizeof(buffer));

	ssize_t bytesRead = recv(fd, buffer, sizeof(buffer) - 1, 0);
	if (bytesRead <= 0)
	{
		if (bytesRead == 0)
			std::cerr << "Client fd: " << fd << " disconnected." << std::endl;
		else
			std::cerr << "Error: recv failed of fd " << fd << "." << std::endl;
		removeClient(fd);
		return; 
	}

	client->appendtobuffer(std::string(buffer, bytesRead));
	while (client->hasCompleteMessage())
	{
		std::string line = client->extractMessage();
		if (!line.empty())
			processCommand(client, line);
	}
}

void	Server::removeClient(int fd)
{
	std::map<int, Client*>::iterator it = _clients.find(fd);
	if (it == _clients.end())
		return;
	Client* client = it->second;

	std::vector<Channel*> channels = client->getChannels();
	for (std::vector<Channel*>::iterator channelIt = channels.begin();
		channelIt != channels.end(); ++channelIt)
	{
		Channel* channel = *channelIt;
		std::string partMsg = client->getPrefix() + " PART " + channel->getName()
			+ "\r\n";
		channel->broadcast(partMsg, NULL);
		channel->removeClient(client);
		client->leaveChannel(channel);
		if (channel->isEmpty())
			deleteChannel(channel->getName());
	}
	close(fd);
	delete client;
	_clients.erase(it);

	for (std::vector<struct pollfd>::iterator pollIt = _pollFds.begin();
		pollIt != _pollFds.end(); ++pollIt)
	{
		if (pollIt->fd == fd)
		{
			_pollFds.erase(pollIt);
			break;
		}
	}
	std::cout << "Client fd: " << fd << " removed." << std::endl;
}