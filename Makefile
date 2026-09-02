NAME		= webserv

CXX			= c++
CXXFLAGS	= -Wall -Wextra -Werror -std=c++11

OBJ_DIR		= obj

#SRCS		= httpParser/HttpParser.cpp \
#			  httpParser/HttpTester.cpp \
#			  httpParser/RequestLine.cpp \
#			  httpParser/Headers.cpp \
#			  httpParser/HeaderUtils.cpp \
#			  httpParser/HeaderValues.cpp \
#			  httpParser/ContentType.cpp \
#			  #ConfigParser.cpp \

SRCS =   ConfigParser/ConfigParser.cpp \
			  ConfigParser/ConfigLex.cpp \
			  main.cpp

OBJS 		= $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

RM			= rm -rf

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re