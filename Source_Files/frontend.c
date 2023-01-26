#include "../Header_Files/auxFrontend.h"

int main(int argc, char *argv[]) {

    //Arguments Verification
    if (argc != 3) {
        printf("[ERROR SYNTAX] - ./frontend <username> <password>");
        return -1;
    }

    //Checks if backend is running
    look4token();

    struct sigaction saINT, saUSR1, saUSR2, saCHLD;
    saINT.sa_handler = &handleSignalSIGINT;
    saCHLD.sa_handler = &handleSignalSIGCHLD;
    saUSR1.sa_sigaction = &handleSignalSIGUSR1;
    saUSR1.sa_flags = SA_SIGINFO | SA_RESTART;
    saUSR2.sa_sigaction = &handleSignalSIGUSR2;
    saUSR2.sa_flags = SA_SIGINFO | SA_RESTART;

    sigaction(SIGINT, &saINT, NULL);
    sigaction(SIGUSR1, &saUSR1, NULL);
    sigaction(SIGUSR2, &saUSR2, NULL);
    sigaction(SIGCHLD, &saCHLD, NULL);


    //Declaring Variables
    char username[20], password[20];
    char command[80];
    loginAuthentication = false;

    //Obtem credênciais de login
    strcpy(username, argv[1]);
    strcpy(password, argv[2]);
    printf("Username: %s | Password: %s \n", username, password);

    //Login Struct
    InfoLogin login;

    //Set Credentials
    setUpCredentials(&login, username, password);

    //Create and open fifoClis
    int fdR;
    fdR = createOpenReqFifo(login);
    createAnsFifo(login);

    //Send Cli Credentials to Serv
    sendServerLoginCredentials(login);

    //Receive Confirmation from Server
    receiveCliLoginConfirmation();

    //Create Thread Responsible for HeartBeats
    pthread_mutex_init(&mutexComms, NULL);
    pthread_create(&tidHeartBeat, NULL, &sendHeartBeats, (void*) &fdR);

    do {
        //Escolhe o commando
        char *tempCommand = commandList();
        if (tempCommand != NULL) {
            //Copia para uma variavel local de ponteiro para poder libertar a memória alocada
            strcpy(command, tempCommand);
            free(tempCommand);
        }

        //Lê o commando
        readCommand(command, fdR);
    } while (strcmp("exit\n", command) != 0);

}

void *sendHeartBeats(void *arg) {
    int* fdR = (int*) arg;
    char* heartBeatRateChar = getenv("HEARTBEAT");
    int heartBeatRateInt;

    if((heartBeatRateInt = (int) strtol(heartBeatRateChar, NULL, 10)) == 0){
        printf("Couldn't Read Environment Variable 'HEARTBEAT'\n");
        pthread_exit((void*) -1);
    }

    //Build Up Message
    Message msg;
    strcpy(msg.command[0], "heartbeat");
    msg.sizeMessage = 1;

    while(1){
        pthread_mutex_lock(&mutexComms);
        writeToServ(msg.command, msg.sizeMessage, *fdR);
        pthread_mutex_unlock(&mutexComms);
        sleep(heartBeatRateInt);
    }
}

//To terminate Thread HeartBeat
void handleSignalSIGCHLD(int s){
    pthread_exit((void*) 0);
}

//For promotions notifications
void handleSignalSIGUSR2(int s, siginfo_t *sip, void *ptr) {
    char message[160];

    if (sip->si_value.sival_int < 0) {
        if (sip->si_value.sival_int == -31)
            strcpy(message, "\nThe promotion on the item's category with id 0, has ended!\n");
        else
            sprintf(message, "\nThe promotion on the item's category with id %d, has ended!\n", (sip->si_value.sival_int * -1));
    }
    if (sip->si_value.sival_int > 0) {
        if (sip->si_value.sival_int == 31)
            strcpy(message, "\nThe category of the item with id 0, is now on promotion!\n");
        else
            sprintf(message, "\nThe category of the item with id %d, is now on promotion!\n", (sip->si_value.sival_int));
    }

    //write(1, message, sizeof(char)* strlen(message));
}


//For buy/sell notifications
void handleSignalSIGUSR1(int s, siginfo_t *sip, void *ptr) {
    char message[160];

    if (sip->si_value.sival_int < 0) {
        if (sip->si_value.sival_int == -31)
            strcpy(message, "\nThe item with id: 0, was sold!\n");
        else
            sprintf(message, "\nThe item with id: %d, was sold!\n", (sip->si_value.sival_int * -1));
    }
    if (sip->si_value.sival_int > 0) {
        if (sip->si_value.sival_int == 31)
            strcpy(message, "\nThe item with id: 0, was added for sale!\n");
        else
            sprintf(message, "\nThe item with id: %d, was added for sale!\n", (sip->si_value.sival_int));
    }

    //write(1, message, sizeof(char) * strlen(message));
}

void createAnsFifo(InfoLogin login) {

    if (mkfifo(login.clififoAnswer, 0777) == -1) {
        printf("Couldn't Create %s pipe!\n", login.clififoAnswer);
        exit(-1);
    }

}

void handleSignalSIGINT(int s) {
    shutDownCLient();
    write(1, "Frontend Killed!\n", sizeof(char) * strlen("Frontend Killed!\n"));
    exit(1);
}


void receiveCliLoginConfirmation() {

    int fdAns;
    if ((fdAns = open(GFifoNameAnswer, O_RDWR)) == -1) {
        printf("Couldn't Open %s pipe!\n", GFifoNameAnswer);
        shutDownCLient();
        exit(-1);
    }

    char loginState[20];
    if (read(fdAns, loginState, sizeof(char) * 20) != -1) {
        close(fdAns);
        if (strncmp(loginState, "1", sizeof(char)) == 0) {
            loginAuthentication = true;
            printf("Logged In successfully!\n");
        } else {
            if(strncmp(loginState, "0", sizeof(char)) == 0)
                printf("Login credentials didn't match!\n");
            if(strncmp(loginState, "2", sizeof(char)) == 0)
                printf("User is already Logged in!\n");

            shutDownCLient();
            exit(-1);
        }
    } else {
        printf("Couldn't Send Credentials to server!\n");
        close(fdAns);
        shutDownCLient();
        exit(-1);
    }
}

void setUpCredentials(InfoLogin *login, char username[], char password[]) {
    char fifoNameR[80], fifoNameA[80];

    sprintf(fifoNameR, "%d", getpid());
    strcat(fifoNameR, "cliFifoRequest");
    strcpy(login->clififoRequest, fifoNameR);
    strcpy(GFifoNameRequest, login->clififoRequest);

    sprintf(fifoNameA, "%d", getpid());
    strcat(fifoNameA, "cliFifoAnswer");
    strcpy(login->clififoAnswer, fifoNameA);
    strcpy(GFifoNameAnswer, login->clififoAnswer);

    strcpy(login->name, username);
    strcpy(login->password, password);
    login->pid = getpid();
}

int createOpenReqFifo(InfoLogin login) {
    //Create cliFifoRequest
    if (mkfifo(login.clififoRequest, 0777) == -1) {
        printf("Couldn't Create %s pipe!\n", login.clififoRequest);
        exit(-1);
    }

    //Open cliFifo
    int fdCliReq;
    if ((fdCliReq = open(login.clififoRequest, O_RDWR)) == -1) {
        printf("Couldn't Open %s pipe!\n", login.clififoRequest);
        shutDownCLient();
        exit(-1);
    }

    return fdCliReq;
}

void sendServerLoginCredentials(InfoLogin login) {

    //Open servFifo
    int fdServ;
    if ((fdServ = open("servFifo", O_RDWR)) == -1) {
        printf("Couldn't Open 'servFifo' pipe!\n");
        shutDownCLient();
        exit(-1);
    }

    //Send Login Credentials
    if (write(fdServ, &login, sizeof(InfoLogin)) == -1) {
        printf("Couldn't Send Credentials to server!\n");
        shutDownCLient();
        exit(-1);
    }
}

void look4token() {
    int fp = open(TOKEN, O_CREAT | O_EXCL, 0760);

    if (fp != -1) {
        printf("Program 'backend' isn't running!\n");
        remove("token.txt");
        exit(2);
    }
}

void readCommand(char command[], int fdCliReq) {
    char *splitedCommand;
    char commandTokenized[6][80];
    int sizeCommand;
    char *validadeConversionEndptr1 = NULL, *validadeConversionEndptr2 = NULL, *validadeConversionEndptr3 = NULL;
    splitedCommand = strtok(command, " ,.-\t");

    for (int i = 0; splitedCommand != NULL; i++) {
        if (i == 6) {
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
        if (strcmp("exit\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'exit' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            shutDownCLient();
            return;
        }

        if (strcmp("cash\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'cash' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            printf("Received Cash: '%d' €", receiveIntFromServ());
            return;
        }

        if (strcmp("time\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'time' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            printf("Received Time: '%d' seconds", receiveIntFromServ());
            return;
        }

        if (strcmp("list\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'list' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            receiveItemsFromServ();
            return;
        }

        if (strcmp("liprom\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'liprom' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            receiveItemsFromServ();
            return;
        }

        if (strcmp("lisold\n", commandTokenized[0]) == 0) {
            printf("\nCommand 'lisold' -- %s\n", commandTokenized[0]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            receiveItemsFromServ();
            return;
        }

        printf("\nComando invalido!\n");
        return;
    }





    //SIZE 2 COMMANDS
    if (sizeCommand == 2) {
        if (strcmp("add", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            //If invalid String is given to strtol (it can't convert), it will set validadeConversionEndptrX = comandTokenized[X]
            strtol(commandTokenized[1], &validadeConversionEndptr1, 10);
            //Checking validadeConversionEndptrX = comandTokenized[X]
            if (commandTokenized[1] != validadeConversionEndptr1) {
                printf("\nComand 'add <value>' -- %s %ld\n", commandTokenized[0],
                       strtol(commandTokenized[1], NULL, 10));
                pthread_mutex_lock(&mutexComms);
                writeToServ(commandTokenized, sizeCommand, fdCliReq);
                pthread_mutex_unlock(&mutexComms);
                return;
            }
        }

        if (strcmp("licat", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            printf("\nCommand 'licat <nome-categoria>' -- %s %s\n", commandTokenized[0], commandTokenized[1]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            receiveItemsFromServ();
            return;
        }

        if (strcmp("lisel", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            printf("\nCommand 'lisel <username-vendedor>' -- %s %s\n", commandTokenized[0], commandTokenized[1]);
            pthread_mutex_lock(&mutexComms);
            writeToServ(commandTokenized, sizeCommand, fdCliReq);
            pthread_mutex_unlock(&mutexComms);
            receiveItemsFromServ();
            return;
        }

        if (strcmp("lival", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            //If invalid String is given to strtol (it can't convert), it will set validadeConversionEndptrX = comandTokenized[X]
            strtol(commandTokenized[1], &validadeConversionEndptr1, 10);
            //Checking validadeConversionEndptrX = comandTokenized[X]
            if (commandTokenized[1] != validadeConversionEndptr1) {
                printf("\nCommand 'lival <preco_max>' -- %s %ld\n", commandTokenized[0], strtol(commandTokenized[1], NULL, 10));
                pthread_mutex_lock(&mutexComms);
                writeToServ(commandTokenized, sizeCommand, fdCliReq);
                pthread_mutex_unlock(&mutexComms);
                receiveItemsFromServ();
                return;
            }
        }

        if (strcmp("litime", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[1], sizeof(char)) != 0) {
            //If invalid String is given to strtol (it can't convert), it will set validadeConversionEndptrX = comandTokenized[X]
            strtol(commandTokenized[1], &validadeConversionEndptr1, 10);
            //Checking validadeConversionEndptrX = comandTokenized[X]
            if (commandTokenized[1] != validadeConversionEndptr1) {
                printf("\nCommand 'litime <temp_max>' -- %s %ld\n", commandTokenized[0],
                       strtol(commandTokenized[1], NULL, 10));
                pthread_mutex_lock(&mutexComms);
                writeToServ(commandTokenized, sizeCommand, fdCliReq);
                pthread_mutex_unlock(&mutexComms);
                receiveItemsFromServ();
                return;
            }
        }

        printf("\nComando invalido!\n");
        return;
    }



    //SIZE 3 COMMANDS
    if (sizeCommand == 3) {
        if (strcmp("buy", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[2], sizeof(char)) != 0) {
            //If invalid String is given to strtol (it can't convert), it will set validadeConversionEndptrX = comandTokenized[X]
            strtol(commandTokenized[1], &validadeConversionEndptr1, 10);
            strtol(commandTokenized[2], &validadeConversionEndptr2, 10);
            //Checking validadeConversionEndptrX = comandTokenized[X]
            if (commandTokenized[1] != validadeConversionEndptr1 && commandTokenized[2] != validadeConversionEndptr2) {
                printf("\nCommand 'buy <id> <valor>' -- %s %ld %ld\n", commandTokenized[0],
                       strtol(commandTokenized[1], NULL, 10), strtol(commandTokenized[2], NULL, 10));
                pthread_mutex_lock(&mutexComms);
                writeToServ(commandTokenized, sizeCommand, fdCliReq);
                pthread_mutex_unlock(&mutexComms);
                return;
            }
        }

        printf("Comando invalido!\n");
        return;
    }

    //SIZE 6 COMMANDS
    if (sizeCommand == 6) {
        if (strcmp("sell", commandTokenized[0]) == 0 && strncmp("\n", commandTokenized[5], sizeof(char)) != 0) {
            //If invalid String is given to strtol (it can't convert), it will set validadeConversionEndptrX = comandTokenized[X]
            strtol(commandTokenized[3], &validadeConversionEndptr1, 10);
            strtol(commandTokenized[4], &validadeConversionEndptr2, 10);
            strtol(commandTokenized[5], &validadeConversionEndptr3, 10);
            //Checking validadeConversionEndptrX = comandTokenized[X]
            if (commandTokenized[3] != validadeConversionEndptr1 && commandTokenized[4] != validadeConversionEndptr2 && commandTokenized[5] != validadeConversionEndptr3) {

                printf("\nCommand 'sell <nome-item> <categoria> <preco_base> <preco_inst> <tempo-max>' -- %s %s %s %ld %ld %ld\n",
                       commandTokenized[0], commandTokenized[1], commandTokenized[2], strtol(commandTokenized[3], NULL, 10),
                       strtol(commandTokenized[4], NULL, 10), strtol(commandTokenized[5], NULL, 10));
                pthread_mutex_lock(&mutexComms);
                writeToServ(commandTokenized, sizeCommand, fdCliReq);
                pthread_mutex_unlock(&mutexComms);
                return;
            }
        }

        printf("\nComando invalido!\n");
        return;
    }

    printf("Comando invalido!\n");
}

int receiveIntFromServ() {
    int fdAns;
    if((fdAns = open(GFifoNameAnswer, O_RDWR)) == -1){
        printf("Couldn't Open %s pipe!\n", GFifoNameAnswer);
        shutDownCLient();
        exit(-1);
    }

    int temp;
    if(read(fdAns, &temp, sizeof(int)) == -1){
        printf("[Error Write time command] - Couldn't write to fifo.\n");
        close(fdAns);
        return -1;
    }else{
        close(fdAns);
        return temp;
    }
}

void receiveItemsFromServ() {
    int fdAns;
    if((fdAns = open(GFifoNameAnswer, O_RDWR)) == -1){
        printf("Couldn't Open %s pipe!\n", GFifoNameAnswer);
        shutDownCLient();
        exit(-1);
    }

    Leilao temp[MAX_LEILOES];

    if(read(fdAns, &temp, sizeof(Leilao)*30) == -1){
        printf("[Error Write time command] - Couldn't write to fifo.\n");
        return;
    }else{
        printf("Item <id>: 1<description> 2<category> 3<present_value> 4<buy_now_value> 5<max_duration> 6<seller> 7<buyer> \n\nReceived the following items:\n");
        for(int i=0; i<30; i++){
            if(temp[i].item.id == -1)
                break;

            printf("Item %d: %s %s %d %d %d %s %s\n", temp[i].item.id, temp[i].item.descricao, temp[i].item.categoria, temp[i].item.valorBase, temp[i].item.valorInst, temp[i].tempo, temp[i].item.usernameSeller, temp[i].item.usernameBuyer);
        }
    }

    close(fdAns);
}

void writeToServ(char message[][80], int sizeCommand, int fdCLiReq) {
    message[sizeCommand-1][strcspn(message[sizeCommand-1], "\n")] = '\0';

    Message tempMessage;
    tempMessage.sizeMessage = sizeCommand;

    for(int i=0; i<sizeCommand; i++)
        strcpy(tempMessage.command[i], message[i]);

    if (write(fdCLiReq, &tempMessage, sizeof(Message)) == -1) {
        printf("[ERROR Command] Couldn't write to pipe %s", GFifoNameRequest);
    }
}

char *commandList() {
    char *command = malloc(sizeof(char) * 80);

    printf("\n\n\n\nLista de commandos\n\n");

    printf("'sell <nome-item> <categoria> <preco_base> <preco_inst> <tempo-max>' - Coloca um item à venda\n");
    printf("'list' - Lista todos os items\n");
    printf("'liprom' - Lista todos os items em promocao\n");
    printf("'lisold' - Lista todos os items ja vendidos\n");
    printf("'licat <nome-categoria>' - Lista items de uma categoria\n");
    printf("'lisel <username-vendedor>' - Lista items de um vendedor\n");
    printf("'lival <preco_max>' - Lista items até um valor (preco_max)\n");
    printf("'litime <tempo_max>' - Lista items com prazo até um determinado instante (tempo_max)\n");
    printf("'time' - Mostra Tempo Atual\n");
    printf("'buy <id> <valor>' - Propoem compra de item\n");
    printf("'cash' - Motra dinheiro do utilizador\n");
    printf("'add <valor>' - Adiciona Saldo ao Utilizador\n");
    printf("'exit' - Abandona Sala de leiloes\n");

    printf("Commando: ");
    fflush(stdout);
    fgets(command, sizeof(char) * 80, stdin);

    return command;
}

void shutDownCLient() {
    //close fd
    unlink(GFifoNameRequest);
    unlink(GFifoNameAnswer);

    if (loginAuthentication == true) {
        //kill Thread
        pthread_kill(tidHeartBeat, SIGCHLD);
        pthread_join(tidHeartBeat, NULL);
        write(1, "\nThread HeartBeats Killed!\n", sizeof("\nThread HeartBeats Killed!\n"));

        //destroy mutex
        pthread_mutex_destroy(&mutexComms);
    }
}


