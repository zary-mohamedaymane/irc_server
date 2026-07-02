#include "../inc/Channel.hpp"

Channel::Channel() : _userLimit(0), _inviteOnly(false), _topicProtected(true) {}

bool Channel::_validChannelName(std::string chanName)
{
	size_t len = chanName.length();

	if (len < 2 || len > 200)
		return false;

	if (!(chanName[0] == '#' || chanName[0] == '&'))
		return false;

	for (size_t i = 1; i < chanName.length(); i++)
	{
		if (std::isspace(chanName[i]) || chanName[i] == 7 || chanName[i] == ',')
			return false;
	}

	return true;
}

// Channels names are strings (beginning with a '&' or '#' character) of
// length up to 200 characters.  Apart from the the requirement that the
// first character being either '&' or '#'; the only restriction on a
// channel name is that it may not contain any spaces (' '), a control G
// (^G or ASCII 7), or a comma (',' which is used as a list item
// separator by the protocol).
