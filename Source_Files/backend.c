#include "../Header_Files/auxbackend.h"

bool checkUserExists(InfoClis *infoClis, char *username);

bool checkPromExists(InfoProms *infoProms, char *promName);

int main(int argc, char *argv[]) {

    //Arguments Verification
    if (argc != 1) {
        printf("[ERROR SYNTAX] - ./backend");
        return -1;
    }

    //Get Token (Create File)
    getToken();

    struct sigaction sa1, sa2;
    sa1.sa_handler = &handleSignalSIGUSR1;
    sa2.sa_handler = &handleSignalSIGUSR2;

    sigaction(SIGUSR1, &sa1, NULL);
    sigaction(SIGUSR2, &sa2, NULL);

    //Data Structs
    InfoBackend backend;
    backend.infoLeiloes.sizeList = 0;
    backend.infoProms.sizeList = 0;
    backend.infoClis.sizeList = 0;
    pthread_mutex_init(&backend.mutexTimer, NULL);
    pthread_mutex_init(&backend.mutexCliInfo, NULL);
    pthread_mutex_init(&backend.mutexItemInfo, NULL);

    //Load possible existing Items
    loadItems(&backend.infoLeiloes);

    //Load Promoters
    loadPromoters(&backend.infoProms);

    //load Users
    int numUsers = loadUsers();

    //Create Timer Thread
    pthread_create(&backend.funcThreads[0], NULL, &startClock, (void *) &backend);

    //Create Promoters Thread
    if (backend.infoProms.sizeList != 0)
        pthread_create(&backend.funcThreads[1], NULL, &runProm, (void *) &backend);

    //Create and launch Thread to Accept Clients
    pthread_create(&backend.funcThreads[2], NULL, &clientAcceptor, (void *) &backend);

    //Reading Commands
    char command[80];
    do {
        //Escolhe o commando
        char *tempCommand = commandList();
        if (tempCommand != NULL) {
            //Copia para uma variavel local de ponteiro para poder libertar a memória alocada
            strcpy(command, tempCommand);
            free(tempCommand);
        }

        //Lê o commando
        readCommand(command, &backend);
    } while (strcmp("close\n", command) != 0);

    shutDownServer(&backend.infoClis, &backend.infoLeiloes, &backend.infoProms, backend.funcThreads, &backend.mutexTimer, &backend.mutexCliInfo, &backend.mutexItemInfo, &backend.time);
}

void handleSignalSIGUSR2(int s) {

}

void shutDownServer(InfoClis *infoClis, InfoLeiloes *infoLeiloes, InfoProms *infoProms, pthread_t funcThreads[], pthread_mutex_t *mutexTimer, pthread_mutex_t *mutexCliInfo,
                    pthread_mutex_t *mutexItemInfo, int *time) {

    //Kill FuncThreads
    pthread_kill(funcThreads[0], SIGUSR1);
    if (infoProms->sizeList != 0) {
        infoProms->stopAllProms = true;
        pthread_kill(funcThreads[1], SIGUSR2);
    }
    pthread_kill(funcThreads[2], SIGUSR1);

    //Kill all client and clientThreads
    for (int i = 0; i < infoClis->sizeList; i++) {
        kill(infoClis->listCli[i].pid, SIGINT);
        pthread_kill(infoClis->listCli[i].tPid, SIGUSR1);
    }

    //Wait funcThreads
    for (int i = 0; i < infoClis->sizeList; i++) {
        pthread_join(infoClis->listCli[i].tPid, NULL);
    }

    //Wait funcThreads
    pthread_join(funcThreads[0], NULL);
    if (infoProms->sizeList != 0)
        pthread_join(funcThreads[1], NULL);
    pthread_join(funcThreads[2], NULL);

    //Save Items
    saveItemsFile(*infoLeiloes);

    //Set all fields to Zero to prevent overflow of variables (segmentation fault)
    memset(infoClis->listCli, 0, sizeof(CliData) * infoClis->sizeList);
    memset(infoLeiloes->listLeiloes, 0, sizeof(Leilao) * infoLeiloes->sizeList);
    memset(infoProms->promotorsList, 0, sizeof(PromData) * infoProms->sizeList);

    //Free memory of allocated arrays
    if (infoClis->sizeList != 0)
        free(infoClis->listCli);
    if (infoLeiloes->sizeList != 0)
        free(infoLeiloes->listLeiloes);
    if (infoProms->sizeList != 0)
        free(infoProms->promotorsList);

    //Destroy Mutexs
    pthread_mutex_destroy(mutexTimer);
    pthread_mutex_destroy(mutexCliInfo);
    pthread_mutex_destroy(mutexItemInfo);

    //Deletes servFifo
    deleteServFifo();

    //Releases token (Delete File)
    releaseToken();

    //Save Users Data
    char *nameFileUsers = getenv("FUSERS");
    saveUsersFile(nameFileUsers);

    //Save Time
    saveCurrentTime(*time);

    printf("Shutting down Server!\n");
}

void saveItemsFile(InfoLeiloes infoLeiloes) {
    char *nameFileItems = getenv("FITEMS");
    if(nameFileItems != NULL){
        FILE *fd = fopen(nameFileItems, "w");
        if(fd == NULL){
            printf("[Error Couldn't Open items File, in order to save them!]");
            exit(0);
        }

        int j=0;
        for(int i=0; i<infoLeiloes.sizeList; i++){
            if(infoLeiloes.listLeiloes[i].item.sold == false) {
                fprintf(fd, "%d %s %s %d %d %d %s %s\n", j, infoLeiloes.listLeiloes[i].item.descricao, infoLeiloes.listLeiloes[i].item.categoria,
                        infoLeiloes.listLeiloes[i].item.backUpPriceBase, infoLeiloes.listLeiloes[i].item.backUpPriceInst, infoLeiloes.listLeiloes[i].tempo,
                        infoLeiloes.listLeiloes[i].item.usernameSeller, infoLeiloes.listLeiloes[i].item.usernameBuyer);
                j++;
            }
        }

        fclose(fd);
    }
}

void saveCurrentTime(int time) {
    char *nameFileProms = getenv("FITIME");

    if(nameFileProms != NULL) {
        FILE  *fd = fopen(nameFileProms, "w");
        if(fd == NULL){
            printf("[ERROR Couldn't open file containing time, in order to save current time!]\n");
            exit(0);
        }

        fprintf(fd, "%d", time);
        fclose(fd);
    }
}

void handleSignalSIGUSR1(int s) {
    printf("Thread Killed!\n");
    //write(1, "Thread Killed!\n", sizeof(char)* strlen("Thread Killed!\n"));
    pthread_exit((void *) 1);
}

void removePromoter(InfoProms *infoProms, char promName[], pthread_t promThreadPid) {
    bool promfound = false;
    int j = 0;
    PromData *temp;
    int pid2kill;

    if(checkPromExists(infoProms, promName)){

        if (infoProms->sizeList - 1 != 0)
            temp = malloc(sizeof(PromData) * (infoProms->sizeList - 1));

        for (int i = 0; i < infoProms->sizeList; i++) {
            if (strcmp(promName, infoProms->promotorsList[i].promName) == 0) {

                if ((infoProms->sizeList - 1) == 0) {
                    infoProms->stopAllProms = true;
                    pthread_kill(promThreadPid, SIGUSR2);
                }

                pid2kill = infoProms->promotorsList[i].pid;
            } else {
                if (infoProms->sizeList - 1 != 0)
                    memcpy(&temp[j], &infoProms->promotorsList[i], sizeof(PromData) * 1);
                j++;
            }
        }

        free(infoProms->promotorsList);
        infoProms->promotorsList = temp;
        (infoProms->sizeList)--;
        kill(pid2kill, SIGUSR1);
        waitpid(pid2kill, NULL, 0);
    }else
        printf("Given Promoter wasn't found\n");

}

bool checkPromExists(InfoProms *infoProms, char *promName) {
    for(int i=0; i<infoProms->sizeList; i++){
        if (strcmp(promName, infoProms->promotorsList[i].promName) == 0)
            return true;
    }
    return false;
}

void removeUser(InfoClis *infoClis, char username[]) {

    bool userfound = false;
    int j = 0, userPid;
    pthread_t userTpid;
    CliData *temp;

   if(checkUserExists(infoClis, username)){
       if (infoClis->sizeList - 1 != 0)
           temp = malloc(sizeof(CliData) * (infoClis->sizeList - 1));

       for (int i = 0; i < infoClis->sizeList; i++) {
           if (strcmp(username, infoClis->listCli[i].username) == 0) {
               userPid = infoClis->listCli[i].pid;
               userTpid = infoClis->listCli[i].tPid;
               userfound = true;
           } else {
               if (infoClis->sizeList - 1 != 0)
                   memcpy(&temp[j], &infoClis->listCli[i], sizeof(CliData) * 1);
               j++;
           }
       }

       free(infoClis->listCli);
       infoClis->listCli = temp;
       infoClis->sizeList--;
       kill(userPid, SIGINT);
       pthread_kill(userTpid, SIGUSR1);
       pthread_join(userTpid, NULL);
       printf("User Removed\n");

   }else{
       printf("Given user wasn't found\n");
   }
}

bool checkUserExists(InfoClis *infoClis, char username[]) {
    for(int i=0; i<infoClis->sizeList; i++){
        if (strcmp(username, infoClis->listCli[i].username) == 0)
            return true;
    }

    return false;
}

void addUser(InfoClis *infoClis, CliData newUser) {
    if (infoClis->sizeList == 0) {
        infoClis->listCli = malloc(sizeof(CliData) * (infoClis->sizeList + 1));
    } else {
        infoClis->listCli = realloc(infoClis->listCli, sizeof(CliData) * (infoClis->sizeList + 1));
    }

    newUser.timeOut = 0;
    infoClis->listCli[infoClis->sizeList] = newUser;
    (infoClis->sizeList)++;
}

int create_OpenServFifo() {
    //Create servFifo
    if (mkfifo("servFifo", 0777) == -1) {
        printf("Couldn't Create 'servFifo' pipe!\n");
        exit(-1);
    }

    //Open servFifo
    int fdServ;
    if ((fdServ = open("servFifo", O_RDWR)) == -1) {
        printf("Couldn't Open 'servFifo' pipe!\n");
        deleteServFifo();
        exit(-1);
    }

    return fdServ;
}


void *startClock(void *arg) {

    InfoBackend *backend = (InfoBackend *) arg;

    char *nameFileProms = getenv("FITIME");

    if(nameFileProms != NULL){
        FILE  *fd = fopen(nameFileProms, "r");
        if(fd == NULL){
            printf("[ERROR Couldn't open file containing time!]\n");
            exit(0);
        }

        //Read Time Contained in File
        fscanf(fd, "%d", &backend->time);
        fclose(fd);
    }


    while (1) {
        //Increment Timer
        pthread_mutex_lock(&backend->mutexTimer);
        backend->time = backend->time + 1;
        pthread_mutex_unlock(&backend->mutexTimer);


        pthread_mutex_lock(&backend->mutexItemInfo);

        //Decrement remaining time
        decrementTimeItem(&backend->infoLeiloes, &backend->infoClis);
        //Check if item's time has expired
        checkItemSold(&backend->infoLeiloes, &backend->mutexCliInfo);
        //Check if item's sale time has expired
        checkSaleEnd(&backend->infoLeiloes, &backend->infoClis);
        //Check User TimeOut
        checkUserTimeout(&backend->infoClis, &backend->mutexCliInfo);

        pthread_mutex_unlock(&backend->mutexItemInfo);

        sleep(1);
    }
}

void checkUserTimeout(InfoClis *infoClis, pthread_mutex_t *mutexCliInfo) {
    char tempUsername[80];
    pthread_mutex_lock(mutexCliInfo);
    for(int i=0; i<infoClis->sizeList; i++){
        (infoClis->listCli[i].timeOut)++;

        if(infoClis->listCli[i].timeOut == 15) {
            strcpy(tempUsername, infoClis->listCli[i].username);
            removeUser(infoClis, infoClis->listCli[i].username);
            printf("\nThe user %s has timedout!\n", tempUsername);
        }
    }
    pthread_mutex_unlock(mutexCliInfo);
}

void checkSaleEnd(InfoLeiloes *infoLeiloes, InfoClis *infoCLis) {
    for (int i = 0; i < infoLeiloes->sizeList; i++) {
        if (infoLeiloes->listLeiloes[i].item.sold == false && infoLeiloes->listLeiloes[i].item.onSale == true) {
            if (infoLeiloes->listLeiloes[i].item.timePromotion == 0) {
                infoLeiloes->listLeiloes[i].item.onSale = false;
                infoLeiloes->listLeiloes[i].item.valorInst = infoLeiloes->listLeiloes[i].item.backUpPriceInst;
                notifyItemPromotionStatus(infoCLis, (infoLeiloes->listLeiloes[i].item.id) * (-1), false);
            } else {
                (infoLeiloes->listLeiloes[i].item.timePromotion)--;
            }
        }
    }
}

void checkItemSold(InfoLeiloes *infoLeiloes, pthread_mutex_t *mutexCLiInfo) {

    for (int i = 0; i < infoLeiloes->sizeList; i++) {
        if (infoLeiloes->listLeiloes[i].tempo == 0 && strcmp(infoLeiloes->listLeiloes[i].item.usernamePresentBuyer, "none") != 0 &&
            strcmp(infoLeiloes->listLeiloes[i].item.usernameBuyer, "-") == 0) {
            pthread_mutex_lock(mutexCLiInfo);
            strcpy(infoLeiloes->listLeiloes[i].item.usernameBuyer, infoLeiloes->listLeiloes[i].item.usernamePresentBuyer);
            updateUserBalance(infoLeiloes->listLeiloes[i].item.usernamePresentBuyer,
                              (getUserBalance(infoLeiloes->listLeiloes[i].item.usernamePresentBuyer) - infoLeiloes->listLeiloes[i].item.valorBase));
            pthread_mutex_unlock(mutexCLiInfo);
        }
    }
}

void decrementTimeItem(InfoLeiloes *infoLeiloes, InfoClis *infoClis) {
    for (int i = 0; i < infoLeiloes->sizeList; i++) {
        if (!infoLeiloes->listLeiloes[i].item.sold) {
            (infoLeiloes->listLeiloes[i].tempo)--;

            if (infoLeiloes->listLeiloes[i].tempo == 0 && infoLeiloes->listLeiloes[i].item.sold == false) {
                infoLeiloes->listLeiloes[i].item.sold = true;
                notifyItemSaleStatus(infoClis, (infoLeiloes->listLeiloes[i].item.id) * (-1), true);
            }
        }
    }
}


void *clientAcceptor(void *arg) {
    InfoBackend *backend = (InfoBackend *) arg;

    InfoLogin templogin;
    CliData tempCli;
    bool userIsLogged = false;
    int fdServ = create_OpenServFifo();

    printf("\n\nStarted Accepting CLients \n\n");
    while (true) {
        int fd;
        //Receive New User
        if (read(fdServ, &templogin, sizeof(InfoLogin)) != 1) {
            fd = open(templogin.clififoAnswer, O_RDWR);
            if (isUserValid(templogin.name, templogin.password) == 1) {

                //Checks if already logged
                pthread_mutex_lock(&backend->mutexCliInfo);
                userIsLogged = checkUserAlreadyLogged(&backend->infoClis, templogin.name);
                pthread_mutex_unlock(&backend->mutexCliInfo);

                if(userIsLogged == false) {
                    write(fd, "1", sizeof(char));
                    strcpy(tempCli.clififoRequest, templogin.clififoRequest);
                    strcpy(tempCli.clififoAnswer, templogin.clififoAnswer);
                    strcpy(tempCli.username, templogin.name);
                    tempCli.pid = templogin.pid;
                    addUser(&backend->infoClis, tempCli);
                    pthread_create(&backend->infoClis.listCli[backend->infoClis.sizeList - 1].tPid, NULL, &clientAttendant, (void *) backend);
                }else
                    write(fd, "2", sizeof(char));
            } else {
                write(fd, "0", sizeof(char));
            }
        } else {
            printf("There was an error reading from 'servFifo' pipe!\n");
            pthread_exit((void *) -1);
        }
    }
}

bool checkUserAlreadyLogged(InfoClis *infoClis, char username[80]) {
   for(int i=0; i<infoClis->sizeList; i++){
       if(strcmp(infoClis->listCli[i].username, username) == 0)
           return true;
   }

    return false;
}

void *clientAttendant(void *arg) {
    InfoBackend *backend = (InfoBackend *) arg;

    Message tempMessage;
    int fdA, fdR;

    //User Vars in current Thread
    char threadUsername[20];
    char threadClififoRequest[80];
    char threadClififoAnswer[80];

    strcpy(threadUsername, backend->infoClis.listCli[backend->infoClis.sizeList - 1].username);
    strcpy(threadClififoRequest, backend->infoClis.listCli[backend->infoClis.sizeList - 1].clififoRequest);
    strcpy(threadClififoAnswer, backend->infoClis.listCli[backend->infoClis.sizeList - 1].clififoAnswer);

    printf("Started Thread of user '%s' with fifos  '%s' '%s'!\n", threadUsername, threadClififoRequest, threadClififoAnswer);

    //Open Request and anwser Fifo
    if ((fdR = open(threadClififoRequest, O_RDWR)) == -1) {
        printf("Couldn't Open fifo Request!");
        pthread_exit((void *) -1);
    }


    while (1) {
        if (read(fdR, &tempMessage, sizeof(Message)) == -1) {
            printf("Couldn't Read fifo!");
        } else {
            if (strcmp(tempMessage.command[0], "exit") == 0) {
                printf("Shutting down Thread! '%s'\n", threadUsername);
                removeUser(&backend->infoClis, threadUsername);
                continue;
            }

            if (strcmp(tempMessage.command[0], "sell") == 0) {
                printf("Command sell '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                pthread_mutex_lock(&backend->mutexCliInfo);
                addItemForSale(&backend->infoLeiloes, tempMessage.command[1], tempMessage.command[2], tempMessage.command[3], tempMessage.command[4], tempMessage.command[5],
                               threadUsername);
                notifyItemSaleStatus(&backend->infoClis, backend->infoLeiloes.listLeiloes[backend->infoLeiloes.sizeList - 1].item.id, false);
                pthread_mutex_unlock(&backend->mutexCliInfo);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }


            if (strcmp(tempMessage.command[0], "list") == 0) {
                printf("Command list '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsNonSold(threadClififoAnswer, backend->infoLeiloes);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }

            if(strcmp(tempMessage.command[0], "liprom")==0){
                printf("command liprom '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsSale(threadClififoAnswer, backend->infoLeiloes);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }

            if(strcmp(tempMessage.command[0], "lisold")==0){
                printf("command lisold '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsSold(threadClififoAnswer, backend->infoLeiloes);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }

            if (strcmp(tempMessage.command[0], "licat") == 0) {
                printf("Command licat '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsCategory(threadClififoAnswer, backend->infoLeiloes, tempMessage.command[1]);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }


            if (strcmp(tempMessage.command[0], "lisel") == 0) {
                printf("Command lisel '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsSeller(threadClififoAnswer, backend->infoLeiloes, tempMessage.command[1]);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }


            if (strcmp(tempMessage.command[0], "lival") == 0) {
                printf("Command lival '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsValue(threadClififoAnswer, backend->infoLeiloes, tempMessage.command[1]);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }


            if (strcmp(tempMessage.command[0], "litime") == 0) {
                printf("Command litime '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                get_Send_ItemsDuration(threadClififoAnswer, backend->infoLeiloes, tempMessage.command[1]);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }


            if (strcmp(tempMessage.command[0], "time") == 0) {
                printf("Command time '%s'\n", threadUsername);
                sendTimeUser(threadClififoAnswer, &backend->mutexTimer, &backend->time);
                continue;
            }


            if (strcmp(tempMessage.command[0], "buy") == 0) {
                printf("Command buy '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexItemInfo);
                pthread_mutex_lock(&backend->mutexCliInfo);
                tryBuyItem(&backend->infoLeiloes, &backend->infoClis, threadUsername, tempMessage.command[1],
                           tempMessage.command[2]);
                pthread_mutex_unlock(&backend->mutexCliInfo);
                pthread_mutex_unlock(&backend->mutexItemInfo);
                continue;
            }


            if (strcmp(tempMessage.command[0], "cash") == 0) {
                printf("Command cash '%s'\n", threadUsername);
                sendCashUser(threadClififoAnswer, &backend->mutexCliInfo, threadUsername);
                continue;
            }


            if (strcmp(tempMessage.command[0], "add") == 0) {
                printf("Command add '%s'\n", threadUsername);
                addCashUser(&backend->mutexCliInfo, threadUsername, tempMessage.command[1]);
                continue;
            }

            if(strcmp(tempMessage.command[0], "heartbeat") == 0){
                //printf("HeartBeatReceived: '%s'\n", threadUsername);
                pthread_mutex_lock(&backend->mutexCliInfo);
                refreshTimeoutUser(&backend->infoClis, threadUsername);
                pthread_mutex_unlock(&backend->mutexCliInfo);
                continue;
            }
        }
    }
}

void refreshTimeoutUser(InfoClis *infoCLis, char username[]) {
    for(int i=0; i<infoCLis->sizeList; i++){
        if(strcmp(infoCLis->listCli[i].username, username) == 0)
            infoCLis->listCli[i].timeOut=0;
    }
}

void get_Send_ItemsSold(char threadClififoAnswer[80], InfoLeiloes infoLeiloes) {
    int j = 0;
    Leilao temp[MAX_LEILOES];

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (infoLeiloes.listLeiloes[i].item.sold == true) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;

    sendListItemUser(threadClififoAnswer, temp);
}

void get_Send_ItemsSale(char threadClififoAnswer[80], InfoLeiloes infoLeiloes) {
    int j = 0;
    Leilao temp[MAX_LEILOES];

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (infoLeiloes.listLeiloes[i].item.sold == false && infoLeiloes.listLeiloes[i].item.onSale == true) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;

    sendListItemUser(threadClififoAnswer, temp);
}

void addItemForSale(InfoLeiloes *infoLeiloes, char descricao[], char categoria[], char valorBase[], char valorInst[], char tempo[], char usernameSeller[]) {
    if (infoLeiloes->sizeList == 0)
        infoLeiloes->listLeiloes = malloc(sizeof(Leilao) * (infoLeiloes->sizeList + 1));
    else
        infoLeiloes->listLeiloes = realloc(infoLeiloes->listLeiloes, sizeof(Leilao) * (infoLeiloes->sizeList + 1));


    Leilao tempLeilao;
    tempLeilao.item.id = infoLeiloes->sizeList;
    strcpy(tempLeilao.item.descricao, descricao);
    strcpy(tempLeilao.item.categoria, categoria);
    tempLeilao.item.valorBase = (int) strtol(valorBase, NULL, 10);
    tempLeilao.item.valorInst = (int) strtol(valorInst, NULL, 10);
    tempLeilao.tempo = (int) strtol(tempo, NULL, 10);
    strcpy(tempLeilao.item.usernameSeller, usernameSeller);
    strcpy(tempLeilao.item.usernameBuyer, "-");
    tempLeilao.item.sold = false;
    strcpy(tempLeilao.item.usernamePresentBuyer, "none");
    tempLeilao.item.backUpPriceInst = tempLeilao.item.valorInst;
    tempLeilao.item.onSale = false;
    tempLeilao.item.timePromotion = 0;

    infoLeiloes->listLeiloes[infoLeiloes->sizeList] = tempLeilao;
    (infoLeiloes->sizeList)++;
}

void tryBuyItem(InfoLeiloes *infoLeiloes, InfoClis *infoClis, char username[], char idItem[], char valueGiven[]) {
    int tempIdItem, tempValueGiven;
    tempIdItem = (int) strtol(idItem, NULL, 10);
    tempValueGiven = (int) strtol(valueGiven, NULL, 10);

    //Check if valid id Given
    if (tempIdItem > infoLeiloes->sizeList - 1) {
        return;
    }

    //Check if valid Given Value given
    if (infoLeiloes->listLeiloes[tempIdItem].item.valorBase < tempValueGiven && !infoLeiloes->listLeiloes[tempIdItem].item.sold && getUserBalance(username) >= tempValueGiven) {

        strcpy(infoLeiloes->listLeiloes[tempIdItem].item.usernamePresentBuyer, username);
        infoLeiloes->listLeiloes[tempIdItem].item.valorBase = tempValueGiven;

        if (infoLeiloes->listLeiloes[tempIdItem].item.valorInst <= tempValueGiven) {
            strcpy(infoLeiloes->listLeiloes[tempIdItem].item.usernameBuyer, username);
            infoLeiloes->listLeiloes[tempIdItem].item.sold = true;

            updateUserBalance(username, (getUserBalance(username) - tempValueGiven));
            notifyItemSaleStatus(infoClis, (infoLeiloes->listLeiloes[tempIdItem].item.id) * (-1), true);
        }
    }
}

void notifyItemSaleStatus(InfoClis *infoClis, int id, bool sold) {
    union  sigval sv;
    if(id!=0){
        sv.sival_int = id;
    }else{
        if(sold == true)
            sv.sival_int = -31;
        else
            sv.sival_int = 31;
    }



    for(int i=0; i<infoClis->sizeList;i++)
        sigqueue(infoClis->listCli[i].pid, SIGUSR1, sv);

}

void get_Send_ItemsDuration(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char time[]) {
    int j = 0, temptime;
    Leilao temp[MAX_LEILOES];

    temptime = (int) strtol(time, NULL, 10);

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (infoLeiloes.listLeiloes[i].tempo <= temptime && infoLeiloes.listLeiloes[i].item.sold == false) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;
    sendListItemUser(threadClififoAnswer, temp);
}

void get_Send_ItemsValue(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char price[]) {
    int j = 0, tempPrice;
    Leilao temp[MAX_LEILOES];

    tempPrice = (int) strtol(price, NULL, 10);

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (infoLeiloes.listLeiloes[i].item.valorBase <= tempPrice && infoLeiloes.listLeiloes[i].item.sold == false) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;
    sendListItemUser(threadClififoAnswer, temp);
}

void get_Send_ItemsSeller(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char seller[]) {
    int j = 0;
    Leilao temp[MAX_LEILOES];

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (strcmp(seller, infoLeiloes.listLeiloes[i].item.usernameSeller) == 0 && infoLeiloes.listLeiloes[i].item.sold == false) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;

    sendListItemUser(threadClififoAnswer, temp);
}

void get_Send_ItemsCategory(char threadClififoAnswer[], InfoLeiloes infoLeiloes, char category[]) {
    int j = 0;
    Leilao temp[MAX_LEILOES];

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (strcmp(category, infoLeiloes.listLeiloes[i].item.categoria) == 0 && infoLeiloes.listLeiloes[i].item.sold == false) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;

    sendListItemUser(threadClififoAnswer, temp);
}

void get_Send_ItemsNonSold(char threadClififoAnswer[], InfoLeiloes infoLeiloes) {
    int j = 0;
    Leilao temp[MAX_LEILOES];

    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (infoLeiloes.listLeiloes[i].item.sold == false) {
            temp[j] = infoLeiloes.listLeiloes[i];
            j++;
        }
    }

    temp[j].item.id = -1;

    sendListItemUser(threadClififoAnswer, temp);
}

void sendListItemUser(char threadClififoAnswer[], Leilao temp[]) {
    int fdA;
    //Open Request and anwser Fifo
    if ((fdA = open(threadClififoAnswer, O_RDWR)) == -1) {
        printf("Couldn't Open fifo Answer 'command Time'!");
        pthread_exit((void *) -1);
    }

    if (write(fdA, temp, sizeof(Leilao) * 30) == -1) {
        printf("[Error Write time command] - Couldn't write to fifo.\n");
    }

    close(fdA);
}

void addCashUser(pthread_mutex_t *mutexCliInfo, char username[], char cash[]) {
    int updatedCash;

    pthread_mutex_lock(mutexCliInfo);
    if ((updatedCash = (int) strtol(cash, NULL, 10)) == 0) {
        printf("Couldn't add cash to user '%s'", username);
        pthread_mutex_unlock(mutexCliInfo);
        return;
    } else {
        updatedCash += getUserBalance(username);
        updateUserBalance(username, updatedCash);
    }
    pthread_mutex_unlock(mutexCliInfo);
}

void sendCashUser(char threadClififoAnswer[], pthread_mutex_t *mutexCliInfo, char username[]) {
    int fdA, cash;

    //Open Request and anwser Fifo
    if ((fdA = open(threadClififoAnswer, O_RDWR)) == -1) {
        printf("Couldn't Open fifo Answer 'command Time'!");
        pthread_exit((void *) -1);
    }

    pthread_mutex_lock(mutexCliInfo);
    cash = getUserBalance(username);
    if (write(fdA, &cash, sizeof(int)) == -1) {
        printf("[Error Write cash command] - Couldn't write to fifo.\n");
    }
    pthread_mutex_unlock(mutexCliInfo);

    close(fdA);
}

void sendTimeUser(char threadClififoAnswer[], pthread_mutex_t *mutexTimer, int *time) {

    int fdA;
    //Open Request and anwser Fifo
    if ((fdA = open(threadClififoAnswer, O_RDWR)) == -1) {
        printf("Couldn't Open fifo Answer 'command Time'!");
        pthread_exit((void *) -1);
    }

    pthread_mutex_lock(mutexTimer);
    if (write(fdA, time, sizeof(int)) == -1) {
        printf("[Error Write time command] - Couldn't write to fifo.\n");
    }
    pthread_mutex_unlock(mutexTimer);

    close(fdA);
}

void showItems(InfoLeiloes infoLeiloes) {
    printf("\n\nSize: %d\n", infoLeiloes.sizeList);
    printf("Item <id>: 1<description> 2<category> 3<present_value> 4<buy_now_value> 5<max_duration> 6<seller> 7<buyer> 8<sell_state> 9<present_buyer> 10<sale_state> 11<prom_duration>\n\n");
    for (int i = 0; i < infoLeiloes.sizeList; i++) {
        if (infoLeiloes.listLeiloes[i].item.sold == false) {
            if (infoLeiloes.listLeiloes[i].item.onSale == true) {
                printf("Item %d: %s %s %d %d %d %s %s 'Not Sold' %s 'On Sale' %d\n", infoLeiloes.listLeiloes[i].item.id, infoLeiloes.listLeiloes[i].item.descricao,
                       infoLeiloes.listLeiloes[i].item.categoria,
                       infoLeiloes.listLeiloes[i].item.valorBase, infoLeiloes.listLeiloes[i].item.valorInst, infoLeiloes.listLeiloes[i].tempo,
                       infoLeiloes.listLeiloes[i].item.usernameSeller, infoLeiloes.listLeiloes[i].item.usernameBuyer, infoLeiloes.listLeiloes[i].item.usernamePresentBuyer,
                       infoLeiloes.listLeiloes[i].item.timePromotion);
            } else {
                printf("Item %d: %s %s %d %d %d %s %s 'Not Sold' %s 'Not On Sale' %d\n", infoLeiloes.listLeiloes[i].item.id, infoLeiloes.listLeiloes[i].item.descricao,
                       infoLeiloes.listLeiloes[i].item.categoria,
                       infoLeiloes.listLeiloes[i].item.valorBase, infoLeiloes.listLeiloes[i].item.valorInst, infoLeiloes.listLeiloes[i].tempo,
                       infoLeiloes.listLeiloes[i].item.usernameSeller, infoLeiloes.listLeiloes[i].item.usernameBuyer, infoLeiloes.listLeiloes[i].item.usernamePresentBuyer,
                       infoLeiloes.listLeiloes[i].item.timePromotion);
            }
        } else {
            if (infoLeiloes.listLeiloes[i].item.onSale == true) {
                printf("Item %d: %s %s %d %d %d %s %s 'Sold' %s 'On Sale' %d\n", infoLeiloes.listLeiloes[i].item.id, infoLeiloes.listLeiloes[i].item.descricao,
                       infoLeiloes.listLeiloes[i].item.categoria,
                       infoLeiloes.listLeiloes[i].item.valorBase, infoLeiloes.listLeiloes[i].item.valorInst, infoLeiloes.listLeiloes[i].tempo,
                       infoLeiloes.listLeiloes[i].item.usernameSeller, infoLeiloes.listLeiloes[i].item.usernameBuyer, infoLeiloes.listLeiloes[i].item.usernamePresentBuyer,
                       infoLeiloes.listLeiloes[i].item.timePromotion);
            } else {
                printf("Item %d: %s %s %d %d %d %s %s 'Sold' %s 'Not On Sale' %d\n", infoLeiloes.listLeiloes[i].item.id, infoLeiloes.listLeiloes[i].item.descricao,
                       infoLeiloes.listLeiloes[i].item.categoria,
                       infoLeiloes.listLeiloes[i].item.valorBase, infoLeiloes.listLeiloes[i].item.valorInst, infoLeiloes.listLeiloes[i].tempo,
                       infoLeiloes.listLeiloes[i].item.usernameSeller, infoLeiloes.listLeiloes[i].item.usernameBuyer, infoLeiloes.listLeiloes[i].item.usernamePresentBuyer,
                       infoLeiloes.listLeiloes[i].item.timePromotion);
            }
        }
    }
}

void loadItems(InfoLeiloes *infoLeiloes) {
    char *nameFileItems = getenv("FITEMS");
    FILE *fd;
    Leilao leilao;

    if (nameFileItems != NULL) {
        //r set the stream at the beginning
        fd = fopen(nameFileItems, "r");

        if (fd != NULL) {
            while (fscanf(fd, "%d %s %s %d %d %d %s %s", &leilao.item.id, leilao.item.descricao, leilao.item.categoria, &leilao.item.valorBase, &leilao.item.valorInst,
                          &leilao.tempo, leilao.item.usernameSeller, leilao.item.usernameBuyer) != EOF) {

                if (infoLeiloes->sizeList == 0) {
                    //Use of malloc, because list is empty/NULL
                    infoLeiloes->listLeiloes = malloc(sizeof(Leilao) * (infoLeiloes->sizeList + 1));
                } else {
                    //Use of realloc, because list ins't empty or NULL
                    infoLeiloes->listLeiloes = realloc(infoLeiloes->listLeiloes, sizeof(Leilao) * (infoLeiloes->sizeList + 1));
                }

                //Initialize variables not contained in the file
                strcpy(leilao.item.usernamePresentBuyer, "none");
                leilao.item.sold = false;
                leilao.item.backUpPriceInst = leilao.item.valorInst;
                leilao.item.backUpPriceBase = leilao.item.valorBase;
                leilao.item.onSale = false;
                leilao.item.timePromotion = 0;


                //Adds new auction to the list, and increases size of the array
                infoLeiloes->listLeiloes[infoLeiloes->sizeList] = leilao;
                (infoLeiloes->sizeList)++;
            }

            fclose(fd);
        } else {
            printf("Couldn't open File!");
            return;
        }
    } else
        printf("Couldn't resolve File name!");
}


void releaseToken() {
    remove(TOKEN);
}


void getToken() {
    int fp = open(TOKEN, O_CREAT | O_EXCL, 0760);

    if (fp == -1) {
        printf("Program is already running!\n");
        exit(2);
    }
}


char *commandList() {
    char *command = malloc(sizeof(char) * 80);

    printf("\n\n\n\nLista de commandos\n\n");

    printf("'users' - Lista todos os utilizadores ativos\n");
    printf("'list' - Lista todos os items a venda\n");
    printf("'kick <username>' - Expulsa um utilizador da sala de leiloes\n");
    printf("'prom' - Lista todos os promotores atualmente ativos\n");
    printf("'reprom' - Atualiza todos os promotores\n");
    printf("'cancel <promotor_name>' - Cancela uma promocao\n");
    printf("'close' - Encerra Backend\n");

    printf("\nCommando: ");
    fflush(stdout);
    fgets(command, sizeof(char) * 80, stdin);

    return command;
}

void readCommand(char command[], InfoBackend *backend) {
    char *splitedCommand;
    char commandTokenized[2][80];
    int sizeCommand;
    splitedCommand = strtok(command, " ,.-\t");

    //Splitting command into single words, and saving it to commandTokenized
    for (int i = 0; splitedCommand != NULL; i++) {
        if (i == 2) {
            printf("Comando Invalido!\n");
            return;
        }
        strcpy(commandTokenized[i], splitedCommand);
        splitedCommand = strtok(NULL, " ,.-\t");
        sizeCommand = i + 1;
    }

    printf("\n\n\n\nSize: %d\n", sizeCommand);
    for (int i = 0; i < sizeCommand; i++) {
        printf("%s ", commandTokenized[i]);
        fflush(stdout);
    }

    //SIZE 1 COMMANDS
    if (sizeCommand == 1) {

        if (strcmp("users\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'users' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&backend->mutexCliInfo);
            showUsers(backend->infoClis);
            pthread_mutex_unlock(&backend->mutexCliInfo);
            return;
        }

        if (strcmp("list\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'list' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&backend->mutexItemInfo);
            showItems(backend->infoLeiloes);
            pthread_mutex_unlock(&backend->mutexItemInfo);
            return;
        }

        if (strcmp("prom\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'prom' -- %s\n", commandTokenized[0]);
            showProms(backend->infoProms);
            return;
        }

        if (strcmp("reprom\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'reprom' -- %s\n", commandTokenized[0]);
            relauch_promoters(backend);
            return;
        }

        if (strcmp("close\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'close' -- %s\n", commandTokenized[0]);
            return;
        }

        printf("\nComando invalido!\n");
        return;
    }

    //SIZE 2 COMMANDS
    if (sizeCommand == 2) {
        if (strcmp("kick", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            printf("\nCommand 'kick <username>' -- %s %s\n", commandTokenized[0], commandTokenized[1]);
            commandTokenized[1][strcspn(commandTokenized[1], "\n")] = '\0';
            pthread_mutex_lock(&backend->mutexCliInfo);
            removeUser(&backend->infoClis, commandTokenized[1]);
            pthread_mutex_unlock(&backend->mutexCliInfo);
            return;
        }

        if (strcmp("cancel", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            printf("\nCommand 'cancel <promotor_name>' -- %s %s\n", commandTokenized[0], commandTokenized[1]);
            commandTokenized[1][strcspn(commandTokenized[1], "\n")] = '\0';
            removePromoter(&backend->infoProms, commandTokenized[1], backend->funcThreads[1]);
            return;
        }

        printf("\nComando invalido!\n");
        return;
    }

    printf("\nComando invalido!\n");
}

void relauch_promoters(InfoBackend *backend) {
    backend->infoProms.stopAllProms = true;
    pthread_kill(backend->funcThreads[1], SIGUSR2);
    pthread_join(backend->funcThreads[1], NULL);

    free(backend->infoProms.promotorsList);
    backend->infoProms.sizeList = 0;
    loadPromoters(&backend->infoProms);
    backend->infoProms.stopAllProms = false;

    if (backend->infoProms.sizeList != 0)
        pthread_create(&backend->funcThreads[1], NULL, &runProm, (void *) backend);
}

void showProms(InfoProms infoProms) {
    for (int i = 0; i < infoProms.sizeList; i++) {
        printf("%d - %s\n", i + 1, infoProms.promotorsList[i].promName);
        fflush(stdout);
    }
}

void showUsers(InfoClis infoClis) {
    for (int i = 0; i < infoClis.sizeList; i++)
        printf("Name: '%s'// ClififoRequest: '%s' // ClififoAnswer: '%s' // PID: '%d'// TPid: '%lu' // timeout: %d\n", infoClis.listCli[i].username, infoClis.listCli[i].clififoRequest,
               infoClis.listCli[i].clififoAnswer, infoClis.listCli[i].pid,
               infoClis.listCli[i].tPid, infoClis.listCli[i].timeOut);
}

int loadUsers() {
    char *nameFileUsers = getenv("FUSERS");

    //Verify if Environment Variable is Declared
    if (nameFileUsers != NULL)
        return loadUsersFile(nameFileUsers);

    //Couldn't access users file
    printf("Couldn't resolve Users File Name!\n");
    return 0;
}

void *runProm(void *arg) {

    InfoBackend *backend = (InfoBackend *) arg;


    //Create Anonymous PiPe
    int fd[2];
    if (pipe(fd) == -1) {
        printf("Couldn't Create Pipe!\n");
        pthread_exit((void *) -1);
    }

    //Fork Variables
    int res[backend->infoProms.sizeList];
    char temp[160];

    for (int i = 0; i < backend->infoProms.sizeList; i++) {
        //Builds the String to execute the file
        char pathPromotor[80] = "../Executable_Files/";
        strcat(pathPromotor, backend->infoProms.promotorsList[i].promName);

        //Removing "\n" of fgets at the end of the Strings (pathPromotor & choosenPromotor)
        pathPromotor[strcspn(pathPromotor, "\n")] = '\0';
        backend->infoProms.promotorsList[i].promName[strcspn(backend->infoProms.promotorsList[i].promName, "\n")] = '\0';


        printf("\n");

        //Executing Fork
        res[i] = fork();
        backend->infoProms.promotorsList[i].pid = res[i];
        if (res[i] == 0) {

            //Redirecting Stdout
            close(1);
            dup(fd[1]);
            close(fd[1]);
            close(fd[0]);

            //RUN Promoter
            execlp(pathPromotor, backend->infoProms.promotorsList[i].promName, NULL);
            printf("There was an error running the promoter!\n");
            exit(3);
        }

        usleep(250);
    }

    //Read Message
    close(fd[1]);
    while (!backend->infoProms.stopAllProms) {
        if (read(fd[0], temp, sizeof(temp)) == -1) {
            printf("[Error] - Reading Anonymous Pipe\n");
        } else {
            //Remove "\n" of the received String
            temp[strcspn(temp, "\n")] = '\0';
            pthread_mutex_lock(&backend->mutexItemInfo);
            pthread_mutex_lock(&backend->mutexCliInfo);
            setItemOnSale(&backend->infoLeiloes, &backend->infoClis, temp);
            pthread_mutex_unlock(&backend->mutexCliInfo);
            pthread_mutex_unlock(&backend->mutexItemInfo);
        }
    }
    close(fd[0]);

    //Shut Down promoters
    for (int i = 0; i < backend->infoProms.sizeList; i++) {
        printf("Killing PID: %d\n", backend->infoProms.promotorsList[i].pid);
        kill(backend->infoProms.promotorsList[i].pid, SIGUSR1);
    }

    //Wait for childs to end
    for (int i = 0; i < backend->infoProms.sizeList; i++)
        waitpid(backend->infoProms.promotorsList[i].pid, NULL, 0);

    printf("Thread Prom Killed!\n");
}

void setItemOnSale(InfoLeiloes *infoLeiloes, InfoClis *infoClis, char sale[]) {
    char *splitedSale;
    char saleTokenized[3][80];
    int sizeSale = 0;

    //Tokenize Promotion Received
    splitedSale = strtok(sale, " ,.-\t");

    for (int i = 0; splitedSale != NULL; i++) {
        if (i == 3) {
            return;
        }
        strcpy(saleTokenized[i], splitedSale);
        splitedSale = strtok(NULL, " ,.-\t");
        sizeSale = i + 1;
    }

    //Show sale & Set Item on sale
    if (sizeSale == 3 && strtol(saleTokenized[1], NULL, 10) != 0 && strtol(saleTokenized[2], NULL, 10) != 0) {
        printf("\n\n\n\nSize: %d\n", sizeSale);
        for (int i = 0; i < sizeSale; i++) {
            printf("'%s' ", saleTokenized[i]);
            fflush(stdout);
        }

        for (int i = 0; i < infoLeiloes->sizeList; i++) {
            if(strcmp(infoLeiloes->listLeiloes[i].item.categoria, saleTokenized[0]) == 0 && infoLeiloes->listLeiloes[i].item.sold == false){
                infoLeiloes->listLeiloes[i].item.valorInst = (int) ((float) infoLeiloes->listLeiloes[i].item.backUpPriceInst * ((float) (1.0f - (((float) strtol(saleTokenized[1], NULL, 10) / 100)))));
                infoLeiloes->listLeiloes[i].item.onSale = true;
                infoLeiloes->listLeiloes[i].item.timePromotion = (int) strtol(saleTokenized[2], NULL, 10);
                notifyItemPromotionStatus(infoClis, infoLeiloes->listLeiloes[i].item.id, true);

                //Check if Sold, by price drop
                if(infoLeiloes->listLeiloes[i].item.valorInst <= infoLeiloes->listLeiloes[i].item.valorBase && strcmp(infoLeiloes->listLeiloes[i].item.usernamePresentBuyer, "none") != 0 && infoLeiloes->listLeiloes[i].item.sold == false){
                    infoLeiloes->listLeiloes[i].item.sold= true;
                    strcpy(infoLeiloes->listLeiloes[i].item.usernameBuyer, infoLeiloes->listLeiloes[i].item.usernamePresentBuyer);
                    updateUserBalance(infoLeiloes->listLeiloes[i].item.usernamePresentBuyer, (getUserBalance(infoLeiloes->listLeiloes[i].item.usernamePresentBuyer)-infoLeiloes->listLeiloes[i].item.valorBase));
                    notifyItemSaleStatus(infoClis, (infoLeiloes->listLeiloes[i].item.id) * (-1), true);
                }
            }
        }
    }
}

void notifyItemPromotionStatus(InfoClis *infoClis, int id, bool onPromotion) {
    union sigval sv;

    if(id!=0)
        sv.sival_int = id;
    else{
        if(onPromotion == true)
            sv.sival_int = 31;
        else
            sv.sival_int = -31;
    }

    for(int i=0; i<infoClis->sizeList;i++)
        sigqueue(infoClis->listCli[i].pid, SIGUSR2, sv);
}

void loadPromoters(InfoProms *infoProms) {
    char *nameFileProms = getenv("FPROMOTERS");
    FILE *fd;
    char temp[80];
    PromData tempProm;

    infoProms->stopAllProms = false;

    if (nameFileProms != NULL) {
        //r set the stream at the beginning
        fd = fopen(nameFileProms, "r");

        if (fd != NULL) {
            while (fgets(tempProm.promName, sizeof(char) * 80, fd) != NULL) {
                if (infoProms->sizeList == 0)
                    infoProms->promotorsList = malloc(sizeof(PromData) * (infoProms->sizeList + 1));
                else
                    infoProms->promotorsList = realloc(infoProms->promotorsList, sizeof(PromData) * (infoProms->sizeList + 1));


                infoProms->promotorsList[infoProms->sizeList] = tempProm;
                (infoProms->sizeList)++;
            }
            fclose(fd);
        } else {
            printf("Couldn't open file!");
            return;
        }
    } else
        printf("Couldn't resolve File name!");
}

void deleteServFifo() {
    //close fd
    remove("servFifo");
}





