#include "HttpParser.hpp"

int main(void)
{
	HttpParser test;

	// test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080 \r\nContent-length: 5  \r\n\r\nherro");
	// test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080  \r\nConnection:    kEeP-alive \r\nUser-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)  \r\nContent-length:  42   \r\nTrAnsFer-encoDing:  chunked  \r\nConTeNt-tYpe:             text/html ; charset=\"utf-\t8\";	 boundary=\"abc\\\"1\\\"23\"; charset=utf-10           \r\nExpect:     100-continue    \r\n\r\ntesting body");
	test.parseHttpRequest("GET /index.html HTTP/1.1\r\nHoSt: loCal-hOst:8080  \r\nContent-length:42\r\n\r\ntesting body");

	// std::cout << "\n-----Additional tests------\n";
	// std::cout << "Method:" << test.getMethod() << "\n";
	// std::cout << "Target:" << test.getTarget() << "\n";
	// std::cout << "Version:" << test.getVersion() << "\n";

	// std::cout << "\n----Printing Headers----\n";
	// for (std::map<std::string, std::string>::const_iterator it = test.getHeaders().begin();
	//      it != test.getHeaders().end(); ++it)
	// {
	//      std::cout << "Name:" << it->first << "\n";
	//      std::cout << "Value:" << it->second << "\n";
	// }

	return 0;
}

// OLD USEFUL TESTS


// Mini server

// int serverFd = socket(AF_INET, SOCK_STREAM, 0); // Front door for accepting new clients

// if(serverFd == -1)
// {
// 	std::cerr << "Server socket failed\n";
// 	return 1;
// }
// std::cout << "Server socket successful\n";

// // Built in socket struct "sockaddr_in"
// struct sockaddr_in serverAddress = {}; // Initlizes everything to 0

// serverAddress.sin_family = AF_INET; // IPV4
// serverAddress.sin_port = htons(8080); // converts port 8080 to bytes
// serverAddress.sin_addr.s_addr = htonl(INADDR_ANY); // listen on any local IPV4 address

// if(bind(serverFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) // Binds serverFd to serverAddress struct
// {
// 	std::cerr << "Bind failed\n";
// 	return 1;
// }
// else
// 	std::cout << "Bind successful\n";

// if(listen(serverFd, 10) == -1) // 10 is the max number of requests that can be in the 
// {							   //queue waiting to be accepted but not the max number of clients
// 	std::cerr << "Listen failed\n";
// 	return 1;
// }
// else
// 	std::cout << "Listen successful\n";

// // Accept new client and create new fd
// int clientFd = accept(serverFd, NULL, NULL);// NULL, NULL ignors new clients ip and port for now
// if(clientFd == -1)
// {
// 	std::cerr << "Accept failed\n";
// 	return 1;
// }
// else
// 	std::cout << "Accept successful\n";

// std::string clientBuffer;

// char buffer[4096]; // Temporary bucket for one receive call
// // int bytesReceived = recv(clientFd, buffer, sizeof(buffer), 0); // Receive bytes from clientFd, put them in buffer and
// 															   // 4096 bytes at a time, 0 = default behaviour

// HttpParser test;

// while(true)
// {
// 	std::cout << "Waiting for more bytes...\n";
// 	int bytesReceived = recv(clientFd, buffer, sizeof(buffer), 0);
// 	if(bytesReceived > 0) // Data received
// 	{
// 		clientBuffer.append(buffer, bytesReceived); // Append from buffer bytesReceived to clientBuffer
// 		std::cout << clientBuffer << "\n";
// 		test.parseHttpRequest(clientBuffer);
		
// 	}
// 	if(bytesReceived == 0)
// 	{
// 		std::cout << "Client dissconnected\n";
// 		break;
// 	}
// }
