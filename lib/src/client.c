/**
 * @file client.c
 * @authors Romain FREZIER
 * @authors Etienne TILLIER
 * @brief Client functions implementation
 * @version 0.1
 * @date 2022-05-26
 *
 * @copyright Copyright (c) 2022
 *
 */
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <regex.h>

#include "../headers/client.h"
#include "../headers/colors.h"
#include "../headers/commandClient.h"
#include "../headers/stringFunc.h"
#include "../headers/tools.h"

/**
 * @brief Maximum size of a message
 */
#define MAX 100

/**
 * @brief socket descriptor for the connexion between client and server
 */
int dS;

/**
 * @brief Ip address of the server
 */
char *ipAddress;

/**
 * @brief Port for sending file
 */
int portSendingFile;

/**
 * @brief Thread for send messages
 */
pthread_t sendThread;

/**
 * @brief Thread for receive messages
 */
pthread_t receiveThread;

void launchClient(char *ip, int port)
{
  greenMessage("Start program\n");

  ipAddress = ip;
  dS = socketConnection(port);

  // check the ^C
  signal(SIGINT, signalHandler);
  // check the end of the terminal
  signal(SIGTERM, signalHandler);

  // Checking username
  int check;
  do
  {
    // Connection message size reception
    u_long sizeCoMsg;
    int receive = recv(dS, &sizeCoMsg, sizeof(u_long), 0);
    if (receive == -1)
    {
      redErrorMessage("Error message size received\n");
    }
    else if (receive == 0)
    {
      redErrorMessage("\nServer shutdown now !\n");
    }
    // Message reception
    char *isConnected = (char *)malloc(sizeCoMsg * sizeof(char));
    if (recv(dS, isConnected, sizeCoMsg * sizeof(char), 0) == -1)
    {
      redErrorMessage("An error appeared during connection to the server...\n");
    }

    check = strcmp(isConnected, "Connected");
    char *arr[2];
    if (check != 0)
    {
      char *username = (char *)malloc(sizeof(char) * 50);
      do{
          blueMessage("\nUsername must have only letters, numbers, or . -\nUsername can have a length of 1 to 50 characters\n");
          printf("\n%s\n", isConnected);
          fgets(username, 50, stdin);
          username[strcspn(username, "\n")] = 0;
      } while(regex(username, "^ *([a-zA-Z0-9\\.-]+) *$") != 0);

      getRegexGroup(arr, 2, username, "^ *([a-zA-Z0-9\\.-]+) *$");
      sendSpecificMessage(dS, arr[1]);
    }
    else
    {
      greenMessage(isConnected);
      greenMessage(" as : ");
      blueMessage(arr[1]);
      printf("\n\n");
    }
  } while (check != 0);

  while(1){
    // Create message threads
    pthread_create(&sendThread, NULL, sendMessage, (void *)(size_t)dS);
    pthread_create(&receiveThread, NULL, receiveMessage, (void *)(size_t)dS);
    pthread_join(receiveThread, 0);
    pthread_cancel(sendThread);
  }

  // Waiting for the end of threads
  pthread_join(sendThread, 0);

  // Shutdown of socket
  shutdown(dS, 2);
  printf("End program\n");
}

void *sendMessage(void *socket)
{
  char *m = (char *)malloc(MAX * sizeof(char));

  while (strcmp(m, "/quit") != 0)
  {
    // Client input
    printf("Enter your message (100 max) : \033[0;32m");
    fgets(m, 100, stdin);
    m[strcspn(m, "\n")] = 0;
    reset();
    // check user given command
    checkCommand(m, ipAddress, portSendingFile, (int)(size_t)socket);
  }
  shutdown((int)(size_t)socket, 2);
  free(m);
  exit(0);
}

void *receiveMessage(void *socket)
{
  char *m = (char *)malloc(MAX * sizeof(char));
  while (1)
  {
    // Size reception
    u_long sizeMessage;
    int receive = recv((int)(size_t)socket, &sizeMessage, sizeof(u_long), 0);
    if (receive == -1)
    {
      redErrorMessage("Error message size received\n");
    }
    else if (receive == 0)
    {
      redErrorMessage("Server shutdown now !\n");
    }

    // Message reception
    char *messageReceive = (char *)malloc(sizeMessage * sizeof(char));
    if (recv((int)(size_t)socket, messageReceive, sizeMessage * sizeof(char), 0) == -1)
    {
      redErrorMessage("Error message received\n");
    }

    // check if the client need to quit the chat server
    if (strcmp(messageReceive, "/quit") == 0)
    {
      printf("/quit server received \n");
      quitForUser(dS);
    }

    // check if the message received is a pm
    int resRegexPm = regex(messageReceive, "(pm).*");
    // check if the message received is a broadcast
    int resRegexAll = regex(messageReceive, "(ALL).*");
    int resRegexJChannel = regex(messageReceive, "^\\/jchannel +([0-9]{1,2}) +([0-9]{4}) *$");

    if (resRegexPm == 0)
    {
      printf("\n");
      purpleMessage(messageReceive);
      printf("\n");
    }
    else if (resRegexAll == 0){
      printf("\n");
      redMessage(messageReceive);
      printf("\n");
    }
    else if (resRegexJChannel == 0)
    {
      char *msg[3];
      getRegexGroup(msg, 3, messageReceive, "^\\/jchannel +([0-9]{1,2}) +([0-9]{4}) *$");
      int port = atoi(msg[2]);
      shutdown((int)(size_t)socket, 2);
      redMessage("\nYou left your channel for channel ");
      redMessage(msg[1]);
      printf("\n");
      dS = socketConnection(port);
      break;
    }
    else
    {
      printf("\n");
      blueMessage(messageReceive);
      printf("\n");
    }
    printf("Enter your message (100 max) : \033[0;32m\n");
  }
  free(m);
  return NULL;
}

void signalHandler(int n)
{
  quitForUser(dS);
}

int socketConnection(int port)
{
  int newdS = socket(PF_INET, SOCK_STREAM, 0);
  int enable = 1;
  if (newdS < 0)
  {
    redErrorMessage("ERROR opening socket for sending file");
  }
  if (setsockopt(newdS, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
  {
    redErrorMessage("setsockopt(SO_REUSEADDR) failed");
  }

  // info for sending file socket
  portSendingFile = port + 1;

  // fill info for socket dS
  struct sockaddr_in aS;
  aS.sin_family = AF_INET;
  inet_pton(AF_INET, ipAddress, &(aS.sin_addr));
  aS.sin_port = htons(port);
  socklen_t lgA = sizeof(struct sockaddr_in);

  // connection to socket newdS
  connect(newdS, (struct sockaddr *)&aS, lgA);
  return newdS;
}