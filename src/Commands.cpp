#include "../inc/Server.hpp"
#include <iostream>
#include <sstream>
#include <string>

void Server::_handlePass(size_t i, std::vector<std::string> &tokens, bool &erased)
{
  // 461 = ERR_NEEDMOREPARAMS
  // 462 = ERR_ALREADYREFISTRED
  // 464 = ERR_PASSWDMISMATCH

  User &user = _Users[_pollFds[i].fd];
	std::string target = user._nickName.empty() ? "*" : user._nickName;

  if (tokens.empty())
  {
    _sendMessage(i, ":localhost.ircserver 461 " + target + " PASS :Not enough parameters\r\n");
    (_handleQuit(i, "auth error"), erased = true);
  }
  else if (user._registered)
  {
    _sendMessage(i, ":localhost.ircserver 462 " + target + " :You may not reregister\r\n");
    (_handleQuit(i, "auth error"), erased = true);
  }
  else if (tokens[0] != _password)
  {
    _sendMessage(i, ":localhost.ircserver 464 " + target + " :Password incorrect\r\n");
    (_handleQuit(i, "auth error"), erased = true);
  }
  else
    user._authenticated = true;
}

void Server::_handleNick(size_t i, std::vector<std::string> &tokens, bool &erased)
{
  // 431 = ERR_NONICKNAMEGIVEN
  // 432 = ERR_ERRONEUSNICKNAME
  // 433 = ERR_NICKNAMEINUSE

	int fd = _pollFds[i].fd;
  User &user = _Users[fd];
	std::string target = user._nickName.empty() ? "*" : user._nickName;

  size_t j;

  if (tokens.empty())
    _sendMessage(i, ":localhost.ircserver 431 " + target + " :No nickname given\r\n");
  else if (!user._authenticated)
  {
    _sendMessage(i, ":localhost.ircserver 464 " + target + " :Password required\r\n");
    (_handleQuit(i, "auth error"), erased = true);
  }
  else if (!User::_validNickName(tokens[0]))
    _sendMessage(i, ":localhost.ircserver 432 "  + tokens[0] + " :Erroneous nickname\r\n");
  else if ((j = _getPollIndexByNick(tokens[0])) != 0 && i != j)
    _sendMessage(i, ":localhost 433 " + tokens[0] + " :Nickname is already in use\r\n");
  else
  {
    if (user._registered) // must broadcast nickname change (potentially same nickname)
    {
			std::string msg = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " NICK :" + tokens[0] + "\r\n";
			for (std::set<std::string>::iterator it = user._joinedChannels.begin(); it != user._joinedChannels.end(); ++it)
  		{
    		std::string chanName = *it;
				_broadcastToChannel(chanName, msg, fd);
			}
    }
    user._nickName = tokens[0];
  }
}

void Server::_handleUser(size_t i, std::vector<std::string> &tokens, bool &erased)
{
  // 461 = ERR_NEEDMOREPARAMS
  // 462 = ERR_ALREADYREGISTRED

  User &user = _Users[_pollFds[i].fd];
	std::string target = user._nickName.empty() ? "*" : user._nickName;

  if (tokens.size() < 4)
    _sendMessage(i, ":localhost.ircserver 461 " + target + " USER :Not enough parameters\r\n");
  else if (!user._authenticated)
  {
    _sendMessage(i, ":localhost.ircserver 464 " + target + " :Password required\r\n");
    (_handleQuit(i, "auth error"), erased = true);
  }
  else if (user._registered)
    _sendMessage(i, ":localhost.ircserver 462 " + target + " :You may not reregister\r\n");
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

void Server::_handleJoin(size_t i, std::vector<std::string> &tokens) {
  // extracts the channel names (first argument) and the channel keys (second
  // argument) from tokens for each channel name/key pair:
  // 1. validate channel name syntax
  // 2. check channel existence
  // 2.1 channel doesnt exist -> create it and add user to operators, then add channel to server
  // 2.2 channel exists -> check double-join? -> invite-only? -> key correct? -> channel full? -> add user fd to channel
  // 3. add channel name to user's joinedChannels
  // 4. broadcast to channel that the new user joined
  // 5. send success message to client: TOPIC + NAMEREPLY (RFC Join)

  int userFd = _pollFds[i].fd;
  User &user = _Users[userFd];
  std::string target = user._nickName.empty() ? "*" : user._nickName;

  if (tokens.empty())
  {
    _sendMessage(i, ":localhost.ircserver 461 " + target + " JOIN :Not enough parameters\r\n");
    return;
  }
  std::vector<std::string> chanList = splitByComma(tokens[0]);
  std::vector<std::string> keyList;
  if (tokens.size() > 1)
    keyList = splitByComma(tokens[1]);

  for (size_t x = 0; x < chanList.size(); x++) 
  {
    std::string chanName = chanList[x];
    std::string lowerChanName = _tolowerStr(chanName);
    std::string providedKey = (x < keyList.size()) ? keyList[x] : "";

    if (!Channel::_validChannelName(chanName))
    {
      _sendMessage(i, ":localhost.ircserver 403 " + target + " " + chanName + " :No such channel\r\n");
      continue;
    }

    bool chanExists = (_Channels.find(lowerChanName) != _Channels.end());

    if (!chanExists) {
      Channel newChan;
      newChan._name = chanName; 
      newChan._memberFds.insert(userFd);
      newChan._operatorFds.insert(userFd);

      _Channels[lowerChanName] = newChan;
    }
    else
    {
      Channel &chan = _Channels[lowerChanName];

      if (chan._memberFds.find(userFd) != chan._memberFds.end())
        continue; // double-join

      if (chan._inviteOnly && chan._invitedFds.find(userFd) == chan._invitedFds.end())
      {
        _sendMessage(i, ":localhost.ircserver 473 " + target + " " + chanName + " :Cannot join channel (+i)\r\n");
        continue;
      }

      if (!chan._key.empty() && providedKey != chan._key)
      {
        _sendMessage(i, ":localhost.ircserver 475 " + target + " " + chanName + " :Cannot join channel (+k)\r\n");
        continue;
      }

      if (chan._userLimit > 0 && chan._memberFds.size() >= chan._userLimit)
      {
        _sendMessage(i, ":localhost.ircserver 471 " + target + " " + chanName + " :Cannot join channel (+l)\r\n");
        continue;
      }

      chan._memberFds.insert(userFd);
    }

    user._joinedChannels.insert(lowerChanName);

    std::string joinMsg = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " JOIN :" + chanName + "\r\n";
    _broadcastToChannel(lowerChanName, joinMsg, -1);

    Channel &chan = _Channels[lowerChanName];
    if (chan._topic.empty())
      _sendMessage(i, ":localhost.ircserver 331 " + target + " " + chanName + " :No topic is set\r\n");
    else
      _sendMessage(i, ":localhost.ircserver 332 " + target + " " + chanName + " :" + chan._topic + "\r\n");

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

    _sendMessage(i, ":localhost.ircserver 353 " + target + " = " + chanName + " :" + nameList + "\r\n");
    _sendMessage(i, ":localhost.ircserver 366 " + target + " " + chanName + " :End of /NAMES list.\r\n");
  }
}

void Server::_handlePart(size_t i, std::vector<std::string> &tokens) {
  // 1. check parameters, extract channel names
  // 2. for each channel:
  // 2.1 channel exists?
  // 2.2 user in channel?
  // 2.3 remove user from channel and broadcast part message
	// 2.4 remove channel name from user's joined channels
  // 2.5 if channel is empty, remove it

  int userFd = _pollFds[i].fd;
  User &user = _Users[userFd];
	std::string target = user._nickName.empty() ? "*" : user._nickName;

  if (tokens.empty())
  {
    _sendMessage(i, ":localhost.ircserver 461 " + target + " PART :Not enough parameters\r\n");
    return;
  }
  std::vector<std::string> chanList = splitByComma(tokens[0]);

  for (size_t x = 0; x < chanList.size(); x++)
  {
    std::string chanName = chanList[x];
    std::string lowerChanName = _tolowerStr(chanName);

    bool chanExists = (_Channels.find(lowerChanName) != _Channels.end());
    if (!chanExists)
    {
      _sendMessage(i, ":localhost.ircserver 403 " + target + " " + chanName + " :No such channel\r\n");
      continue;
    }

    Channel &chan = _Channels[lowerChanName];
    if (chan._memberFds.find(userFd) == chan._memberFds.end())
    {
      _sendMessage(i, ":localhost.ircserver 442 " + target + " " + chanName + " :You're not on that channel\r\n");
      continue;
    }

    std::string partMsg = ":" + user._nickName + "!" + user._userName + "@" + user._hostName + " PART :" + chanName + "\r\n";
    _broadcastToChannel(lowerChanName, partMsg, -1);

    chan._memberFds.erase(userFd);
    chan._operatorFds.erase(userFd);
    chan._invitedFds.erase(userFd);

		user._joinedChannels.erase(lowerChanName);

		if (chan._memberFds.empty())
      _Channels.erase(chanName);
  }
}

void Server::_handlePrivmsg(size_t i, std::vector<std::string> &tokens) {
  // 1. check parameters, extract recipients and message
  // 2. loop over each recipient:
  // 2.1 target is channel -> check channel exists? -> user is channel member? -> send to members expect sender
  // 2.2 target is a user -> check user existence? -> send to user

  int userFd = _pollFds[i].fd;
  User &user = _Users[userFd];

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

      Channel &chan = _Channels[lowerChanName];

      if (chan._memberFds.find(userFd) == chan._memberFds.end())
      {
        _sendMessage(i, ":localhost.ircserver 404 " + user._nickName + " " + target + " :Cannot send to channel\r\n");
        continue;
      }
      _broadcastToChannel(lowerChanName, msgPrefix, userFd);
    }
    else
    {
      size_t targetIndex = _getPollIndexByNick(target);
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
bool Server::_lookupChannel(std::string channelName) {

  if (_Channels.find(channelName) != _Channels.end())
    return (true);

  return (false);
}

bool Server::_lookupSender(int userFd, std::string channelName) {

  Channel &targetChannel = _Channels.find(channelName)->second;

  if (targetChannel._memberFds.find(userFd) != targetChannel._memberFds.end())
    return (true);

  return (false);
}

bool Server::_lookupSenderPrivilege(int userFd, std::string channelName) {

  Channel &targetChannel = _Channels.find(channelName)->second;

  if (targetChannel._operatorFds.find(userFd) !=
      targetChannel._operatorFds.end())
    return (true);
  return (false);
}

void Server::_removeUserFromChannel(int userFd, std::string channelName) {

  Channel &targetChannel = _Channels.find(channelName)->second;

  targetChannel._memberFds.erase(userFd);
  targetChannel._operatorFds.erase(userFd);
  targetChannel._invitedFds.erase(userFd);
}

void Server::_handleKick(size_t i, std::vector<std::string> &tokens) {

  if (tokens.size() < 2) {
    _sendMessage(i, ":localhost.ircserver 461 KICK :Not enough parameters\r\n");
    return;
  }

  int senderFd = _pollFds[i].fd;

  std::vector<std::string> chanList = splitByComma(tokens[0]);
  std::vector<std::string> userList = splitByComma(tokens[1]);
  std::string reason;

  if (tokens.size() > 2)
    reason = tokens[2];

  if (chanList.size() > 1 && userList.size() > 1 &&
      chanList.size() != userList.size()) {
    _sendMessage(i, ":localhost.ircserver 461 KICK :Not enough parameters\r\n");
    return;
  }

  size_t maxIter = (chanList.size() > userList.size()) ? chanList.size() : userList.size();

  for (size_t k = 0; k < maxIter; k++) {

    std::string currentChan = (chanList.size() == 1) ? chanList[0] : chanList[k];
    std::string currentUser = (userList.size() == 1) ? userList[0] : userList[k];
    std::string lowerChanName = _tolowerStr(currentChan);

    if (!_lookupChannel(lowerChanName)) {
      _sendMessage(i, ":localhost.ircserver 403 KICK " + currentChan + " :No such channel\r\n");
      continue;
    }
    if (!_lookupSender(senderFd, lowerChanName)) {
      _sendMessage(i, ":localhost.ircserver 442 KICK " + currentChan + " :You're not on that channel\r\n");
      continue;
    }
    if (!_lookupSenderPrivilege(senderFd, lowerChanName)) {
      _sendMessage(i, "localhost.ircserver 482 KICK " + currentChan + " :You're not channel operator\r\n");
      continue;
    }

    size_t targetUserPollIndex = _getPollIndexByNick(currentUser);
    if (!targetUserPollIndex) {
      _sendMessage(i, "localhost.ircserver 401 KICK " + currentUser + " :No such nick\r\n");
      continue;
    }

    int targetUserFd = _pollFds[targetUserPollIndex].fd;

    if (!_lookupSender(targetUserFd, lowerChanName)) {
      _sendMessage(i, "localhost.ircserver 441 KICK " + currentUser + " " + currentChan + " :They aren't on that channel\r\n");
      continue;
    }

    User& senderUser = _Users[senderFd];
    std::string kickMsg = ":" + senderUser._nickName + "!" + senderUser._userName + "@" + senderUser._hostName + " KICK " + currentChan + " " + currentUser;
    if (!reason.empty())
      kickMsg += " :" + reason;
    kickMsg += "\r\n";

    _broadcastToChannel(currentChan, kickMsg, -1);

    _removeUserFromChannel(targetUserFd, currentChan);

    _Users.find(targetUserFd)->second._joinedChannels.erase(lowerChanName);

    if (_Channels.find(currentChan)->second._memberFds.empty())
      _Channels.erase(_Channels.find(currentChan));
  }
}

void Server::_handleInvite(size_t i, std::vector<std::string> &tokens) {

  if (tokens.size() < 2) {
    _sendMessage(i, ":localhost.ircserver 461 INVITE :Not enough parameters\r\n");
    return;
  }

  int senderFd = _pollFds[i].fd;
  std::string targetUser = tokens[0];
  std::string targetChan = tokens[1];
  std::string lowerChanName = _tolowerStr(targetChan);

  User& sender = _Users[senderFd];

  if (!_lookupChannel(lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 403 INVITE " + targetChan + " :No such channel\r\n");
    return ;
  }

  if (!_lookupSender(senderFd, lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 442 INVITE " + targetChan + " :You're not on that channel\r\n");
    return ;
  }

  Channel&  chan = _Channels[lowerChanName];

  if (chan._inviteOnly) {
    if (!_lookupSenderPrivilege(senderFd, lowerChanName)) {
      _sendMessage(i, "localhost.ircserver 482 INVITE " + targetChan + " :You're not channel operator\r\n");
      return ;
    }
  }

  size_t  targetUserPollIndex = _getPollIndexByNick(targetUser);
  if (!targetUserPollIndex) {
    _sendMessage(i, "localhost.ircserver 401 INVITE " + targetUser + " :No such nick\r\n");
    return ;
  }

  int targetUserFd = _pollFds[targetUserPollIndex].fd;

  if (_lookupSender(targetUserFd, lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 443 INVITE " + targetUser + " " + targetChan + " :is already on channel\r\n");
    return ;
  }

  chan._invitedFds.insert(targetUserFd);
  _sendMessage(i, ":localhost.ircserver 341 INVITE " + sender._nickName + " " + targetUser + " " + targetChan + "\r\n");
  _sendMessage(targetUserPollIndex, ":" + sender._nickName + "!" + sender._userName + "@" + sender._hostName + " INVITE " + targetUser + " :" + targetChan + "\r\n");
}

void Server::_handleMode(size_t i, std::vector<std::string> &tokens) {

  if (tokens.size() < 1) {
    _sendMessage(i, "MODE :Not enough parameters\r\n");
    return ;
  }

  std::string target = tokens[0];
  if (target[0] != '#' && target[0] != '&')
    return ;

  std::string lowerChanName = _tolowerStr(target);
  if (!_lookupChannel(lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 403 MODE :" + target + " :No such channel\r\n");
    return ;
  }

  int       senderFd = _pollFds[i].fd;
  User&     sender = _Users[senderFd];
  Channel&  chan = _Channels[lowerChanName];

  if (tokens.size() == 1) {
    std::string viewModeStr = ":localhost.ircserver 324 " + sender._nickName + " " + target + " ";
    if (chan._inviteOnly)
      viewModeStr += "+i";
    if (chan._topicProtected)
      viewModeStr += "+t";
    if (!chan._key.empty())
      viewModeStr += "+k";
    if (chan._userLimit > 0) {
      std::ostringstream  oss;
      oss << chan._userLimit;
      viewModeStr += "+l" + oss.str();
    }
    viewModeStr += "\r\n";
    _sendMessage(i, viewModeStr);
    return ;
  }

  if (!_lookupSenderPrivilege(senderFd, lowerChanName)) {
    _sendMessage(i, "localhost.ircserver 482 MODE :" + target + " :You're not channel operator\r\n");
    return ;
  }

  std::string modeStr = tokens[1];
  bool        sign = true;
  size_t      argIndex = 2;

  for (size_t j = 0; j < modeStr.size(); j++) {
    char  c = modeStr[j];

    if (c == '+')
      sign = true;
    else if (c == '-')
      sign = false;
    else if (c == 'i')
      chan._inviteOnly = sign;
    else if (c == 't')
      chan._topicProtected = sign;
    else if (c == 'k') {
      if (sign) {
        if (argIndex < tokens.size())
          chan._key = tokens[argIndex++];
        else {
          _sendMessage(i, ":localhost.ircserver 461 MODE :Not enough parameters\r\n");
          return ;
        }
      }
      else
        chan._key = "";
    }
    else if (c == 'l') {
      if (sign) {
        if (argIndex < tokens.size())
          chan._userLimit = std::atoi(tokens[argIndex++].c_str());
        else {
          _sendMessage(i, ":localhost.ircserver 461 MODE :Not enough paramters\r\n");
          return ;
        }
      }
      else
        chan._userLimit = 0;
    }
    else if (c == 'o') {
      if (argIndex < tokens.size()) {
        std::string targetNick = tokens[argIndex++];
        size_t      targetUserPollIndex = _getPollIndexByNick(targetNick);
        if (!targetUserPollIndex) {
          _sendMessage(i, ":localhost.ircserver 401 " + sender._nickName + " " + targetNick + " :No such nick\r\n");
          continue ;
        }
        int targetUserFd = _pollFds[targetUserPollIndex].fd;
        if (sign)
          chan._operatorFds.insert(targetUserFd);
        else
          chan._operatorFds.erase(targetUserFd);
      }
      else {
        _sendMessage(i, ":localhost.ircserver 461 MODE :Not enough paramters\r\n");
        return ;
      }
    }
    else
      _sendMessage(i, ":localhost.ircserver 472 " + sender._nickName + " " + c + " :is unknown mode char to me \r\n");
  }

  std::string broadcastMsg = ":" + sender._nickName + "!" + sender._userName + "@" + sender._hostName + " MODE ";
  for (size_t t = 0; t < tokens.size(); t++) {
    if (t < tokens.size() - 1)
      broadcastMsg += " ";
  }
  broadcastMsg += "\r\n";
  _broadcastToChannel(lowerChanName, broadcastMsg, -1);
}

void Server::_handleTopic(size_t i, std::vector<std::string> &tokens) {

  if (tokens.size() < 1) {
    _sendMessage(i, ":localhost.ircserver 461 TOPIC :Not enough paramters\r\n");
    return ;
  }

  std::string targetChan = tokens[0];
  std::string lowerChanName = _tolowerStr(targetChan);

  if (!_lookupChannel(lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 403 " + _Users[_pollFds[i].fd]._nickName + " " + targetChan + " :No such channel\r\n");
    return ;
  }

  int senderFd = _pollFds[i].fd;

  if (!_lookupSender(senderFd, lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 442 " + _Users[_pollFds[i].fd]._nickName + " " + targetChan + " :You're not on that channel\r\n");
    return ;
  }

  Channel&  chan = _Channels[lowerChanName];
  User&     sender = _Users[senderFd];

  if (tokens.size() == 1) {
    if (chan._topic.empty())
      _sendMessage(i, ":localhost.ircserver 331 " + sender._nickName + " " + targetChan + " :No topic is set\r\n");
    else
      _sendMessage(i, ":localhost.ircserver 332 " + sender._nickName + " " + targetChan + " :" + chan._topic + "\r\n");
    return ;
  }

  if (chan._topicProtected) {
    if (!_lookupSenderPrivilege(senderFd, lowerChanName)) {
      _sendMessage(i, ":localhost.ircserver 482 " + sender._nickName + " " + targetChan + " :You're not channel operator\r\n");
      return ;
    }
  }

  chan._topic = tokens[1];
  std::string broadcastMsg = ":" + sender._nickName + "!" + sender._userName + "@" + sender._hostName + " TOPIC " + targetChan + " :" + tokens[1] + "\r\n";
  _broadcastToChannel(lowerChanName, broadcastMsg, -1);
}
/* ---------------------------------------------------------------------------------------- */
