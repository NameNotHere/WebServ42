c++ -Wall -Wextra -Werror \
  test_cgi.cpp CgiHandler.cpp CgiHelper.cpp CgiResponse.cpp \
  -o test

Still need to connect it to main and Makefile
No socket or event loop invoke in CGIHandler
Need to add HTTP routing map URLs (script_name, script_path and PATH_INFO)
CGI configuration still confusing to me ngl
CgiResponse is not converted into servers HTTP because I don't know it
HEAD and redirects still need to be worked on, will have to look more into that as well

Ofcourse debugging because I tried to do the logic part myself but ofcourse also used quite a bit of AI