
#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <time.h>

#define TAM 256
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20
#define MAX_CARS_PER_ROAD 8

#define MUTEX_SERVER _T("Server")
#define SHARED_MEMORY_NAME _T("ShareMemory")
#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")
#define MUTEX_ROAD _T("MutexRoad")
#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_CONSUMIDOR _T("MutexConsumidor")

#define KEY_SPEED TEXT("Speed")
#define KEY_ROADS TEXT("Roads")

#define KEY_DIR TEXT("Software\\TPSO2\\")

#define CAR _T('C')
#define FROG _T('F')

typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;

    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;
typedef struct CELULA_BUFFER {
    TCHAR command[TAM];
}CELULA_BUFFER;
typedef struct BUFFER_CIRCULAR {
    DWORD dwNumProd;
    DWORD dwNumCons;
    DWORD dwPosE;//proxima posicao de escrita
    DWORD dwPosL;//proxima posicao de leitura

    CELULA_BUFFER buffer[TAM_BUF];
}BUFFER_CIRCULAR;
typedef struct SHARED_MEMORY {
    SHARED_BOARD sharedBoard;
    BUFFER_CIRCULAR bufferCircular;
}SHARED_MEMORY;

typedef enum WAY { UP, DOWN, LEFT, RIGHT, STOP }WAY;
typedef struct OBJECT {
    DWORD dwX, dwY;
    DWORD dwLastX, dwLastY;
    TCHAR c;
}OBJECT;
typedef struct ROAD {
    DWORD dwNumOfCars;
    DWORD dwSpaceBetween;
    DWORD dwSpeed;

    HANDLE hMutex;
    HANDLE hThread;

    WAY way;

    OBJECT cars[MAX_CARS_PER_ROAD];
}ROAD;

typedef struct GAME {
    DWORD dwLevel;
    DWORD dwShutDown;
    DWORD dwInitSpeed, dwInitNumOfRoads;

    HANDLE hKey;

    HANDLE hSemEscrita;//handle para o semaforo que controla as escritas (controla quantas posicoes estao vazias)
    HANDLE hSemLeitura; //handle para o semaforo que controla as leituras (controla quantas posicoes estao preenchidas)
    HANDLE hMutex;

    SHARED_MEMORY *sharedMemory;
    ROAD roads[MAX_ROADS];
}GAME;

//Cabeçalhos de funçoes de inicializaçao
void initRoads(ROAD* roads, DWORD dwNumOfRoads);
void initSharedBoard(SHARED_BOARD* sharedBoard, DWORD dwHeight);
void initRegestry(GAME* data);
void moveObject(OBJECT* objData, WAY way);

//Posicionamento de cursor
void GoToXY(int column, int line) {//coloca o cursor no local desejado
    COORD coord = { column,line };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void getCurrentCursorPosition(int* x, int* y) {//recebe duas variaveis e guarda nelas a posicao atual do cursor
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(hConsole, &csbi)) {
        *x = csbi.dwCursorPosition.X;
        *y = csbi.dwCursorPosition.Y;
    }
}

DWORD WINAPI ThreadConsumidor(LPVOID param) {
    GAME* game = (GAME*)param;
    while (!game->dwShutDown) {
        WaitForSingleObject(game->hSemLeitura, INFINITE);
        WaitForSingleObject(game->hMutex, INFINITE);
        _tprintf(_T("\nRecebi: %s"), game->sharedMemory->bufferCircular.buffer[game->sharedMemory->bufferCircular.dwPosL].command);
       // game->sharedMemory->bufferCircular.dwPosL++; //nao percebi poque nao funciona

        //se apos o incremento a posicao de leitura chegar ao fim, tenho de voltar ao inicio
        if (game->sharedMemory->bufferCircular.dwPosL == TAM_BUF)
            game->sharedMemory->bufferCircular.dwPosL = 0;
        ReleaseMutex(game->hMutex);
        ReleaseSemaphore(game->hSemEscrita, 1, NULL);
    }
    ExitThread(0);
}

DWORD WINAPI CMDThread(LPVOID param) {
    GAME* game = (GAME*)param;
    DWORD x, y, written, value;
    COORD pos;
    TCHAR command_line[TAM];
    TCHAR cmd[TAM];
    getCurrentCursorPosition(&x, &y);
    while (!game->dwShutDown) {
        fflush(stdout); fflush(stdin);
        GoToXY(0, 0);
        _tprintf(_T("Velocidade: %u Numero de estradas: %u \n\n"), game->dwInitSpeed, game->dwInitNumOfRoads);
        getCurrentCursorPosition(&x, &y);
        GoToXY(x, y);
        pos.X = x; pos.Y = y;
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        _tprintf(_T("[SERVER]$ "));
        _fgetts(command_line, TAM, stdin);
        _stscanf_s(command_line, _T("%s %u"), cmd, TAM, &value);

        if (!_tcscmp(cmd, _T("exit"))) { game->dwShutDown = 1; break; }

        if (!(_tcscmp(cmd, _T("setf")))) {
            if (value < 1 || value > MAX_ROADS) {
                _tprintf(_T("Insira um valor entre 1 e 8\n"));
            }
            else {
                game->dwInitNumOfRoads = value;
                if (RegSetValueEx(game->hKey, KEY_ROADS, 0, REG_DWORD, (LPCBYTE)&value, sizeof(value)) != ERROR_SUCCESS)
                    _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            }
        }
        else if (!(_tcscmp(cmd, _T("setv")))) {
            if (value < 0||value>5) continue;
            game->dwInitSpeed = value;
            if (RegSetValueEx(game->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&value, sizeof(value)) != ERROR_SUCCESS)
                _tprintf(TEXT("[AVISO] O atributo nao foi alterado nem criado!\n"));
        }
        else if (!(_tcscmp(cmd, _T("help")))) {
            _tprintf(_T("\n-- setv x : definir a velocidade dos carros ( x inteiro maior que 1)"));
            _tprintf(_T("\n-- setf x : definir o numero de faixas ( x inteiro entre 1 e 8)"));
            _tprintf(_T("\n-- exit : sair\n\n"));
        }
    }

    ExitThread(0);
}

DWORD WINAPI UpdateThread(LPVOID param) {
    GAME* game = (GAME*)param;

    while (!game->dwShutDown) {
        if (game->dwInitNumOfRoads != game->sharedMemory->sharedBoard.dwHeight - 4) {
            initSharedBoard(&game->sharedMemory->sharedBoard,game->dwInitNumOfRoads+4);
        }
        else {
            for (int i = 0; i < game->dwInitNumOfRoads; i++) {
                for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                    OBJECT obj = game->roads[i].cars[j];
                    game->sharedMemory->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;
                    game->sharedMemory->sharedBoard.board[obj.dwLastY][obj.dwLastX] = _T(' ');
                }
            }
        }
    }
    ExitThread(0);
}

DWORD WINAPI RoadMove(LPVOID param) {

    ROAD* road = (ROAD*)param;

    HANDLE hUpdateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hUpdateEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(7);
    }
    int runningCars = 0, numSteps = 0;
    while (TRUE) {
        WaitForSingleObject(road->hMutex, INFINITE);
        if (numSteps % road->dwSpaceBetween == 0 && runningCars < road->dwNumOfCars) { numSteps = 0; runningCars++; }
        for (int i = 0; i < runningCars; i++) {
            moveObject(&road->cars[i], road->way);
        }
        numSteps++;
        SetEvent(hUpdateEvent);
        ReleaseMutex(road->hMutex);
        Sleep(road->dwSpeed);
    }
    ExitThread(0);
}

void moveObject(OBJECT* objData, WAY way) {
    objData->dwLastX = objData->dwX;
    objData->dwLastY = objData->dwY;

    switch (way) {
        case UP: {
            if (objData->dwY - 1 < 0)break;
            objData->dwY--;
            break;
        }
        case DOWN: {
            if (objData->dwY + 1 > MAX_ROADS)break;
            objData->dwY++;
            break;
        }
        case LEFT: {
            if (objData->dwX - 1 <= 0) objData->dwX = MAX_WIDTH;
            objData->dwX--;
            break;
        }
        case RIGHT: {
            if (objData->dwX + 1 >= MAX_WIDTH) objData->dwX = 0;
            objData->dwX++;
            break;
        }
    }

}

void initSharedBoard(SHARED_BOARD* sharedBoard, DWORD dwHeight) {
    sharedBoard->dwHeight = dwHeight;
    sharedBoard->dwWidth = MAX_WIDTH;

    for (int i = 0; i < sharedBoard->dwHeight; i++) {
        for (int j = 0; j < sharedBoard->dwWidth; j++) {
            if (i == 1 || i == sharedBoard->dwHeight - 2) {
                sharedBoard->board[i][j] = _T('-');
            }
            else {
                sharedBoard->board[i][j] = _T(' ');
            }
        }
    }

}
void initRegestry(GAME* data) {
    DWORD estado;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, KEY_DIR, 0, KEY_ALL_ACCESS, &(data->hKey)) != ERROR_SUCCESS) {

        if (RegCreateKeyEx(HKEY_CURRENT_USER, KEY_DIR, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &(data->hKey), &estado) != ERROR_SUCCESS) {
            _tprintf(TEXT("\n[ERRO]Chave nao foi nem criada nem aberta! ERRO!"));
            ExitProcess(1);
        }
        else {
            _tprintf(_T("\n[AVISO] É necessário definir valores de velocidade e de número de faixas antes de começar!\n"));
            do {
                _tprintf(_T("Velocidade:"));
                _tscanf_s(_T("%u"), &data->dwInitSpeed);
            } while (data->dwInitSpeed <= 0);

            if (RegSetValueEx(data->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&data->dwInitSpeed, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %d\n"), data->dwInitSpeed);

            do {
                _tprintf(_T("\nNúmero de faixas (MAX 8):"));
                _tscanf_s(_T("%u"), &data->dwInitNumOfRoads);
            } while (data->dwInitNumOfRoads > MAX_ROADS || data->dwInitNumOfRoads <= 0);

            if (RegSetValueEx(data->hKey, KEY_ROADS, 0, REG_DWORD, (LPCBYTE)&data->dwInitNumOfRoads, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Faixas = %u\n"), data->dwInitNumOfRoads);
        }
    }
    else {
        DWORD dwSize = sizeof(DWORD);
        if (RegQueryValueExW(data->hKey, KEY_SPEED, NULL, NULL, (LPBYTE) & (data->dwInitSpeed), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("Não foi possivel obter o valor da velocidade por favor insira um novo\n"));
        }
        if (RegQueryValueExW(data->hKey, KEY_ROADS, NULL, NULL, (LPBYTE) & (data->dwInitNumOfRoads), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("Não foi possivel obter o valor das faixas por favor insira um novo\n"));
        }
    }
}
void initRoads(ROAD* roads, DWORD dwInitSpeed) {

    for (int i = 0; i < MAX_ROADS; i++) {
        roads[i].dwNumOfCars = rand() % MAX_CARS_PER_ROAD + 1;
        roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / MAX_CARS_PER_ROAD) + 2;
        roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
        roads[i].dwSpeed = dwInitSpeed * 100;
        roads[i].hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoadMove, (LPVOID)&roads[i], CREATE_SUSPENDED, NULL);
        for (int j = 0; j < roads[i].dwNumOfCars; j++) {
            roads[i].cars[j].c = CAR;
            roads[i].cars[j].dwX = (roads[i].way == RIGHT ? 0 : MAX_WIDTH);
            roads[i].cars[j].dwY = i + 2;
            roads[i].cars[j].dwLastX = roads[i].cars[j].dwX;
            roads[i].cars[j].dwLastY = roads[i].cars[j].dwY;
        }
    }
}
void initBufCir(GAME* game) {
    game->hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    game->hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    game->hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR);

    if (game->hSemEscrita == NULL || game->hSemLeitura == NULL || game->hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        exit(-1);
    }

    game->sharedMemory->bufferCircular.dwNumCons = 0;
    game->sharedMemory->bufferCircular.dwNumProd = 0;
    game->sharedMemory->bufferCircular.dwPosE = 0;
    game->sharedMemory->bufferCircular.dwPosL = 0;

    game->dwShutDown = 0;
    //temos de usar o mutex para aumentar o nConsumidores para termos os ids corretos
    WaitForSingleObject(game->hMutex, INFINITE);
    game->sharedMemory->bufferCircular.dwNumCons++;
    ReleaseMutex(game->hMutex);


}
int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hExistServer, hUpdateEvent, hExitEvent;
    HANDLE hFileMap;
    HANDLE hUpdateThread, hCMDThread, hThreadConsumidor;
    HANDLE hMutexRoad;
    GAME game;
#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    //So permite 1 servidor a correr
    hExistServer = CreateMutex(NULL, TRUE, MUTEX_SERVER);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        _tprintf(_T("[ERRO] Ja existe um servidor a correr\n"));
        ExitProcess(1);
    }
    hExitEvent = CreateEvent(NULL, TRUE, FALSE, EXIT_EVENT);
    if (hExitEvent == NULL) {
        _tprintf(_T("[ERRO] Evento não criado\n\n"));
        ExitProcess(1);
    }
    srand(time(NULL));
    hUpdateEvent = CreateEvent(NULL, TRUE, FALSE, UPDATE_EVENT);
    if (hUpdateEvent == NULL) {
        _tprintf(_T("[ERRO] Evento não criado\n\n"));
        exit(-1);
    }

    //Memoria partilhada
    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hFileMap == NULL) {
        hFileMap = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(SHARED_MEMORY),
            SHARED_MEMORY_NAME);

        if (hFileMap == NULL) {
            _tprintf(TEXT("Erro no CreateFileMapping\n"));
            return -1;
        }
    }
    game.sharedMemory = (SHARED_MEMORY*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (game.sharedMemory == NULL) {
        _tprintf(_T("Erro no mapviewoffile\n"));
        exit(-1);
    }
    //inicializacao
    game.dwShutDown = 0;
    game.dwLevel = 1;
    initRegestry(&game);
    initSharedBoard(&game.sharedMemory->sharedBoard, game.dwInitNumOfRoads + 4);
    initRoads(game.roads, game.dwInitSpeed);
    initBufCir(&game);

    hMutexRoad = CreateMutex(NULL, FALSE, MUTEX_ROAD);
    for (int i = 0; i < MAX_ROADS; i++) {
        game.roads[i].hMutex = hMutexRoad;
        ResumeThread(game.roads[i].hThread);
    }

    //threads
    hCMDThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, (LPVOID)&game, 0, NULL);
    hUpdateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, (LPVOID)&game, 0, NULL);
    hThreadConsumidor = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadConsumidor, (LPVOID)&game, 0, NULL);
    WaitForSingleObject(hUpdateThread, INFINITE);
    WaitForSingleObject(hCMDThread, INFINITE);

    SetEvent(hExitEvent);
    UnmapViewOfFile(game.sharedMemory);
    CloseHandle(hFileMap);
    CloseHandle(game.hKey);
    CloseHandle(hExitEvent);
    CloseHandle(hMutexRoad);
    CloseHandle(hUpdateEvent);
    CloseHandle(hExistServer);
}