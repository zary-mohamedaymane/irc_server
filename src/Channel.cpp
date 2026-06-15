#include "../inc/Channel.hpp"

Channel::Channel() : _userLimit(0), _inviteOnly(false), _topicProtected(true) {}

bool Channel::_validChannelName(std::string chanName)
{
	// check if not empty
	if (!chanName.empty())
		return false;
	return true;
}
