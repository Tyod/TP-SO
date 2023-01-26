#ifndef TP_AUXBACKEND_H
#define TP_AUXBACKEND_H

#include "util.h"
#include "users_lib.h"

#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <stdbool.h>

typedef struct {
    pthread_t tPid;
    char username[20];
    int pid;
    char clififoRequest[80];
    char clififoAnswer[80];
    int timeOut;
} CliData;

typedef struct {
    int pid;
    char promName[80];
} PromData;

typedef struct {
    CliData *listCli;
    int sizeList;
} InfoClis;

typedef struct {
    Leilao *listLeiloes;
    int sizeList;
} InfoLeiloes;

typedef struct {
    PromData *promotorsList;
    int sizeList;
    bool stopAllProms;
} InfoProms;

typedef struct {
    InfoClis infoClis; //Client Information Struct
    InfoLeiloes infoLeiloes; //Struct to save Auctions
    InfoProms infoProms; //Struct to save Promoters
    pthread_t funcThreads[3]; //Functionalities Threads array [0]-TimerThread [1]-Promoters Thread [2]-ClientAcceptorThread
    pthread_mutex_t mutexTimer;
    pthread_mutex_t mutexCliInfo;
    pthread_mutex_t mutexItemInfo;
    int time;
}InfoBackend;

void getToken();
void releaseToken();
char *commandList();
void readCommand(char command[], InfoBackend *infobackend);
void loadItems(InfoLeiloes *infoLeiloes);
void showItems(InfoLeiloes infoLeiloes);
void loadPromoters(InfoProms *infoProms);
void *runProm(void* arg);
int loadUsers();
void deleteServFifo();
void *clientAcceptor(void *arg);
void *startClock(void *arg);
void addUser(InfoClis *infoClis, CliData newUser);
void removeUser(InfoClis *infoClis, char username[]);
void *clientAttendant(void *arg);
void handleSignalSIGUSR1(int s);
void shutDownServer(InfoClis *infoClis, InfoLeiloes *infoLeiloes, InfoProms *infoProms, pthread_t funcThreads[], pthread_mutex_t *mutexTimer, pthread_mutex_t *mutexCliInfo,
                    pthread_mutex_t *mutexItemInfo, int *time);
void removePromoter(InfoProms *infoProms, char promName[], pthread_t promThreadPid);
void showUsers(InfoClis infoClis);
void notifyItemSaleStatus(InfoClis *infoClis, int id, bool sold);
void notifyItemPromotionStatus(InfoClis *infoClis, int id, bool onPromotion);
void handleSignalSIGUSR2(int s);
void sendTimeUser(char threadClififoAnswer[], pthread_mutex_t *mutexTimer, int *time);
void sendCashUser(char threadClififoAnswer[], pthread_mutex_t *mutexCliInfo, char username[]);
void addCashUser(pthread_mutex_t *mutexCliInfo, char username[], char cash[]);
void sendListItemUser(char threadClififoAnswer[], Leilao temp[]);
void get_Send_ItemsNonSold(char threadClififoAnswer[], InfoLeiloes infoLeiloes);
void get_Send_ItemsCategory(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char category[]);
void get_Send_ItemsSeller(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char seller[]);
void get_Send_ItemsValue(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char price[]);
void get_Send_ItemsDuration(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char time[]);
void decrementTimeItem(InfoLeiloes *infoLeiloes, InfoClis *infoClis);
void tryBuyItem(InfoLeiloes *infoLeiloes, InfoClis *infoClis, char username[], char idItem[], char valueGiven[]);
void checkItemSold(InfoLeiloes *infoLeiloes, pthread_mutex_t *mutexCLiInfo);
void addItemForSale(InfoLeiloes *infoLeiloes, char descricao[], char categoria[], char valorBase[], char valorInst[], char tempo[], char usernameSeller[]);
void checkSaleEnd(InfoLeiloes *infoLeiloes, InfoClis *infoCLis);
void get_Send_ItemsSale(char threadClififoAnswer[80], InfoLeiloes infoLeiloes);
void get_Send_ItemsSold(char threadClififoAnswer[80], InfoLeiloes infoLeiloes);
void checkUserTimeout(InfoClis *infoClis, pthread_mutex_t *mutexCliInfo);
void refreshTimeoutUser(InfoClis *infoCLis, char username[]);
bool checkUserAlreadyLogged(InfoClis *infoClis, char username[80]);
void saveCurrentTime(int time);
void saveItemsFile(InfoLeiloes infoLeiloes);
void showProms(InfoProms infoProms);
void relauch_promoters(InfoBackend *backend);
void setItemOnSale(InfoLeiloes *infoLeiloes, InfoClis *infoClis, char sale[]);


#endif //TP_AUXBACKEND_H
