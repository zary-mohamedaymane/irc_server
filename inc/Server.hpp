#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <map>
#include <poll.h> // struct pollfd
#include <cstdlib>  // added by yhajbi
#include <sstream>  // added by yhajbi

#include "User.hpp"
#include "Channel.hpp"

class Server
{
	private:
		int													_port;
		std::string									_password;
		int													_serverSocket;
		std::vector<struct pollfd>	_pollFds;

		std::map<int, User>							_Users; // indexed by pollFds.fd
		std::map<std::string, Channel>	_Channels; // indexed by channel name (lowercased)

	public:
		Server(int port, char* password);
		~Server();

		static Server*	instance;
		void 						run();
		void 						stop(); // cannot re-run() after stop()

		void						_handleConnect();
		void						_handleQuit(size_t poll_index, std::string msg);
		bool						_shareChannels(int userFd1, int userFd2);
		void						_handleMessage(size_t poll_index, bool& erased);

		void						_parseCommand(size_t poll_index, std::string& command, bool& erased);
		void						_sendMessage(size_t poll_index, std::string message);

		std::string			_tolowerStr(std::string str);
		size_t					_getPollIndexByNick(std::string nickName); // returns user poll_index or 0 if not found
		size_t					_getPollIndexByFd(int fd);

		void 						_broadcastToChannel(std::string chanName, std::string message, int excludeFd);

		void						_handlePass(size_t poll_index, std::vector<std::string>& tokens, bool& erased);
		void						_handleNick(size_t poll_index, std::vector<std::string>& tokens, bool& erased);
		void						_handleUser(size_t poll_index, std::vector<std::string>& tokens, bool& erased);

		void						_handleJoin(size_t poll_index, std::vector<std::string>& tokens);
		void						_handlePart(size_t poll_index, std::vector<std::string>& tokens);
		void						_handlePrivmsg(size_t poll_index, std::vector<std::string>& tokens);

		/* ---------------------------------- YOUSSEF --------------------------------------------- */
		void						_handleMode(size_t poll_index, std::vector<std::string>& tokens);
		void						_handleTopic(size_t poll_index, std::vector<std::string>& tokens);
		void						_handleInvite(size_t poll_index, std::vector<std::string>& tokens);
		void						_handleKick(size_t poll_index, std::vector<std::string>& tokens);

		bool						_lookupChannel(std::string channelName);
		bool						_lookupSender(int userFd, std::string channelName);
		bool						_lookupSenderPrivilege(int userFd, std::string channelName);
		void						_removeUserFromChannel(int userFd, std::string channelName);
		/* ---------------------------------------------------------------------------------------- */
};

#endif
