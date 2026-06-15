#include "../inc/User.hpp"
#include <cctype> // std::isspace

User::User(): _authenticated(false), _registered(false) {}

bool User::_validNickName(std::string s) // double-check the protocol grammar
{
	if (s.empty() || s.size() > 9)
		return false;
	if (s[0] == '-' || ('0' <= s[0] && s[0] <= '9'))
		return false;
	for (size_t i = 0; i < s.size(); i++)
	{
		if (std::isspace(s[i]) || s[i] == '@' || s[i] == '!' || s[i] == '#') // &?
			return false;
	}
	return true;
}