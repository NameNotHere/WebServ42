NAME		= webserv

CXX			= c++
CXXFLAGS    = -O3 -Wall -Wextra -Werror -I. -MMD -MP

OBJ_DIR		= obj

SRCS		= httpParser/HttpParser.cpp \
			  httpParser/RequestLine.cpp \
			  httpParser/HttpBody.cpp \
			  httpParser/Headers.cpp \
			  httpParser/HeaderUtils.cpp \
			  httpParser/HeaderValues.cpp \
			  httpParser/ContentType.cpp \
			  ConfigParser/ConfigDirectiveParser.cpp \
			  ConfigParser/ConfigParser.cpp \
			  ConfigParser/ConfigLex.cpp \
			  ServerHandler/core/Server.cpp \
			  ServerHandler/events/ServerEvents.cpp \
			  ServerHandler/events/ServerClientState.cpp \
			  ServerHandler/events/ServerPollEvents.cpp \
			  ServerHandler/core/HttpRequestProcessing.cpp \
			  ServerHandler/handlers/HandleGet.cpp \
			  ServerHandler/handlers/HandleDelete.cpp \
			  ServerHandler/handlers/HandlePost.cpp \
			  ServerHandler/responses/ResponseOutput.cpp \
			  ServerHandler/responses/ResponseUtils.cpp \
			  ServerHandler/responses/Responder.cpp \
			  ServerHandler/responses/ServerErrors.cpp \
			  www/cgi/CgiHandler.cpp \
			  www/cgi/CgiResponse.cpp \
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
