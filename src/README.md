*This project has been created as part of the 42 curriculum by mzary and yhajbi.*


# Description

In this project we implement our own IRC server following the RFC 1459 published in 1993 by Jarkko Oikarinen and Darren Reed.
IRC (Internet Relay Chat) is a text-based instant messaging protocol. IRC works on a client to server networking model and operates on the application layer of the OSI model.Users may connect to a server using a web app, a dedicated client software, such as irssi,  or simply
using the network utility command nc (NetCat).


# Instructions

clone and compile using `make`.
start the server using `./ircserv <port> <password>`.
connect to the server using irssi or nc.


# Resources

## references

**External functions used**
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

https://en.cppreference.com/cpp/container/vector
https://en.cppreference.com/cpp/container/map

**RFC 1459**
https://datatracker.ietf.org/doc/html/rfc1459

## AI usage

The usage of AI was employed to test the code's integrety, catch bugs and optimizing the code.
