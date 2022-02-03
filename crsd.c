#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include <sys/types.h>
#include <sys/time.h>
#include <cerrno>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <iostream>
#include <string>
#include <map>

#include "interface.h"
#include "command.h"

using namespace std;

// Contains chat room data
class ConnectionMap{
  public:
    bool isChatRoom = true;
    vector<int> fdlist;
    int numMem = 0;
    int portNum = -1;
    string name;
    
    // Sends to all users in a chat room... Except the one in the argument
    int sendToAll (const ChatActivity& msg, int excludedsockfd) {

      for(const int& sockfd : fdlist) {
        if(sockfd != excludedsockfd){
          write(sockfd, (char *)&msg, sizeof(struct ChatActivity));
        }
      }
      
    }
};

// Links master socket file descriptors to room information
map<int, ConnectionMap> roomList;
// Maps slave sockets to their "masters"
map<int, int> parentList;

// Iterates through vector to delete all of a given element
int vectorDelete (vector <int>&vect, int deletingval){
  
  for (vector <int>::iterator iter = vect.begin(); iter != vect.end (); iter++){
    
      if (*iter == deletingval){
    	  vect.erase(iter);
    	  break;
	    }
	    
    }
    
}

// Master socket creation
int passiveTCPsock(int port, int backlog) {
  
  struct sockaddr_in sin;          // Internet endpoint address
  memset(&sin, 0, sizeof(sin));    // Zero out address
  sin.sin_family      = AF_INET;  
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(port);      // Setting the port
  
  
  // Creating socket
  // Editing parameters and making sure to catch any errors
  int s = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
  if (s < 0) errexit("Can’t create socket: %s\n", strerror(errno));
  
  int flag = 1;
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int)) < 0) // Getting rid of time limit
    errexit("Setsockopt(SO_REUSEADDR) failed", strerror(errno));
  
  if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
       errexit("Can’t bind to…%s\n", strerror(errno));
  
  listen(s, backlog);
  
  return s;
}

// Retrieving port number from socket
int getSockPort(int fd_num){
  
  // Setting up the socket address info
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  
  socklen_t lengthOfSock = sizeof(sin);
  
  // Getting socket name associated with the file descriptor
  if(getsockname(fd_num, (struct sockaddr *)&sin, &lengthOfSock) < 0)
    throw runtime_error("Get Sock Name Failure");
  
  // Returning the port
  return ntohs(sin.sin_port);
  
}

int main(int argc, char * argv[]) {
  
  // Setting up the socket address info
  struct sockaddr_in fsin;
  memset(&fsin, 0, sizeof(fsin));
  
  socklen_t clientaddrlen = sizeof(fsin);

  int port = atoi(argv[1]); // Service name or port number 
  int m_sock, s_sock;      // Master and slave socket
  
  // Creating the first master socket
  m_sock = passiveTCPsock(port, 32);
  
  // Creating chat room instance for the "control room"
  ConnectionMap controlRoom;
  controlRoom.isChatRoom = false;
  
  // Loading it on to the room list
  roomList[m_sock] = controlRoom;
  
  // Initializing rfd list
  fd_set rfds;
  // Setting up a variable for max fd
  int max_fd;
  
  for (;;) {
    
    // Preparing rd list for populating
    FD_ZERO(&rfds);
    int retval;
    max_fd = 0;
    
    // Populating rfds
    // Making sure we get the maximum file descriptor
    for(const auto& p : roomList){
      
      if (p.first > max_fd)
        max_fd = p.first;
      
      FD_SET(p.first, &rfds);
      
      for(const int& s_sock : p.second.fdlist){
        if (s_sock > max_fd) max_fd = s_sock;
        FD_SET(s_sock, &rfds);
      }
      
    }
    
    // Setting up select
    retval = select(max_fd + 1, &rfds, NULL, NULL, NULL);
    
    // Throwing error if select fails
    if (retval == -1){
      if(errno == EINTR) continue;
      errexit("Failure in select(): %s\n", strerror(errno));
    }
    
    // Looking through master sockets
    for(auto& p : roomList){
      
      // If recieving a connection request
      if(FD_ISSET(p.first, &rfds)){

        s_sock = accept4(p.first, (struct sockaddr*)&fsin, &clientaddrlen, SOCK_NONBLOCK);

        if(s_sock < 0) {
          switch(errno){
            //case EAGAIN: // Considered to be the same as EWOULDBLOCK
            case EINTR:
            case EWOULDBLOCK:
              break;
            default:
              errexit("Failure in accept(): %s\n", strerror(errno));
          }
          
        } else { // Add slave socket to room
          
          p.second.fdlist.push_back(s_sock);
          p.second.numMem += 1;
          parentList[s_sock] = p.first;
          
        }
      }
      
    }
    
    // Check for messages on slave sockets
    for(auto& p : parentList) {
      
      // Handling incoming client messages and disconnections
      if(FD_ISSET(p.first, &rfds)){
        
        struct Send incomingmsg;
        struct ChatActivity usermsg;

        // If receiving an empty read, consider it to be a disconecction
        read(p.first, &incomingmsg, sizeof(struct Send));

        
        if(read(p.first, &incomingmsg, sizeof(struct Send)) == 0){
          
          vectorDelete(roomList[p.second].fdlist, p.first);
          roomList[parentList[p.first]].numMem -= 1;
          parentList.erase(p.first);
          
          continue;
        }
        

        // In the case that the incoming message is a chat message
        if(incomingmsg.query == CHAT_MESSAGE){
          
          // Look up the parent of the requesting socket (chat room)
          // Transfer incoming message to outgoing
          usermsg.category = MESSAGE;
          strcpy(usermsg.message, incomingmsg.msg);
          
          // Getting master socket
          m_sock = parentList[p.first];
          
          // Sending message to all other clients
          roomList[m_sock].sendToAll(usermsg, p.first);
          usermsg.clearmsg();
        
        // In the case that the incoming message is a command
        } else {
          
          // Setting up struct to send back to clients
          struct Reply response;
          memset(&response, 0, sizeof(struct Reply));
          
          // Variables declared outside of switch otherwise compiler gets mad
          string listToSend = "";
          vector<string> roomNameList;
          bool isEqual = false;
          
          switch(incomingmsg.query) {
  				  
  				  case CREATE:
				      
				      // Seeing if room already exists 
				      for (auto& it : roomList) {
				        
                  if (it.second.name == incomingmsg.msg) {
                      isEqual = true;
                  }
                  
              }
              
              if(isEqual){
                response.status = FAILURE_ALREADY_EXISTS;
                break;
              }
				      
				      // If everything works well, create room socket
				      m_sock = passiveTCPsock(0, 32);
				      
				      try {
				        // Setting up chat room
				        port = getSockPort(m_sock); // Making sure it doesn't go wrong
					      ConnectionMap createdSocket;
                createdSocket.isChatRoom = true;
                createdSocket.portNum = port;
                createdSocket.name = incomingmsg.msg;
                
                roomList.insert(pair<int,ConnectionMap>(m_sock,createdSocket));
                
					      response.status = SUCCESS;
					      
				      } catch(const runtime_error& e) {
				        
				        cout << e.what() << endl;
				        response.status = FAILURE_UNKNOWN;
				        
				      }
				      
				      break;
				    case DELETE:
				    
				      // Checking for duplicates
				      m_sock = -1;
				      
				      for (auto& it : roomList) {
 
                  if (it.second.name == incomingmsg.msg) {
                      m_sock = it.first;
                      break;
                  }
                  
              }
              
              // If there is a duplicate...
              if(m_sock == -1){
                response.status = FAILURE_NOT_EXISTS;
              }else{
               
                // Send close requests to clients
                usermsg.category = CHANNEL_DELETE;
                roomList[m_sock].sendToAll(usermsg, p.first);  // Since client isn't in room, no worries
                
                // Clearing all clients from fdlist
                for(const int& sockfd : roomList[m_sock].fdlist) {
                  FD_CLR(sockfd, &rfds);
                }
                
                // Taking care of the room
                roomList.erase(m_sock);
                close(m_sock);
                
                response.status = SUCCESS;
              }
				      break;
				   case JOIN:
              
              // Once again, checking if room exists
		          m_sock = -1;
              
				      for (auto& it : roomList) {
 
                  if (it.second.name == incomingmsg.msg) {
                      m_sock = it.first;
                      break;
                  }
                  
              }
              
              if(m_sock == -1){
                response.status = FAILURE_NOT_EXISTS;
                break;
              }else{
                response.status = SUCCESS;
                response.port = roomList[m_sock].portNum;
                response.num_member = roomList[m_sock].numMem;
              }
              break;
				    case LIST:
				      
				      // Loading room names to a vector
				      // ...So the order matches the test cases
              for(const auto& it : roomList){
                if(it.second.isChatRoom){
                  roomNameList.push_back(it.second.name);
                }
                
              }
              
              // Reversing vector
              reverse(roomNameList.begin(), roomNameList.end());
              for(string it : roomNameList){
                  listToSend += it;
                  listToSend += ",";
              }
              
              // If nothing, that means room list is empty
              if(listToSend.length() == 0){
                listToSend = "empty";
              }
              
              // If the list is too long, cut it off
              // It doesn't seem expected of me to do anything else, so I won't do more than I have to
              strncat(response.list_room, listToSend.c_str(), MAX_DATA - 1);
              
              response.status = SUCCESS;
              
              // Not necessary, but doing it for safety
              listToSend.clear();
              
              break;
				    default: // If it got gibberish somehow, return this
				      response.status = FAILURE_UNKNOWN;
          }
           
           // Sending instructions to client!
           write(p.first, &response, sizeof(struct Reply));
           
        }
      }
    }
  }
}