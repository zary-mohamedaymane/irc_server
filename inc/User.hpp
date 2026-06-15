#ifndef USER_HPP
#define USER_HPP

#include <string>
#include <set>

class User
{
  public:
    std::string           _buffer;

    bool                  _authenticated;
    bool                  _registered;

    std::string           _nickName;
    std::string           _userName;
    std::string           _hostName;
    std::string           _realName;

    std::set<std::string> _joinedChannels; // here channel names are lowercased

    User();

    static bool _validNickName(std::string nickName);
};

#endif