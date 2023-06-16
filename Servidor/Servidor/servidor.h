#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <time.h>
#define TAM 256
#define TAM_BUF 10

#define MAX_ROADS 8 //numero maximo de estradas
#define MAX_WIDTH 20 //maaximo comprimento
#define MAX_CARS_PER_ROAD 8 //maximo numeo de carros por estrada
#define MAX_PLAYERS 2 //maximo de jogadores

#define EXIT_EVENT _T("ExitEvent") //evento para sair
#define UPDATE_EVENT _T("UpdateEvent") //evento para atualizar
#define WRITE_SEMAPHORE _T("WriteSemaphore") //semafro para escrever
#define READ_SEMAPHORE _T("ReadSemaphore") //semafro para ler
#define MUTEX_CONSUMIDOR _T("MutexConsumidor") //mutex para consumir mensagens do operador
#define MUTEX_SERVER _T("Server") //servidor
#define MUTEX_ROAD _T("MutexRoad") // mutex para haver sincronismo nas estradas
#define SHARED_MEMORY_NAME _T("ShareMemory") //nome da memoria partilhada

#define KEY_SPEED TEXT("Speed") //velociade inicial do regedit
#define KEY_ROADS TEXT("Roads") //numero de estradas iniciais do regedit

#define KEY_DIR TEXT("Software\\TPSO2\\")
#define PIPE_NAME _T("\\\\.\\pipe\\TP-SO2")//nome do pipe

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
//tabuleiro do jogo
typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;

    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;
//memoria a ser partilhada
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
//diração a seguir
typedef enum WAY { UP, DOWN, LEFT, RIGHT, STOP }WAY;
typedef struct OBJECT {
    DWORD dwX, dwY;
    TCHAR c;
}OBJECT;
//dados de uma estrada
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
//tipo de jogo a correr
typedef enum GAME_TYPE {
    SINGLE_PLAYER,MULTI_PLAYER,NONE
}GAME_TYPE;
//dados do jogo a enviar pelo pipe aos clientes ativos
typedef struct PIPE_GAME_DATA {
    SHARED_BOARD sharedBoard;
    DWORD dwLevel;
    DWORD dwPlayer1Points, dwPlayer2Points;
    DWORD dwX, dwY;
    DWORD dwNEndLevel;//numero de vezes que chegou ao fim
    GAME_TYPE gameType;
    BOOL bWaiting;
}PIPE_GAME_DATA;
//dados dos jogadores
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
//dados do pipe
typedef struct PIPE_DATA {
    PLAYER_DATA playerData[MAX_PLAYERS];
    HANDLE hEvents[MAX_PLAYERS];
    HANDLE hMutex;
    DWORD dwNumClients;
}PIPE_DATA;
//dados do jogo
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

void GoToXY(int column, int line);//posiciona o cursor
void getCurrentCursorPosition(int* x, int* y);//obtem posiçoes do cursor
void moveObject(OBJECT* objData, WAY way);//move o objeto dado uma direção
void changeLevel(GAME* game, BOOL bNextLevel);//muda o nivel (o ultimo parametro verdadeiro ou falso, caso queiramos que o mesmo nivel seja reiniciado)
void restartGame(GAME* game);//reicinica o jogo todo 
void initRegestry(GAME* data);//inicializa os dados do regedit
void initRoads(ROAD* roads, DWORD dwInitSpeed);//inicializa as estradas
void initPipeData(PIPE_DATA* pipeData);//inicializa os dados dos pipes
BOOL runClientRequest(GAME* game, TCHAR c,DWORD i);//corre o pedido do jogador
void disconectClient(GAME* game, DWORD i);//disconecta cliente
DWORD WINAPI AFKCounter(LPVOID param);//conta quantos segundos cada jogador ativo está sem pressionar numa tecla
DWORD WINAPI ReceivePipeThread(LPVOID param);//recebe dados do pipe
DWORD WINAPI WritePipeThread(LPVOID param);//escreve para o pipe informações do jogo
DWORD WINAPI PipeManagerThread(LPVOID param);//gere a chegada de jogadores
DWORD WINAPI ThreadConsumidor(LPVOID param);//consome a informação chegada por memeoria partilhada vinda do operador
DWORD WINAPI CMDThread(LPVOID param);//thread para comandos introduzidos no servidor
DWORD WINAPI UpdateThread(LPVOID param);//atualiza o tabuleiro
DWORD WINAPI RoadMove(LPVOID param);//movimenta a estrada
typedef int (*PFUNC_CONS)(SHARED_DATA*,ROAD *,DWORD);//funcao de consumidor da dll
typedef void (*PFUNC_INIT_SHARED_BOARD)(SHARED_BOARD*,DWORD);//funcao de inicialização do tabuleiro da dll

