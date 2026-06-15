#include "../inc/Server.hpp"
#include <sys/socket.h> // socket(), setsockopt(), SO_REUSEADDR, bind(), listen(), SOMAXCONN, accept(), recv(), send()
#include <netinet/in.h> // struct sockaddr_in
#include <fcntl.h> // fcntl()
#include <unistd.h> // close()
#include <stdexcept> // std::runtime_error()
#include <iostream>
#include <sstream>
#include <cctype> // std::toupper

Server* Server::instance = NULL;

Server::Server(int port, char *password): _port(port), _password(password), _serverSocket(-1)
{
	// initialize the global server instance
	// create the server socket
	// make the socket non-blocking
	// set the reuse addresse flag
	// bind it to an address::port
	// start listening (marks the serverSocket as passive)
	// push back serverSocket to pollFds (poll())

	instance = this;
	_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_serverSocket < 0)
		throw std::runtime_error("socket() failed");

	int prior_flags = fcntl(_serverSocket, F_GETFL);
	if (fcntl(_serverSocket, F_SETFL, prior_flags | O_NONBLOCK) < 0)
	{
		(close(_serverSocket), _serverSocket = -1);
		throw std::runtime_error("fcntl() failed");
	}

	int option = 1;
	if (setsockopt(_serverSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0)
	{
		(close(_serverSocket), _serverSocket = -1);
		throw std::runtime_error("setsockopt() failed");
	}

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(_port);
	serverAddr.sin_addr.s_addr = INADDR_ANY; // all interfaces
	if (bind(_serverSocket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0)
	{
		(close(_serverSocket), _serverSocket = -1);
		throw std::runtime_error("bind() failed");
	}

	if (listen(_serverSocket, SOMAXCONN) < 0)
	{
		(close(_serverSocket), _serverSocket = -1);
		throw std::runtime_error("listen() failed");
	}

	struct pollfd serverPoll;
	serverPoll.fd = _serverSocket;
	serverPoll.events = POLLIN;
	serverPoll.revents = 0;
	_pollFds.push_back(serverPoll);
}

Server::~Server()
{
	stop();
}

void Server::run()
{
	// poll() to wait for events
	// loop over connected clients
	// handle events and commands

	while (instance)
	{
		if (poll(&_pollFds[0], _pollFds.size(), -1) < 0 && instance)
			throw std::runtime_error("poll() failed");

		for (size_t i = 0; i < _pollFds.size();)
		{
			if (_pollFds[i].fd == _serverSocket)
			{
				if (_pollFds[i].revents & POLLIN)
					_handleConnect();
				else if (_pollFds[i].revents & POLLERR)
					throw std::runtime_error("error on server socket (POLLERR)");
				i++;
			}
			else
			{
				bool erased = false;
        if (_pollFds[i].revents & POLLIN)
            _handleMessage(i, erased);
        if (!erased && (_pollFds[i].revents & (POLLHUP | POLLERR)))
        {
            _handleQuit(i, "error or hangup");
            erased = true;
        }
        if (!erased)
            i++;
			}
		}
	}
}

void Server::stop()
{
	if (instance == NULL) // if signal, stop() was already called, destructor calls it again
		return;
	// check if signal was received before constructor could add serverSocket to pollFds
	if (_pollFds.empty() && _serverSocket != -1)
		(close(_serverSocket), _serverSocket = -1);
	else // all fds, including serverSocket, are in pollFds
	{
		(close(_pollFds[0].fd), _pollFds.erase(_pollFds.begin())); //_serverSocket
		while (!_pollFds.empty())
			_handleQuit(0, "server stopping");
	}
	instance = NULL;
}

void Server::_handleConnect()
{
	// accept the request and get fd
	// set fd to non-blocking
	// append fd to pollFds
	// create new User object in _Users map

	int clientSocket = accept(_serverSocket, NULL, NULL);
	if (clientSocket < 0)
	{
		std::cerr << "accept() failed"; // throw or just display error message?
		return;
	}

	int prior_flags = fcntl(clientSocket, F_GETFL);
	if (fcntl(clientSocket, F_SETFL, prior_flags | O_NONBLOCK) < 0) 
	{
		(close(clientSocket), clientSocket = -1);
		throw std::runtime_error("fcntl() failed");
	}

	struct pollfd clientPoll;
	clientPoll.fd = clientSocket;
	clientPoll.events = POLLIN;
	clientPoll.revents = 0;
	_pollFds.push_back(clientPoll);

	_Users[clientPoll.fd] = User();
	std::cout << "New client (FD = " << clientPoll.fd << ") connected to server" << std::endl;
}

void Server::_handleQuit(size_t i, std::string msg)
{
	// notify any user that shares a channel with quitting user
	// remove user from all channels he has joined
	// erase user object indexed by pollFds[i].fd from Users
	// close and remove the fd from pollFds

	int fd = _pollFds[i].fd;
  User& user = _Users[fd];

	//if (user._registered)
	//{
		std::string quitMsg = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " QUIT :" + msg + "\r\n";
		for (std::map<int, User>::iterator it = _Users.begin(); it != _Users.end(); ++it)
		{
			int userFd = it->first;
			if (userFd == fd || _shareChannels(fd, userFd))
			{
				size_t poll_index = _getPollIndexByFd(userFd);
				_sendMessage(poll_index, quitMsg);
			}
		}

  	for (std::set<std::string>::iterator it = user._joinedChannels.begin(); it != user._joinedChannels.end(); ++it)
  	{
    	std::string chanName = *it;

   		if (_Channels.find(chanName) != _Channels.end()) // EDGE CASE: user kicked from channel, channel then closed, user still has channel name in joinedChannels
    	{
      	Channel& chan = _Channels[chanName];

      	chan._memberFds.erase(fd); // user might not be in channel (that still exists) if kicked earlier, but its ok
      	chan._operatorFds.erase(fd);
      	chan._invitedFds.erase(fd);

      	if (chan._memberFds.empty())
        	_Channels.erase(chanName);
    	}
    }
 // }

	_Users.erase(fd);
	std::cout << "Client (FD = " << fd << ") disconnected from server" << std::endl;

	(close(fd), fd = -1);
	_pollFds.erase(_pollFds.begin() + i);
}

bool Server::_shareChannels(int userFd1, int userFd2)
{
	User& user1 = _Users[userFd1];
	User& user2 = _Users[userFd2];

	for (std::set<std::string>::iterator it = user1._joinedChannels.begin(); it != user1._joinedChannels.end(); ++it)
	{
		std::string chanName = *it;
		if (_Channels.find(chanName) != _Channels.end()) // check if channel still exists
		{
			// check if this channel is in user2's joined channels
			if (user2._joinedChannels.find(chanName) != user2._joinedChannels.end())
				return true;
		}
	}
	return false;
}

void Server::_handleMessage(size_t i, bool &erased)
{
	// receive message from client
	// extract commands from client's buffer (chunked requests)
	// client can end message with \n or \r\n
	// call command parser

	char buf[513];
	ssize_t size = recv(_pollFds[i].fd, buf, 512, 0);
	if (size < 0)
		throw std::runtime_error("recv() failed");
	else if (size == 0)
	{
		_handleQuit(i, "eof or hangup");
		erased = true;
	}
	else
	{
		buf[size] = 0;
		std::string& buffer = _Users[_pollFds[i].fd]._buffer;
		buffer.append(buf);

		size_t end = 0;
		while (!erased && (end = buffer.find("\n", 0)) != std::string::npos)
		{
			std::string command = buffer.substr(0, end);
			if (!command.empty() && command[command.size() - 1] == '\r')
				command.erase(command.size() - 1);
			buffer.erase(0, end + 1);
			if (!command.empty())
				_parseCommand(i, command, erased);
		}
	}
}

void Server::_parseCommand(size_t i, std::string& command, bool& erased)
{
	// parse command into tokens (COMMAND parameter list)
	// call appropriate authentication / commad handler
	
	/* debug */
	std::cout << "'" << command << "'" << std::endl;
	/* debug */
	
	std::vector<std::string>	tokens;
	size_t										colonPos = command.find(":");
	std::string								beforeColon = command;
	std::string								afterColon = "";

	if (colonPos != std::string::npos)
	{
		beforeColon = command.substr(0, colonPos);
		afterColon = command.substr(colonPos + 1);
	}
	std::stringstream	ss(beforeColon);
	std::string				token;
	while (ss >> token)
		tokens.push_back(token);
	if (colonPos != std::string::npos)
		tokens.push_back(afterColon);

	if (tokens.empty())
		return;
	std::string cmd = tokens[0];
	tokens.erase(tokens.begin());

	if (cmd == "CAP") {_handleCapabilityNegotiation(i, tokens); return;}
	else if (cmd == "PASS") {_handlePass(i, tokens, erased); return;}
	else if (cmd == "NICK") {_handleNick(i, tokens, erased); return;}
	else if (cmd == "USER") {_handleUser(i, tokens, erased); return;}

	if (!_Users[_pollFds[i].fd]._registered &&
		(cmd == "JOIN" || cmd == "PART" || cmd == "MODE" || cmd == "TOPIC"
		|| cmd == "INVITE" || cmd == "KICK" || cmd == "PRIVMSG"))
	{
	 	_sendMessage(i, ":localhost.ircserver 451 * :not registered\r\n");
		return;
	}

	if (cmd == "JOIN") _handleJoin(i, tokens);
	else if (cmd == "PART") _handlePart(i, tokens);
	else if (cmd == "MODE") _handleMode(i, tokens);
	else if (cmd == "TOPIC") _handleTopic(i, tokens);
	else if (cmd == "INVITE") _handleInvite(i, tokens);
	else if (cmd == "KICK") _handleKick(i, tokens);
	else if (cmd == "PRIVMSG") _handlePrivmsg(i, tokens);
	else if (cmd == "QUIT") {_handleQuit(i, tokens.size() > 0? tokens[0]: "error"); erased = true;}
	// any other command is ignored
}

void Server::_sendMessage(size_t i, std::string message)
{
	if (send(_pollFds[i].fd, message.c_str(), message.size(), 0) < 0)
		throw std::runtime_error("send() failed");
}

std::string Server::_tolowerStr(std::string str)
{
  for (size_t i = 0; i < str.size(); i++)
    str[i] = std::tolower(static_cast<unsigned char>(str[i]));
  return str;
}

size_t Server::_getUserByNick(std::string nickName) // IRC is case-insensitive; returns pollFds index
{
	nickName = _tolowerStr(nickName);
	for (size_t i = 1; i < _pollFds.size(); i++)
	{
		std::string i_nickName = _tolowerStr(_Users[_pollFds[i].fd]._nickName);
		if (i_nickName == nickName)
			return i;
	}
	return 0;
}

size_t Server::_getPollIndexByFd(int fd)
{
  for (size_t p = 0; p < _pollFds.size(); p++)
  {
    if (_pollFds[p].fd == fd)
      return p;
  }
  return 0;
}

void Server::_broadcastToChannel(std::string chanName, std::string message, int excludeFd)
{
	// check if channel exists (to be safe hhh)
	// loop over channel members
	// get pollIndex from fd and send message

	if (_Channels.find(chanName) == _Channels.end())
    return;

	// normally every method that calls broadcast to channel must send a lowercase name
	// but one can never be too safe
	chanName = _tolowerStr(chanName);

  Channel& chan = _Channels[chanName];
  for (std::set<int>::iterator it = chan._memberFds.begin(); it != chan._memberFds.end(); ++it)
  {
    int memberFd = *it;
    if (memberFd == excludeFd)
      continue;
    size_t pollIndex = _getPollIndexByFd(memberFd);
    _sendMessage(pollIndex, message);
  }
}
