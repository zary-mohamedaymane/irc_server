𝘛𝘩𝘪𝘴 𝘱𝘳𝘰𝘫𝘦𝘤𝘵 𝘩𝘢𝘴 𝘣𝘦𝘦𝘯 𝘤𝘳𝘦𝘢𝘵𝘦𝘥 𝘢𝘴 𝘱𝘢𝘳𝘵 𝘰𝘧 𝘵𝘩𝘦 42 𝘤𝘶𝘳𝘳𝘪𝘤𝘶𝘭𝘶𝘮 𝘣𝘺 𝘮𝘻𝘢𝘳𝘺, 𝘺𝘩𝘢𝘫𝘣𝘪.


# Description

The goal from this project is to implement a minimal IRC server following RFC 1459.
The functionalities that are supported are:
	- private and channel messaging
	- channels with the following commands for operators: kick, invite,
		and the following channel modes: key, limit, topic, operator, invite-only
The server is fully compatible with our selected client: sic (simple irc client)

# Instructions

* compile the project using `make`.
* start the server using `./ircserv <port> <password>`.
* connect to the server using sic or nc.

# Resources

## references

	Syscall documentation:

https://man7.org/linux/man-pages/man2/socket.2.html
https://man7.org/linux/man-pages/man2/fcntl.2.html
https://man7.org/linux/man-pages/man3/setsockopt.3p.html
https://man7.org/linux/man-pages/man0/sys_socket.h.0p.html
https://man7.org/linux/man-pages/man2/bind.2.html
https://man7.org/linux/man-pages/man7/ip.7.html
https://man7.org/linux/man-pages/man3/sockaddr_in.3type.html
https://man7.org/linux/man-pages/man2/listen.2.html
https://man7.org/linux/man-pages/man2/poll.2.html
https://man7.org/linux/man-pages/man2/accept.2.html
https://man7.org/linux/man-pages/man2/recv.2.html
https://man7.org/linux/man-pages/man2/sendmsg.2.html

	Language features (c++98):

https://en.cppreference.com/cpp/container/vector
https://en.cppreference.com/cpp/container/map

	IRC Protocol (RFC 1459):
	
https://datatracker.ietf.org/doc/html/rfc1459

## AI usage

AI was smartly used to help with documentation, as well as debug specific client behaviors.
