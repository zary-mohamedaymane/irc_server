#include "../inc/Server.hpp"
#include <iostream>
#include <sstream>
#include <string>

void Server::_handleCapabilityNegotiation(size_t i, std::vector<std::string> &tokens)
{
  User &user = _Users[_pollFds[i].fd];

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

void Server::_handlePass(size_t i, std::vector<std::string> &tokens, bool &erased)
{
  // 461 = ERR_NEEDMOREPARAMS
  // 462 = ERR_ALREADYREFISTRED
  // 464 = ERR_PASSWDMISMATCH

  User &user = _Users[_pollFds[i].fd];

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

void Server::_handleNick(size_t i, std::vector<std::string> &tokens, bool &erased)
{
  // 431 = ERR_NONICKNAMEGIVEN
  // 432 = ERR_ERRONEUSNICKNAME
  // 433 = ERR_NICKNAMEINUSE

  User &user = _Users[_pollFds[i].fd];
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

void Server::_handleUser(size_t i, std::vector<std::string> &tokens, bool &erased)
{
  // 461 = ERR_NEEDMOREPARAMS
  // 462 = ERR_ALREADYREGISTRED

  User &user = _Users[_pollFds[i].fd];

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

void Server::_handleJoin(size_t i, std::vector<std::string> &tokens) {
  // extracts the channel names (first argument) and the channel keys (second
  // argument) from tokens for each channel name/key pair:
  // 1. validate channel name syntax
  // 2. check channel existence
  // 2.1 channel doesnt exist -> create it (CHECK GRAMMAR!!!!) and add user to operators, then add channel to server
  // 2.2 channel exists -> check double-join? -> invite-only? -> key correct? -> channel full? -> add user fd to channel
  // 3. add channel name to user's joinedChannels
  // 4. broadcast to channel that the new user joined
  // 5. send success message to client: TOPIC + NAMEREPLY (RFC Join)

  int userFd = _pollFds[i].fd;
  User &user = _Users[userFd];

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
    std::string lowerChanName = _tolowerStr(chanName);
    std::string providedKey = (x < keyList.size()) ? keyList[x] : "";

    if (!(chanName[0] == '#' || chanName[0] == '&')) // check RFC for grammar
    {
      _sendMessage(i, ":localhost.ircserver 403 " + user._nickName + " " + chanName + " :No such channel\r\n");
      continue;
    }

    bool chanExists = (_Channels.find(lowerChanName) != _Channels.end());

    if (!chanExists) {
      // YOU HAVE TO PARSE CHANNEL NAME AND CHECK IF IT IS VALID!
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

    Channel &chan = _Channels[chanName];
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

void Server::_handlePart(size_t i, std::vector<std::string> &tokens) {
  // 1. check parameters, extract channel names
  // 2. for each channel
  // 2.1 channel exists?
  // 2.2 user in channel?
  // 2.3 remove user from channel and broadcast part message to channe leaving user
  // 2.4 if channel is empty, remove it

  int userFd = _pollFds[i].fd;
  User &user = _Users[userFd];

  if (tokens.empty())
  {
    _sendMessage(i, ":localhost.ircserver 461 PART :Not enough parameters\r\n");
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
      _sendMessage(i, ":localhost.ircserver 403 " + chanName + " :No such channel\r\n");
      continue;
    }

    Channel &chan = _Channels[lowerChanName];
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
      _sendMessage(i, ":localhost.ircserver 403 KICK :" + currentChan + " :No such channel\r\n");
      continue;
    }
    if (!_lookupSender(senderFd, lowerChanName)) {
      _sendMessage(i, ":localhost.ircserver 442 KICK:" + currentChan + " :You're not on that channel\r\n");
      continue;
    }
    if (!_lookupSenderPrivilege(senderFd, lowerChanName)) {
      _sendMessage(i, "localhost.ircserver 482 KICK:" + currentChan + " :You're not channel operator\r\n");
      continue;
    }

    size_t targetUserPollIndex = _getUserByNick(currentUser);
    if (!targetUserPollIndex) {
      _sendMessage(i, "localhost.ircserver 401 KICK:" + currentChan + " :No such nick\r\n");
      continue;
    }

    int targetUserFd = _pollFds[targetUserPollIndex].fd;

    if (!_lookupSender(targetUserFd, lowerChanName)) {
      _sendMessage(i, "localhost.ircserver 441 KICK:" + currentChan + " :They aren't on that channel\r\n");
      continue;
    }

    User& senderUser = _Users[senderFd];
    std::string kickMsg = ":" + senderUser._nickName + "!" + senderUser._userName + "@" + senderUser._hostName + " KICK " + currentChan + " " + currentUser;
    if (!reason.empty())
      kickMsg += ":" + reason;
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
    _sendMessage(i, ":localhost.ircserver 403 INVITE :" + targetChan + " :No such channel\r\n");
    return ;
  }

  if (!_lookupSender(senderFd, lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 442 INVITE :" + targetChan + " :You're not on that channel\r\n");
    return ;
  }

  Channel&  chan = _Channels[lowerChanName];

  if (chan._inviteOnly) {
    if (!_lookupSenderPrivilege(senderFd, lowerChanName)) {
      _sendMessage(i, "localhost.ircserver 482 INVITE :" + targetChan + " :You're not channel operator\r\n");
      return ;
    }
  }

  size_t  targetUserPollIndex = _getUserByNick(targetUser);
  if (!targetUserPollIndex) {
    _sendMessage(i, "localhost.ircserver 401 INVITE :" + targetChan + " :No such nick\r\n");
    return ;
  }

  int targetUserFd = _pollFds[targetUserPollIndex].fd;

  if (_lookupSender(targetUserFd, lowerChanName)) {
    _sendMessage(i, ":localhost.ircserver 443 " + _Users.find(senderFd)->second._nickName + " " + targetUser + " " + targetChan + " :is already on channel\r\n");
    return ;
  }

  chan._invitedFds.insert(targetUserFd);
  _sendMessage(i, ":localhost.ircserver 341 " + sender._nickName + " " + targetUser + " " + targetChan + "\r\n");
  _sendMessage(targetUserPollIndex, ":" + sender._nickName + "!" + sender._userName + "@" + sender._hostName + "INVITE" + targetUser + " :" + targetChan + "\r\n");
}

void Server::_handleMode(size_t i, std::vector<std::string> &tokens) {

  if (tokens.size() < 1) {
    _sendMessage(i, "MODE :Not enough parameters\r\n");
    return ;
  }

  std::string target = tokens[0];
  if (target[0] != '#' || target[0] != '&')
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
    if (chan._userLimit > 0)
      viewModeStr += "+l" + std::to_string(chan._userLimit);
    viewModeStr += "\r\n";
    _sendMessage(i, viewModeStr);
    return ;
  }

  if (_lookupSenderPrivilege(senderFd, lowerChanName)) {
    _sendMessage(i, "localhost.ircserver 482 MODE :" + target + " :You're not channel operator\r\n");
    return ;
  }

  std::string modeStr = tokens[1];
  bool        sign = true;
  size_t      argIndex = 2;

  for (size_t j = 0; j < modeStr.size(); j++) {
    if (modeStr[j] == '+') {
      sign = true;
      continue ;
    }
    else if (modeStr[j] == '-') {
      sign = false;
      continue ;
    }

    if (sign == true && modeStr[j] == 'i') {
      chan._inviteOnly = true;
      continue ;
    }
    else {
      chan._inviteOnly = false;
      continue ;
    }

    if (sign == true && modeStr[j] == 't') {
      chan._topicProtected = true;
      continue ;
    }
    else {
      chan._topicProtected = false;
        continue ;
    }

    if (sign == true && modeStr[j] == 'k') {
      if (argIndex < tokens.size())
        chan._key = tokens[argIndex++];
      else {
        _sendMessage(i, ":localhost.ircserver 461 MODE :Not enough parameters\r\n");
        return ;
      }
      continue ;
    }
    else {
      chan._key = "";
      continue ;
    }

    if (sign == true && modeStr[j] == 'l') {
      if (argIndex < tokens.size())
        chan._userLimit = std::atoi(tokens[argIndex++].c_str());
      else {
        _sendMessage(i, ":localhost.ircserver 461 MODE :Not enough parameters\r\n");
      }
      continue ;
    }
    else {
      chan._userLimit = 0;
      continue ;
    }

    if (modeStr[j] == 'o') {
      std::string targetNick;
      size_t  targetUserPollFd;
      if (argIndex < tokens.size()) {
        targetNick = tokens[argIndex++];
        targetUserPollFd = _getUserByNick(targetNick);
        if (!targetUserPollFd)
          continue ;
      }
      int targetUserFd = _pollFds[targetUserPollFd].fd;
      if (sign == true) {
        chan._operatorFds.insert(targetUserFd);
        continue ;
      }
      else {
        chan._operatorFds.erase(targetUserFd);
        continue ;
      }
    }

    _sendMessage(i, ":localhost.ircserver 472 " + sender._nickName + " " + modeStr[j] + " :is unknown mode char to me \r\n");
    continue ;
  }
}

void Server::_handleTopic(size_t i, std::vector<std::string> &tokens) {

}
/* ---------------------------------------------------------------------------------------- */
