#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <poll.h>
#include <map>
//#include "Client.hpp"
//#include "Channel.hpp"

class Client;
class Channel;

class Server
{
	private:
		int								_port;
		int								_serverSocket;
		std::string						_password;
		bool							_running;
		std::vector<struct pollfds>		_pollFds;
		std::map<int, Client*>			_clients;
		std::map<std::string, Channel*>	_channels;

		void	setupSocket();
		void	handleNewClient();
		void	handleNewConnection();
		void	handleClientData(int fd);
		void	removeClient(int fd);
		void	processCommand(Client* client, std::string& line);

		void	cmdPass(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdNick(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdUser(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdJoin(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdQuit(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdPart(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdTopic(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdNames(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdInvite(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdPrivmsg(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdMode(Client* client, const std::vector<std::string>& params, const std::string& trailing);
		void	cmdPing(Client* client, const std::vector<std::string>& params, const std::string& trailing);

		Client* 	findClientBynick(const std::string& nick) const;
		Channel*	findChannel(const std::string& name) const;
		Channel*	createChannel(const std::string& name, Client* creator);

		void	deleteChannel(const std::string& name);
		void	sendWelcome(Client* client) const;
		bool	isNickNameTaken(const std::string& name) const;
		void	broadcastToChannel(Channel* channel, const std::string& msg, Client* exclude) const;
	public:
		Server(int port, const std::string &password);
		~Server();

		void	run();
};



#endif