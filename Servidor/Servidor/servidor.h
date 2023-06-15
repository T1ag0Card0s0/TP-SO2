#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <time.h>
#define TAM 256
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20
#define MAX_CARS_PER_ROAD 8
#define MAX_PLAYERS 2

#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")
#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_CONSUMIDOR _T("MutexConsumidor")
#define MUTEX_SERVER _T("Server")
#define MUTEX_ROAD _T("MutexRoad")
#define SHARED_MEMORY_NAME _T("ShareMemory")

#define KEY_SPEED TEXT("Speed")
#define KEY_ROADS TEXT("Roads")

#define KEY_DIR TEXT("Software\\TPSO2\\")
#define PIPE_NAME _T("\\\\.\\pipe\\TP-SO2")

#define CAR_RIGHT _T('<')
#define CAR_LEFT _T('>')
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
//    DWORD dwLastX, dwLastY;
    TCHAR c;
}OBJECT;
typedef struct ROAD {
    DWORD dwNumOfCars;
    DWORD dwNumOfObjects;
    DWORD dwSpaceBetween;
    DWORD dwSpeed;
    DWORD dwTimeStoped;
    BOOL bChanged;
    HANDLE hMutex;
    HANDLE hThread;

    WAY lastWay;
    WAY way;

    OBJECT cars[MAX_CARS_PER_ROAD];
    OBJECT objects[MAX_CARS_PER_ROAD];
}ROAD;
typedef enum GAME_TYPE {
    SINGLE_PLAYER,MULTI_PLAYER,NONE
}GAME_TYPE;
typedef struct PIPE_GAME_DATA {
    SHARED_BOARD sharedBoard;
    DWORD dwLevel;
    DWORD dwPlayer1Points, dwPlayer2Points;
    DWORD dwX, dwY;
    DWORD dwNEndLevel;//numero de vezes que chegou ao fim
    GAME_TYPE gameType;
    BOOL bWaiting;
}PIPE_GAME_DATA;
typedef struct PLAYER_DATA {
    HANDLE hPipe;
    OVERLAPPED overlapRead,overlapWrite;
    BOOL active;
    OBJECT obj;
    DWORD dwPoints;
    DWORD dwNEndLevel;//numero de vezes que chegou ao fim
    DWORD dwAFKseg;// numero de segundos away from keyboard 'afk'
    GAME_TYPE gameType;
    BOOL bWaiting;
}PLAYER_DATA;
typedef struct PIPE_DATA {
    PLAYER_DATA playerData[MAX_PLAYERS];
    HANDLE hEvents[MAX_PLAYERS];
    HANDLE hMutex;
    DWORD dwNumClients;
}PIPE_DATA;
typedef struct GAME {
    DWORD dwLevel;
    DWORD dwShutDown;
    DWORD dwInitSpeed, dwInitNumOfRoads;
    BOOL  bPaused;

    HANDLE hKey;

    SHARED_DATA sharedData;
    PIPE_DATA pipeData;
    ROAD roads[MAX_ROADS];
}GAME;

void initRoads(ROAD* roads, DWORD dwNumOfRoads);
void initRegestry(GAME* data);
void moveObject(OBJECT* objData, WAY way);
void changeLevel(GAME* game, BOOL bNextLevel);
typedef int (*PFUNC_CONS)(SHARED_DATA*,ROAD *,DWORD);
typedef void (*PFUNC_INIT_SHARED_BOARD)(SHARED_BOARD*,DWORD);

