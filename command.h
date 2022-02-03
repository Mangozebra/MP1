/*****************************************************************
* FILENAME :        Command.h 
*
*    Functions for 
*
* Version: 1.0
******************************************************************/
#ifndef COMMAND_H_
#define COMMAND_H_

#include "interface.h"
#include <string>

// Command enums
enum Command{
	CREATE,
	DELETE,
	JOIN,
    LIST,
    CHAT_MESSAGE
};

// Struct for handling outgoing messages from client
struct Send{
    enum Command query;
    char msg[MAX_DATA];
};

// Message enums
enum ChatTypes{
    MESSAGE,
    CHANNEL_DELETE
};

// Struct for handling incoming chat messages from server
struct ChatActivity{
    
    enum ChatTypes category;
    char message[MAX_DATA];
    
    void clearmsg(){
        memset(message, 0, sizeof(message));
    }
    
};

// Consider joining a pre-existing chat room (can use password)
// Welcome message

void errexit(std::string error, char* explanation){
    
    printf(error.c_str(), explanation);
    exit(EXIT_FAILURE);
    
}
#endif // COMMAND_H_
