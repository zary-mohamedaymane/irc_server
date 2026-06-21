#include "../inc/Server.hpp"
#include <cstdlib>   // std::atoi
#include <cstring>   // std::strlen
#include <exception> // std::exception
#include <iostream>
#include <signal.h> // signal()

static bool validPort(char *port) {
  size_t len = std::strlen(port);

  if (len == 0)
    return false;
  for (size_t i = 0; i < len; i++) {
    if (port[i] < '0' || port[i] > '9')
      return false;
  }
  int n = std::atoi(port);
  if (n < 1024 || n > 65535)
    return false;
  return true;
}

static bool validPassword(char *password) {
  size_t len = std::strlen(password);

  if (len == 0)
    return false;
  for (size_t i = 0; i < len; i++) {
    if (!std::isspace(password[i]))
      return true;
  }
  return false;
}

static void handler(int signum) {
  (void)signum;
  if (Server::instance)
    Server::instance->stop();
}

int main(int ac, char **av) {
  // parse arguments
  if (ac != 3) {
    std::cerr << "usage: ./ircserv <port> <password>" << std::endl;
    return 1;
  }
  if (!validPort(av[1]) || !validPassword(av[2])) {
    std::cerr << "invalid port or password" << std::endl;
    return 1;
  }
  int port = std::atoi(av[1]);
  char *password = av[2];

  try {
    // handle signals (global pointer to instance)
    signal(SIGINT, handler);
    signal(SIGQUIT, handler);
    signal(SIGTERM, handler);
    signal(SIGPIPE, SIG_IGN); // broken pipe while recv/send just ignore

    Server ircserver(port, password);
    ircserver.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
  }
  return 0;
}
