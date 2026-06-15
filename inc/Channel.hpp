#ifndef CHANNEL_HPP
//#define CHANNEL_HPP

#include <string>
#include <set>

class Channel
{
  public:
    std::string       _name; // not lowercased
    std::string       _topic;
    std::string       _key;
    size_t            _userLimit;   // (0 if unlimited)

    bool              _inviteOnly;
    bool              _topicProtected;

    std::set<int>     _memberFds;
    std::set<int>     _operatorFds;
    std::set<int>     _invitedFds;

    Channel();

    static bool _validChannelName(std::string chanName);
};

#endif
