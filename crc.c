#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sstream>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"
#include "interface.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <stdexcept>
#include <fcntl.h>
    
using namespace std;

int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char *command);
void process_chatmode(const char *host, const int port);

int main(int argc, char **argv){

	// Check if there are not three arguments
	if (argc != 3){
		fprintf(stderr,"usage: enter host address and port number\n");
		exit(1);
	}

	// Showing menu screen
	display_title();

	// Retrieving file descriptor
	int sockfd = connect_to(argv[1], atoi(argv[2]));

	while (1){

		// Retrieve text input
		char command[MAX_DATA];
		get_command(command, MAX_DATA);

		struct Reply reply;

		// Process command and check for invalid argument
		try{
			reply = process_command(sockfd, command);
			display_reply(command, reply);
		}catch (const invalid_argument &e){
			cout << e.what() << endl;
		}
		
		// Capitalizing first four characters
		touppercase(command, 4);

		// Chatmode switch if user chooses to join an existing chat room
		if (strncmp(command, "JOIN", 4) == 0 && reply.status != FAILURE_NOT_EXISTS){
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
			return 0;
		}
	}

	return 0;
}

/*
 *Connect to the server using given host and port information
 *
 *@parameter host    host address given by command line argument
 *@parameter port    port given by command line argument
 * 
 *@return socket fildescriptor
 */
int connect_to(const char *host, const int port){
	
	// Preparing sock address formatting
	int sock = 0;
	struct sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		printf("\n Socket creation error \n");
		return -1;
	}
	
	// Assigning port
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	// Convert IPv4 and IPv6 addresses from text to binary form
	// Check if successful
	if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0){
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}
	
	
	// Connect to server
	// Check if successful
	if (connect(sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		printf("\nConnection Failed \n");
		return -1;
	}

	return sock;

}

/* 
 *Send an input command to the server and return the result
 *
 *@parameter sockfd   socket file descriptor to commnunicate
 *                    with the server
 *@parameter command  command will be sent to the server
 *
 *@return    Reply    
 */
struct Reply process_command(const int sockfd, char *command){
	
	// Parsing command
	stringstream ss(std::string{command});

	string command1;
	string command2;

	ss >> command1;
	
	// If there's anything left, load it to command2
	if (ss.rdbuf()->in_avail() != 0)
	{
		ss >> command2;
	}
	
	struct Send incomingmsg;
	
	// Conversion to uppercase
	for (char &a: command1)
		a = toupper(a);
	
	// Matching inputs to their respective enums
	if (command1 == "LIST"){
		
		incomingmsg.query = LIST;
		
	}else{
		
		// Using hanging braces for readability
		if (command1 == "CREATE")
		{
			incomingmsg.query = CREATE;
		}
		else if (command1 == "DELETE")
		{
			incomingmsg.query = DELETE;
		}
		else if (command1 == "JOIN")
		{
			incomingmsg.query = JOIN;
		}
		else // If command can't be understood
		{
			throw invalid_argument("Illegal argument. Please try again.");
		}
		
		// Adding command as message
		// If no command, then throw
		if (!command2.empty())
		{
			strcpy(incomingmsg.msg, command2.c_str());
		}
		else
		{
			throw invalid_argument("Illegal argument. Please try again.");
		}
	}

	
	// Sending to server
	write(sockfd, &incomingmsg, sizeof(struct Send));
	
	// Hearing back from server
	struct Reply response;
	read(sockfd, (char*) &response, sizeof(struct Reply));

	return response;
}

/* 
 *Get into the chat mode
 * 
 *@parameter host     host address
 *@parameter port     port
 */
void process_chatmode(const char *host, const int port){
	
	
	// Connecting to chat room
	int sockaddress = connect_to(host, port);
	
	// Setting chat room to non blocking so no waiting
	int status = fcntl(sockaddress, F_SETFL, fcntl(sockaddress, F_GETFL, 0) | O_NONBLOCK);

	if (status == -1){
		errexit("fncl error: %s\n", strerror(errno));
	}
	
	// Preparing chat handling
	char buff[MAX_DATA + 1];
	char incomingbuff[sizeof(ChatActivity)];

	string chatString;

	struct Send incomingmsg;

	while (1){
		
		// Setting rfds
		fd_set rfds;
		FD_ZERO(&rfds);
	
		FD_SET(0, &rfds);
		FD_SET(sockaddress, &rfds);
		
		// Setting up select
		int retval;
		retval = select(sockaddress + 1, &rfds, NULL, NULL, NULL);

		if (retval == -1){ // Catching any errors

			errexit("Failure in select(): %s\n", strerror(errno));

		}else if (retval > 0){	
			
			// If it's recieving input (from user)
			if (FD_ISSET(0, &rfds)){	
				
				// Reading input
				int line_chars = read(0, (char*) buff, MAX_DATA);
				
				if (line_chars < 0){ // If it fails, just pull the plug
				 	errexit("> Input read error: %s\n", "Empty line");
				}
				
				// Looking through what was inputted into the buffer
				for (int i = 0; i < line_chars; i++){
					
					// Backspace handling
					if (buff[i] == '\b'){	
						
						chatString.pop_back();
					
					// Sending if enter key is hit
					}else if (buff[i] == '\n'){
						
						// From the test cases
						/*if (chatString == "Q")
						{
							printf("> Exiting the chat room");
							close(sockaddress);
							return;
						}*/
						
						// Clearing incoming.msg for safety
						memset(incomingmsg.msg, 0, MAX_DATA);
        				
        				// Appending newline to end of char array
        				char newline = '\n';
						
        				if(chatString.size() >= MAX_DATA){
        					chatString = chatString.substr(0, MAX_DATA-2);
        				}
        				
        				chatString.push_back(newline);
        				
        				// Copying chatstring to message buffer
						strncpy(incomingmsg.msg, chatString.c_str(), sizeof(incomingmsg.msg) - 1);
						
						chatString.clear(); // Clearing string

						incomingmsg.query = CHAT_MESSAGE; // Marking message as a chat

						write(sockaddress, &incomingmsg, sizeof(struct Send));
						
					}else{
						// Otherwise, add characters to string
						chatString.push_back(buff[i]);
					}
				}
			}
			
			// If client sees message
			if (FD_ISSET(sockaddress, &rfds)){	
				
				// Getting message from server
				int chatmsg = read(sockaddress, (char*) incomingbuff, sizeof(ChatActivity));
				
				// If read is 0 bytes
				if (chatmsg == 0)	{
					errexit("> Chat read error: %s\n", "Empty input");
				}
				
				// Converting to Reply object
				struct ChatActivity *reply = (struct ChatActivity *) incomingbuff;
				
				// If message, clear input buffer and display chat
				if (reply->category == MESSAGE){
					cin.clear();
					
					display_message(reply->message);
				}
				else if (reply->category = CHANNEL_DELETE){ // If request to delete room, disconnect
						printf("> Warning: the chat room is going to be closed...\n");
						close(sockaddress);
						return;
				} else {
						errexit("> Unable to parse message: %s\n", "Empty category");
				}
			}
		}
	}
}