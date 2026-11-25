NAME		= webserv

CXX			= c++
CXXFLAGS    = -O3 -Wall -Wextra -Werror -I. -MMD -MP

OBJ_DIR		= obj

SRCS		= main.cpp \
			  httpParser/Headers.cpp \
			  www/cgi/CgiHandler.cpp \
			  httpParser/HttpBody.cpp \
			  www/cgi/CgiResponse.cpp \
			  httpParser/HttpParser.cpp \
			  httpParser/RequestLine.cpp \
			  httpParser/HeaderUtils.cpp \
			  httpParser/ContentType.cpp \
			  ConfigParser/ConfigLex.cpp \
			  httpParser/HeaderValues.cpp \
			  ConfigParser/ConfigParser.cpp \
			  ServerHandler/core/Server.cpp \
			  ServerHandler/handlers/HandleGet.cpp \
			  ServerHandler/events/ServerEvents.cpp \
			  ServerHandler/handlers/HandlePost.cpp \
			  ServerHandler/responses/Responder.cpp \
			  ConfigParser/ConfigDirectiveParser.cpp \
			  ServerHandler/handlers/HandleDelete.cpp \
			  ServerHandler/responses/ServerErrors.cpp \
			  ServerHandler/events/ServerPollEvents.cpp \
			  ServerHandler/responses/ResponseUtils.cpp \
			  ServerHandler/events/ServerClientState.cpp \
			  ServerHandler/responses/ResponseOutput.cpp \
			  ServerHandler/core/HttpRequestProcessing.cpp

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
