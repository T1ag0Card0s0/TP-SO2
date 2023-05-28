#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <time.h>

#define TAM 200
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20

#define MUTEX_SERVER _T("Server")
#define SHARED_MEMORY_NAME _T("ShareMemory")
#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")
#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_PROD _T("MutexProd")
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

typedef int (*PFUNC_PROD)(SHARED_DATA*);