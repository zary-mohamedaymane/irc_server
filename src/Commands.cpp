#include "../inc/Server.hpp"
#include <sstream>
#include <iostream>

void Server::_handleCapabilityNegotiation(size_t i, std::vector<std::string>& tokens)
{
	User& user = _Users[_pollFds[i].fd];

	if (tokens[0] == "LS")
		_sendMessage(i, ":localhost.ircserver CAP * LS :\r\n");
	else if (tokens[0] == "END")
	{
		if (user._registered)
		{
			_sendMessage(i, ":localhost.ircserver 001 " + user._nickName + " :Welcome to the 42 Internet Relay Network\
				" + user._nickName + "!" + user._userName + "@" + user._hostName + "\r\n");
			_sendMessage(i, ":localhost.ircserver 002 " + user._nickName + " :Your host is localhost, running version 1.0\r\n");
			_sendMessage(i, ":localhost.ircserver 003 " + user._nickName + " :This Sever was created today\r\n");
			_sendMessage(i, ":localhost.ircserver 004 " + user._nickName + " localhost.ircserver 1.0 - itkol\r\n");
		}
	}
}

void Server::_handlePass(size_t i, std::vector<std::string>& tokens, bool& erased)
{
	// 461 = ERR_NEEDMOREPARAMS
	// 462 = ERR_ALREADYREFISTRED
	// 464 = ERR_PASSWDMISMATCH

	User& user = _Users[_pollFds[i].fd];

	if (tokens.empty())
	{
		_sendMessage(i, ":localhost.ircserver 461 * PASS :Not enough parameters\r\n");
		(_handleQuit(i, "auth error"), erased = true);
	}
	else if (user._registered)
	{
		_sendMessage(i, ":localhost.ircserver 462 " + user._nickName + " :Unauthorized command (already registered)\r\n");
		(_handleQuit(i, "auth error"), erased = true);
	}
	else if (tokens[0] != _password)
	{
		_sendMessage(i, ":localhost.ircserver 464 * :Password incorrect\r\n");
		(_handleQuit(i, "auth error"), erased = true);
	}
	else
		user._authenticated = true;
}

void Server::_handleNick(size_t i, std::vector<std::string>& tokens, bool& erased)
{
	// 431 = ERR_NONICKNAMEGIVEN
	// 432 = ERR_ERRONEUSNICKNAME
	// 433 = ERR_NICKNAMEINUSE
	

	User& user = _Users[_pollFds[i].fd];
	size_t j;

	if (tokens.empty())
		_sendMessage(i, ":localhost.ircserver 431 * :No nickname given\r\n");
	else if (!user._authenticated)
	{
		_sendMessage(i, ":localhost.ircserver 464 * :Password required\r\n");
		(_handleQuit(i, "auth error"), erased = true);
	}
	else if (!User::_validNickName(tokens[0]))
		_sendMessage(i, ":localhost.ircserver 432 * " + tokens[0] + " :Erroneous nickname\r\n");
	else if ((j = _getUserByNick(tokens[0])) != 0 && i != j)
		_sendMessage(i, ":localhost 433 * " + tokens[0] + " :Nickname is already in use\r\n");
	else
	{
		if (user._registered) // nickname change, potentially same nickname
		{
			// TODO: broadcast msg to all channels in which user is a member
			// ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " NICK :" + tokens[0] + "\r\n"
		}
		user._nickName = tokens[0];
	}
}

void Server::_handleUser(size_t i, std::vector<std::string>& tokens, bool& erased)
{
	// 461 = ERR_NEEDMOREPARAMS
	// 462 = ERR_ALREADYREGISTRED

	User& user = _Users[_pollFds[i].fd];
	
	if (tokens.size() < 4)
		_sendMessage(i, ":localhost.ircserver 461 " + user._nickName + " USER :Not enough parameters\r\n");
	else if (!user._authenticated)
	{
		_sendMessage(i, ":localhost.ircserver 464 * :Password required\r\n");
		(_handleQuit(i, "auth error"), erased = true);
	}
	else if (user._registered)
		_sendMessage(i, ":localhost.ircserver 462 " + user._nickName + " :Unauthorized command (already registered)\r\n");
	else
	{
		user._userName = tokens[0];
		user._hostName = tokens[1];
		user._realName = tokens[3];
		user._registered = true;
	}
}

static std::vector<std::string> splitByComma(std::string str)
{
    std::vector<std::string> res;
    std::string token;
    std::stringstream ss(str);
    while (std::getline(ss, token, ','))
    {
      if (!token.empty())
        res.push_back(token);
    }
    return res;
}

void Server::_handleJoin(size_t i, std::vector<std::string>& tokens)
{
	// extracts the channel names (first argument) and the channel keys (second argument) from tokens
	// for each channel name/key pair:
	// 1. validate channel name syntax
	// 2. check channel existence
	// 2.1 channel doesnt exist -> create it (CHECK GRAMMAR!!!!) and add user to operators, then add channel to server
	// 2.2 channel exists -> check double-join? -> invite-only? -> key correct? -> channel full? -> add user fd to channel
	// 3. add channel name to user's joinedChannels
	// 4. broadcast to channel that the new user joined
	// 5. send success message to client: TOPIC + NAMEREPLY (RFC Join)

	int userFd = _pollFds[i].fd;
	User& user = _Users[userFd];

	if (tokens.empty())
	{
		_sendMessage(i, ":localhost.ircserver 461 " + user._nickName + " JOIN :Not enough parameters\r\n");
		return;
	}
	std::vector<std::string> chanList = splitByComma(tokens[0]);
	std::vector<std::string> keyList;
	if (tokens.size() > 1)
		keyList = splitByComma(tokens[1]);

	for (size_t x = 0; x < chanList.size(); x++)
	{
		std::string chanName = chanList[x];
		std::string	lowerChanName = _tolowerStr(chanName);
		std::string providedKey = (x < keyList.size()) ? keyList[x]: "";

		if (!(chanName[0] == '#' || chanName[0] == '&')) // check RFC for grammar
    {
      _sendMessage(i, ":localhost.ircserver 403 " + user._nickName + " " + chanName + " :No such channel\r\n");
      continue;
    }

		bool chanExists = (_Channels.find(lowerChanName) != _Channels.end());

		if (!chanExists)
		{
			// YOU HAVE TO PARSE CHANNEL NAME AND CHECK IF IT IS VALID!
			Channel newChan;
			newChan._name = chanName;
			newChan._memberFds.insert(userFd);
			newChan._operatorFds.insert(userFd);

			_Channels[lowerChanName] = newChan;
		}
		else
		{
			Channel& chan = _Channels[lowerChanName];

			if (chan._memberFds.find(userFd) != chan._memberFds.end())
				continue; // double-join

			if (chan._inviteOnly && chan._invitedFds.find(userFd) == chan._invitedFds.end())
			{
				_sendMessage(i, ":localhost.ircserver 473 " + user._nickName + " " + chanName + " :Cannot join channel (+i)\r\n");
        continue;
			}

			if (!chan._key.empty() && providedKey != chan._key)
			{
				_sendMessage(i, ":localhost.ircserver 475 " + user._nickName + " " + chanName + " :Cannot join channel (+k)\r\n");
        continue;
			}

			if (chan._userLimit > 0 && chan._memberFds.size() >= chan._userLimit)
			{
				_sendMessage(i, ":localhost.ircserver 471 " + user._nickName + " " + chanName + " :Cannot join channel (+l)\r\n");
        continue;
			}

			chan._memberFds.insert(userFd);
		}

		user._joinedChannels.insert(lowerChanName);

		std::string joinMsg = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " JOIN :" + chanName + "\r\n";
    _broadcastToChannel(lowerChanName, joinMsg, -1);

		Channel& chan = _Channels[chanName];
		if (chan._topic.empty())
      _sendMessage(i, ":localhost.ircserver 331 " + user._nickName + " " + chanName + " :No topic is set\r\n");
    else
      _sendMessage(i, ":localhost.ircserver 332 " + user._nickName + " " + chanName + " :" + chan._topic + "\r\n");

		std::string nameList = "";
    for (std::set<int>::iterator mit = chan._memberFds.begin(); mit != chan._memberFds.end(); ++mit)
    {
      int fd = *mit;
      if (chan._operatorFds.find(fd) != chan._operatorFds.end())
        nameList += "@" + _Users[fd]._nickName + " ";
      else
        nameList += _Users[fd]._nickName + " ";
    }
    if (!nameList.empty() && nameList[nameList.size() - 1] == ' ')
      nameList.erase(nameList.size() - 1);

    _sendMessage(i, ":localhost.ircserver 353 " + user._nickName + " = " + chanName + " :" + nameList + "\r\n");
    _sendMessage(i, ":localhost.ircserver 366 " + user._nickName + " " + chanName + " :End of /NAMES list.\r\n");
	}
}

void Server::_handlePart(size_t i, std::vector<std::string>& tokens)
{
	// 1. check parameters, extract channel names
	// 2. for each channel
	// 2.1 channel exists?
	// 2.2 user in channel?
	// 2.3 remove user from channel and broadcast part message to channel + leaving user
	// 2.4 if channel is empty, remove it

	int userFd = _pollFds[i].fd;
	User& user = _Users[userFd];

	if (tokens.empty())
	{
		_sendMessage(i, ":localhost.ircserver 461 PART :Not enough parameters\r\n");
		return;
	}
	std::vector<std::string> chanList = splitByComma(tokens[0]);

	for (size_t x = 0; x < chanList.size(); x++)
	{
		std::string chanName = chanList[x];
		std::string	lowerChanName = _tolowerStr(chanName);

		bool chanExists = (_Channels.find(lowerChanName) != _Channels.end());
		if (!chanExists)
		{
			_sendMessage(i, ":localhost.ircserver 403 " + chanName + " :No such channel\r\n");
			continue;
		}

		Channel& chan = _Channels[lowerChanName];
		if (chan._memberFds.find(userFd) == chan._memberFds.end())
		{
			_sendMessage(i, ":localhost.ircserver 442 " + chanName + " :You're not on that channel\r\n");
			continue;
		}

		std::string partMsg = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " PART :" + chanName + "\r\n";
		_broadcastToChannel(lowerChanName, partMsg, -1);
		
		chan._memberFds.erase(userFd);
    chan._operatorFds.erase(userFd);
    chan._invitedFds.erase(userFd);

		if (chan._memberFds.empty())
        _Channels.erase(chanName);
	}
}

void Server::_handlePrivmsg(size_t i, std::vector<std::string>& tokens)
{
	// 1. check parameters, extract recipients and message
	// 2. loop over each recipient:
	// 2.1 target is channel -> check channel exists? -> user is channel member? -> send to members expect sender
	// 2.2 target is a user -> check user existence? -> send to user

  int userFd = _pollFds[i].fd;
  User& user = _Users[userFd];

  if (tokens.empty())
  {
    _sendMessage(i, ":localhost.ircserver 411 " + user._nickName + " :No recipient given (PRIVMSG)\r\n");
    return;
  }
  if (tokens.size() < 2 || tokens[1].empty())
  {
    _sendMessage(i, ":localhost.ircserver 412 " + user._nickName + " :No text to send\r\n");
    return;
  }

  std::vector<std::string> targets = splitByComma(tokens[0]);
  std::string messageText = tokens[1];

  for (size_t t = 0; t < targets.size(); t++)
  {
    std::string target = targets[t];
    std::string msgPrefix = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " PRIVMSG " + target + " :" + messageText + "\r\n";

    if (target[0] == '#' || target[0] == '&')
    {
      std::string lowerChanName = _tolowerStr(target);
      if (_Channels.find(lowerChanName) == _Channels.end())
      {
        _sendMessage(i, ":localhost.ircserver 401 " + user._nickName + " " + target + " :No such nick/channel\r\n");
        continue;
      }

      Channel& chan = _Channels[lowerChanName];
      
      if (chan._memberFds.find(userFd) == chan._memberFds.end())
      {
        _sendMessage(i, ":localhost.ircserver 404 " + user._nickName + " " + target + " :Cannot send to channel\r\n");
        continue;
      }
      _broadcastToChannel(lowerChanName, msgPrefix, userFd);
    }
    else
    {
      size_t targetIndex = _getUserByNick(target);
      if (targetIndex == 0)
      {
        _sendMessage(i, ":localhost.ircserver 401 " + user._nickName + " " + target + " :No such nick/channel\r\n");
        continue;
      }
      _sendMessage(targetIndex, msgPrefix);
    }
  }
}





/* ---------------------------------- YOUSSEF --------------------------------------------- */
bool	Server::_lookupChannel(std::string channelName) {

	if (_Channels.find(channelName) != _Channels.end())
		return (true);

	return (false);
}

bool	Server::_lookupSender(int userFd, std::string channelName) {

	Channel	targetChannel = _Channels.find(channelName)->second;

	if (targetChannel._memberFds.find(userFd) != targetChannel._memberFds.end())
		return (true);
	
	return (false);
}

bool	Server::_lookupSenderPrivilege(int userFd, std::string channelName) {

	Channel	targetChannel = _Channels.find(channelName)->second;

	if (targetChannel._operatorFds.find(userFd) != targetChannel._memberFds.end())
		return (true);
	return (false);
}

void	Server::_removeUserFromChannel(int userFd, std::string channelName) {

	Channel	targetChannel = _Channels.find(channelName)->second;

	/* need to refactor */

	targetChannel._memberFds.erase(targetChannel._memberFds.find(userFd));
	targetChannel._operatorFds.erase(targetChannel._operatorFds.find(userFd));
	targetChannel._invitedFds.erase(targetChannel._invitedFds.find(userFd));
}

void Server::_handleMode(size_t i, std::vector<std::string>& tokens) {(void)i; (void)tokens;}
void Server::_handleTopic(size_t i, std::vector<std::string>& tokens) {(void)i; (void)tokens;}
void Server::_handleInvite(size_t i, std::vector<std::string>& tokens) {(void)i; (void)tokens;}
void Server::_handleKick(size_t i, std::vector<std::string>& tokens) {

	if (tokens.size() < 2) {
		_sendMessage(i, ":localhost.ircserver 461 KICK :Not enough parameters\r\n");
		return ;
	}

	std::vector<std::string>	chanList = splitByComma(tokens[0]);
	std::string					targetUser = tokens[1];
/* 	std::vector<std::string>	userList = splitByComma(tokens[1]); */
	std::string					reason;
	if (tokens.size() > 2)
		reason = tokens[2];

	for (size_t j = 0; j < chanList.size(); j++) {
		if (!_lookupChannel(chanList[j])) {
			_sendMessage(i, ":localhost.ircserver 403 KICK :" + chanList[j] + " :No such channel\r\n");
			continue ;
		}
		if (!_lookupSender(i, chanList[i])) {
			_sendMessage(i, ":localhost.ircserver 442 KICK:" + chanList[j] + " :You're not on that channel\r\n");
			continue ;
		}
		if (!_lookupSenderPrivilege(i, chanList[i])) {
			_sendMessage(i, "localhost.ircserver 482 KICK:" + chanList[j] + " :You're not channel operator\r\n");
			continue ;
		}
		size_t	targetUserFd = _getUserByNick(targetUser);
		if (!targetUserFd) {
			_sendMessage(i, "localhost.ircserver 401 KICK:" + chanList[j] + " :No such nick\r\n");
			continue ;
		}
		if (!_lookupSender(targetUserFd, chanList[i])) {
			_sendMessage(i, "localhost.ircserver 441 KICK:" + chanList[j] + " :They aren't on that channel\r\n");
			continue ;
		}

		std::string	kickMsg = "KICK " + targetUser + " from " + chanList[j];
		if (tokens.size() > 2)
			kickMsg += " using\"" + reason + "\" as the reason (comment).";

		_broadcastToChannel(chanList[j], kickMsg, -1);

		_removeUserFromChannel(targetUserFd, chanList[j]);

		/* need to refactor */

		_Users.find(targetUserFd)->second._joinedChannels.erase(_Users.find(targetUserFd)->second._joinedChannels.find(chanList[j]));

		if (_Channels.find(chanList[j])->second._memberFds.empty())
			_Channels.erase(_Channels.find(chanList[j]));
	}

	/* std::cout << "channel list: " << std::endl;
	for (size_t i = 0; i < chanList.size(); i++) {
		std::cout << chanList[i] << std::endl;
	}

	std::cout << "user list: " << std::endl;
	for (size_t i = 0; i < userList.size(); i++) {
		std::cout << userList[i] << std::endl;
	}

	if (tokens.size() > 2)
		std::cout << tokens[2] << std::endl; */
}
/* ---------------------------------------------------------------------------------------- */
