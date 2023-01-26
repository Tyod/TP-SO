#ifndef TP_UTIL_H
#define TP_UTIL_H

//General Includes
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>

#define MAX_LEILOES 30
#define TOKEN "token.txt"

typedef struct {
    int id;
    char descricao[80];
    char categoria[80];
    int valorBase;
    int valorInst;
    char usernameSeller[80];
    char usernameBuyer[80];
    char usernamePresentBuyer[80];
    bool sold;
    int backUpPriceInst;
    int backUpPriceBase;
    bool onSale;
    int timePromotion;
} Item;

typedef struct {
    Item item;
    int tempo;
} Leilao;

typedef struct {
    char clififoRequest[80];
    char clififoAnswer[80];
    char name[80];
    char password[80];
    int pid;
}InfoLogin;

typedef struct {
    int sizeMessage;
    char command[6][80];
}Message;

#endif //TP_UTIL_H
