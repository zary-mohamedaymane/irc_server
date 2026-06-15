NAME	= ircserv
CC		= c++ -g -O0 -std=c++98 -Wall -Wextra -Werror

SRC		= src/main.cpp src/Server.cpp src/Commands.cpp src/User.cpp src/Channel.cpp
INC		= inc/Server.hpp inc/User.hpp inc/Channel.hpp
OBJ		= $(SRC:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CC) $^ -o $@

%.o: %.cpp $(INC)
	$(CC) -c $< -o $@

clean:
	rm -f $(OBJ)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: clean