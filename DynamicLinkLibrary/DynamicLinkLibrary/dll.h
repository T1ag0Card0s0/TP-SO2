#pragma once
#pragma once

#ifdef DYNAMICLINKLIBRARY_EXPORTS
# define DLL_API __declspec(dllexport)
#else
#define DLL_API __declspec(dllimport)
#endif // DLL_EXPORTS
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <tchar.h>
#include <stdio.h>

#define TAM 256
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20
#define MAX_CARS_PER_ROAD 8
#define MAX_PLAYERS 2

#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_CONSUMIDOR _T("MutexConsumidor")
#define MUTEX_PROD _T("MutexProd")
#define SHARED_MEMORY_NAME _T("ShareMemory")
#define UPDATE_EVENT _T("UpdateEvent")
#define CAR _T('C')
#define FROG _T('F')
//estrutura para o buffer circular
typedef struct CELULA_BUFFER {
    int id;
    TCHAR str[100];
}CELULA_BUFFER;
//representa a nossa memoria partilhada
typedef struct BUFFER_CIRCULAR {
    int nProdutores;
    int nConsumidores;
    int posE; //proxima posicao de escrita  
    int posL; //proxima posicao de leitura
    CELULA_BUFFER buffer[TAM_BUF]; //buffer circular em si (array de estruturas)
}BUFFER_CIRCULAR;

typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;

    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;

typedef struct SHARED_MEMORY {
    BUFFER_CIRCULAR bufferCircular;
    SHARED_BOARD sharedBoard;
}SHARED_MEMORY;
//estrutura de apoio
typedef struct SHARED_DATA {
    SHARED_MEMORY* memPar; //ponteiro para a memoria partilhada
    HANDLE hSemEscrita; //handle para o semaforo que controla as escritas (controla quantas posicoes estao vazias)
    HANDLE hSemLeitura; //handle para o semaforo que controla as leituras (controla quantas posicoes estao preenchidas)
    HANDLE hMutex;
    HANDLE hdll;
    int terminar; // 1 para sair, 0 em caso contrario
    int id;
}SHARED_DATA;

typedef enum WAY { UP, DOWN, LEFT, RIGHT, STOP }WAY;
typedef struct OBJECT {
    DWORD dwX, dwY;
    DWORD dwLastX, dwLastY;
    TCHAR c;
}OBJECT;
typedef struct ROAD {
    DWORD dwNumOfCars;
    DWORD dwNumOfObjects;
    DWORD dwSpaceBetween;
    DWORD dwSpeed;
    DWORD dwTimeStoped;

    HANDLE hMutex;
    HANDLE hThread;

    WAY lastWay;
    WAY way;

    OBJECT cars[MAX_CARS_PER_ROAD];
    OBJECT objects[MAX_CARS_PER_ROAD];
}ROAD;


DLL_API extern int consumidor(SHARED_DATA* sharedData, ROAD* road, DWORD dwNumRoads);
DLL_API extern int produtor(SHARED_DATA* sharedData);
DLL_API extern void initSharedBoard(SHARED_BOARD* sharedBoard, DWORD dwHeight);
void commandExecutor(TCHAR command[], ROAD* road);
void initGameData(SHARED_DATA * sharedData, HANDLE * hFileMap);
void GoToXY(int column, int line);
