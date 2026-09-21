NAME		= webserv

CXX			= c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++17 -I. -MMD -MP

OBJ_DIR		= obj

SRCS		= httpParser/HttpParser.cpp \
			  httpParser/HttpTester.cpp \
			  httpParser/RequestLine.cpp \
			  httpParser/HttpBody.cpp \
			  httpParser/Headers.cpp \
			  httpParser/HeaderUtils.cpp \
			  httpParser/HeaderValues.cpp \
			  httpParser/ContentType.cpp \
			  ConfigParser/ConfigParser.cpp \
			  ConfigParser/ConfigLex.cpp \
			  ServerHandler/Server.cpp \
			  Response/Response.cpp \
			  ServerHandler/HandleGet.cpp \
			  ServerHandler/HandleDelete.cpp \
			  ServerHandler/Responder.cpp \
			  ServerHandler/ServerErrors.cpp \
			  main.cpp

OBJS 		= $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS 		= $(OBJS:.o=.d)

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

-include $(DEPS)

.PHONY: all clean fclean re
