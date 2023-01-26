#ifndef TP_AUXFRONTEND_H
#define TP_AUXFRONTEND_H
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include "util.h"

void readCommand(char command[], int fdCliReq);
char *commandList();
void look4token();
void shutDownCLient();
void writeToServ(char message[][80], int sizeCommand, int fdCLiReq);
void sendServerLoginCredentials(InfoLogin login);
int createOpenReqFifo(InfoLogin login);
void setUpCredentials(InfoLogin *login, char username[20], char password[20]);
void receiveCliLoginConfirmation();
void handleSignalSIGINT(int s);
void createAnsFifo(InfoLogin login);
void handleSignalSIGUSR1(int s, siginfo_t *sip, void *ptr);
void handleSignalSIGUSR2(int s, siginfo_t *sip, void *ptr);
void handleSignalSIGCHLD(int s);
void *sendHeartBeats(void *arg);
int receiveIntFromServ();
void receiveItemsFromServ();

char GFifoNameRequest[80];
char GFifoNameAnswer[80];
pthread_mutex_t mutexComms;
pthread_t tidHeartBeat;
bool loginAuthentication;



#endif //TP_AUXFRONTEND_H
