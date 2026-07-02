#include "../inc/User.hpp"
#include <cctype> // std::isspace

User::User(): _authenticated(false), _registered(false) {}

bool User::_validNickName(std::string s)
{
  // 1. Check length (Max 9 characters as per RFC 1459)
  if (s.empty() || s.size() > 9)
    return false;

  // Helper string containing allowed RFC 1459 special characters
  const std::string specials = "[]\\`^{}|-";

  // 2. Check the first character
  // Must be a letter or a special character (EXCEPT hyphen '-')
  char first = s[0];
  bool isFirstLetter = std::isalpha(static_cast<unsigned char>(first));
  bool isFirstSpecial = (specials.find(first) != std::string::npos && first != '-');

  if (!isFirstLetter && !isFirstSpecial)
    return false;

  // 3. Check subsequent characters
  // Can be letters, digits, or ANY of the allowed special characters (including '-')
  for (size_t i = 1; i < s.size(); i++)
  {
    char c = s[i];
    bool isLetter = std::isalpha(static_cast<unsigned char>(c));
    bool isDigit  = std::isdigit(static_cast<unsigned char>(c));
    bool isSpecial = (specials.find(c) != std::string::npos);

    if (!isLetter && !isDigit && !isSpecial)
      return false;
  }

  return true;
}
