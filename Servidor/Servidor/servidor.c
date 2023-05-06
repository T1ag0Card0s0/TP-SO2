
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
#define BOARD_SHARED_MEMORY _T("ShareBoard")
#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")
#define MUTEX_ROAD _T("MutexRoad")
#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_CONSUMIDOR _T("MutexConsumidor")
#define CMD_FILE_MAP _T("CMDFileMap")

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

    SHARED_BOARD* sharedBoard;
    ROAD roads[MAX_ROADS];
}GAME;

typedef struct CELULA_BUFFER {
    DWORD id;
    TCHAR command[TAM];
}CELULA_BUFFER;
typedef struct BUFFER_CIRCULAR {
    DWORD dwNumProd;
    DWORD dwNumCons;
    DWORD dwPosE;//proxima posicao de escrita
    DWORD dwPosL;//proxima posicao de leitura

    CELULA_BUFFER buffer[TAM_BUF];
}BUFFER_CIRCULAR;

//estrutura de apoio
typedef struct DADOS_THREAD {
    BUFFER_CIRCULAR* memPartilhada;
    HANDLE hSemEscrita;//handle para o semaforo que controla as escritas (controla quantas posicoes estao vazias)
    HANDLE hSemLeitura; //handle para o semaforo que controla as leituras (controla quantas posicoes estao preenchidas)
    HANDLE hMutex;

    DWORD dwTerminar;//1 = sair, 0 = continuar
    DWORD dwId;
}DADOS_THREAD;

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
    DADOS_THREAD* dados = (DADOS_THREAD*)param;
    CELULA_BUFFER cel;
    int contador = 0;
    int soma = 0;

    while (!dados->dwTerminar) {
        WaitForSingleObject(dados->hSemLeitura, INFINITE);
        WaitForSingleObject(dados->hMutex, INFINITE);

        CopyMemory(&cel, &dados->memPartilhada->buffer[dados->memPartilhada->dwPosL], sizeof(CELULA_BUFFER));
        dados->memPartilhada->dwPosL++; //incrementamos a posicao de leitura para o proximo consumidor ler na posicao seguinte

        //se apos o incremento a posicao de leitura chegar ao fim, tenho de voltar ao inicio
        if (dados->memPartilhada->dwPosL == TAM_BUF)
            dados->memPartilhada->dwPosL = 0;
        _tprintf(_T("\nRecebi: %s"), cel.command);
        ReleaseMutex(dados->hMutex);
        ReleaseSemaphore(dados->hSemEscrita, 1, NULL);
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
        for (int i = 0; i < game->dwInitNumOfRoads; i++) {
            for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                OBJECT obj = game->roads[i].cars[j];
                game->sharedBoard->board[obj.dwY][obj.dwX] = obj.c;
                game->sharedBoard->board[obj.dwLastY][obj.dwLastX] = _T(' ');
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
        if (objData->dwX + 1 > MAX_WIDTH) objData->dwX = 0;
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
void initRoads(ROAD* roads, DWORD dwNumOfRoads, DWORD dwInitSpeed) {

    for (int i = 0; i < dwNumOfRoads; i++) {
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
void initBufCir(DADOS_THREAD* dadosThread, HANDLE hFileMap) {
    dadosThread->hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    dadosThread->hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    dadosThread->hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR);

    if (dadosThread->hSemEscrita == NULL || dadosThread->hSemLeitura == NULL || dadosThread->hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        exit(-1);
    }

    dadosThread->memPartilhada = (BUFFER_CIRCULAR*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (dadosThread->memPartilhada == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        exit(-1);
    }

    dadosThread->memPartilhada->dwNumCons = 0;
    dadosThread->memPartilhada->dwNumProd = 0;
    dadosThread->memPartilhada->dwPosE = 0;
    dadosThread->memPartilhada->dwPosL = 0;

    dadosThread->dwTerminar = 0;
    //temos de usar o mutex para aumentar o nConsumidores para termos os ids corretos
    WaitForSingleObject(dadosThread->hMutex, INFINITE);
    dadosThread->memPartilhada->dwNumCons++;
    dadosThread->dwId = dadosThread->memPartilhada->dwNumCons;
    ReleaseMutex(dadosThread->hMutex);


}
int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hExistServer, hUpdateEvent, hExitEvent;
    HANDLE hFileMap, hFileMapBufCir;
    HANDLE hUpdateThread, hCMDThread, hThreadConsumidor;
    HANDLE hMutexRoad;
    GAME game;
    DADOS_THREAD dadosThread;
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
    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, BOARD_SHARED_MEMORY);
    if (hFileMap == NULL) {
        hFileMap = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(SHARED_BOARD),
            BOARD_SHARED_MEMORY);

        if (hFileMap == NULL) {
            _tprintf(TEXT("Erro no CreateFileMapping\n"));
            return -1;
        }
    }
    hFileMapBufCir = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, CMD_FILE_MAP);
    if (hFileMapBufCir == NULL) {
        hFileMapBufCir = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(BUFFER_CIRCULAR),
            CMD_FILE_MAP);

        if (hFileMapBufCir == NULL) {
            _tprintf(TEXT("Erro no CreateFileMapping\n"));
            return -1;
        }
    }

    game.sharedBoard = (SHARED_BOARD*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHARED_BOARD));
    if (game.sharedBoard == NULL) {
        _tprintf(_T("Erro no mapviewoffile\n"));
        exit(-1);
    }
    //inicializacao
    game.dwShutDown = 0;
    game.dwLevel = 1;
    initRegestry(&game);
    initSharedBoard(game.sharedBoard, game.dwInitNumOfRoads + 4);
    initRoads(game.roads, game.dwInitNumOfRoads, game.dwInitSpeed);
    initBufCir(&dadosThread, hFileMapBufCir);

    hMutexRoad = CreateMutex(NULL, FALSE, MUTEX_ROAD);
    for (int i = 0; i < game.dwInitNumOfRoads; i++) {
        game.roads[i].hMutex = hMutexRoad;
        ResumeThread(game.roads[i].hThread);
    }

    //threads
    hCMDThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, (LPVOID)&game, 0, NULL);
    hUpdateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, (LPVOID)&game, 0, NULL);
    hThreadConsumidor = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadConsumidor, (LPVOID)&dadosThread, 0, NULL);
    WaitForSingleObject(hUpdateThread, INFINITE);
    WaitForSingleObject(hCMDThread, INFINITE);

    SetEvent(hExitEvent);
    UnmapViewOfFile(game.sharedBoard);
    CloseHandle(hFileMap);
    CloseHandle(hFileMapBufCir);
    CloseHandle(game.hKey);
    CloseHandle(hExitEvent);
    CloseHandle(hMutexRoad);
    CloseHandle(hUpdateEvent);
    CloseHandle(hExistServer);
}