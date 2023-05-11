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

typedef struct GAME {
    DWORD dwLevel;
    DWORD dwShutDown;
    DWORD dwInitSpeed, dwInitNumOfRoads;
    BOOL  bPaused;

    HANDLE hKey;

    SHARED_DATA sharedData;
    OBJECT players[MAX_PLAYERS];
    ROAD roads[MAX_ROADS];
}GAME;

void initRoads(ROAD* roads, DWORD dwNumOfRoads);
void initSharedBoard(SHARED_BOARD* sharedBoard, DWORD dwHeight);
void initRegestry(GAME* data);
void moveObject(OBJECT* objData, WAY way);
void commandExecutor(TCHAR command[], ROAD* road);
void initPlayers(OBJECT* players, DWORD dwNumRoads);
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
    SHARED_DATA* dados = &(game->sharedData);
    CELULA_BUFFER cel;

    while (!dados->terminar) {
        //esperamos por uma posicao para lermos
        WaitForSingleObject(dados->hSemLeitura, INFINITE);
        //esperamos que o mutex esteja livre
        WaitForSingleObject(dados->hMutex, INFINITE);

        CopyMemory(&cel, &dados->memPar->bufferCircular.buffer[dados->memPar->bufferCircular.posL], sizeof(CELULA_BUFFER));
        dados->memPar->bufferCircular.posL++; //incrementamos a posicao de leitura para o proximo consumidor ler na posicao seguinte
        //se apos o incremento a posicao de leitura chegar ao fim, tenho de voltar ao inicio
        if (dados->memPar->bufferCircular.posL == TAM_BUF)
            dados->memPar->bufferCircular.posL = 0;

        //libertamos o mutex
        ReleaseMutex(dados->hMutex);
        //libertamos o semaforo. temos de libertar uma posicao de escrita
        ReleaseSemaphore(dados->hSemEscrita, 1, NULL);

        commandExecutor(cel.str, game->roads);
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
            if (value < 0 || value>5) continue;
            game->dwInitSpeed = value;
            if (RegSetValueEx(game->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&value, sizeof(value)) != ERROR_SUCCESS)
                _tprintf(TEXT("[AVISO] O atributo nao foi alterado nem criado!\n"));
        }
        else if (!_tcscmp(cmd, _T("pause"))) {
            if (game->bPaused)continue;
            game->bPaused = TRUE;
            for (int i = 0; i < MAX_ROADS; i++) {
                game->roads[i].lastWay = game->roads[i].way;
                game->roads[i].way = STOP;
            }
        }
        else if (!_tcscmp(cmd, _T("unpause"))) {
            if (!game->bPaused)continue;
            game->bPaused = FALSE;
            for (int i = 0; i < MAX_ROADS; i++) {
                game->roads[i].way = game->roads[i].lastWay;
            }
        }
        else if (!_tcscmp(cmd, _T("restart"))) {
            //implementar o comando restart
        }
        else if (!(_tcscmp(cmd, _T("help")))) {
            _tprintf(_T("\n-- setv x : definir a velocidade dos carros ( x inteiro maior que 1)"));
            _tprintf(_T("\n-- setf x : definir o numero de faixas ( x inteiro entre 1 e 8)"));
            _tprintf(_T("\n-- pause : pausar o jogo"));
            _tprintf(_T("\n-- unpause : resumir o jogo"));
            _tprintf(_T("\n-- restart : recomeçar o jogo"));
            _tprintf(_T("\n-- exit : sair\n\n"));
        }
    }

    ExitThread(0);
}


DWORD WINAPI UpdateThread(LPVOID param) {
    GAME* game = (GAME*)param;
    int i, j;
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(-1);
    }
    while (!game->dwShutDown) {
        WaitForSingleObject(hEvent, INFINITE);
        if (game->dwInitNumOfRoads != game->sharedData.memPar->sharedBoard.dwHeight - 4 ||
            game->dwInitSpeed * 100 != game->roads[0].dwSpeed) {

            initSharedBoard(&game->sharedData.memPar->sharedBoard, game->dwInitNumOfRoads + 4);
            for (int i = 0; i < game->dwInitNumOfRoads; i++) {
                game->roads[i].dwSpeed = game->dwInitSpeed * 100;
            }
            initPlayers(game->players, game->dwInitNumOfRoads);

            continue;
        }

        for (i = 0; i < game->dwInitNumOfRoads; i++) {
            for (j = 0; j < game->roads[i].dwNumOfCars; j++) {
                OBJECT obj = game->roads[i].cars[j];
                game->sharedData.memPar->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;
                game->sharedData.memPar->sharedBoard.board[obj.dwLastY][obj.dwLastX] = _T(' ');
            }
        }

        for (i = 0; i < MAX_PLAYERS; i++) {
            game->sharedData.memPar->sharedBoard.board[game->players[i].dwY][game->players[i].dwX] = game->players[i].c;
        }

        for (i = 0; i < game->dwInitNumOfRoads; i++) {
            if (game->roads[i].dwNumOfObjects != 0) {
                for (j = 0; j < game->roads[i].dwNumOfObjects; j++) {
                    OBJECT obj = game->roads[i].objects[j];
                    game->sharedData.memPar->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;
                }
            }
        }

        ResetEvent(hEvent);

    }
    ExitThread(0);
}

DWORD WINAPI RoadMove(LPVOID param) {
    ROAD* road = (ROAD*)param;
    BOOL bNextToObstacle = FALSE;
    HANDLE hUpdateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hUpdateEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(-1);
    }
    int runningCars = 0, numSteps = 0;
    while (TRUE) {
        WaitForSingleObject(road->hMutex, INFINITE);
        if (numSteps % road->dwSpaceBetween == 0 && runningCars < road->dwNumOfCars){
            numSteps = 0;
            runningCars++;
        }
        for (int i = 0; i < runningCars; i++) {

            for (int j = 0; j < road->dwNumOfObjects; j++) {
                if (road->objects[j].dwX - 1 == road->cars[i].dwX && road->way == RIGHT)
                    bNextToObstacle = TRUE;
                else if (road->objects[j].dwX + 1 == road->cars[i].dwX && road->way == LEFT)
                    bNextToObstacle = TRUE;
            }

            for (int j = 0; j < runningCars; j++) {
                if (road->cars[j].dwX - 1 == road->cars[i].dwX && road->way == RIGHT)
                    bNextToObstacle = TRUE;
                else if (road->cars[j].dwX + 1 == road->cars[i].dwX && road->way == LEFT)
                    bNextToObstacle = TRUE;
            }

            if (!bNextToObstacle)moveObject(&road->cars[i], road->way);
            else bNextToObstacle = FALSE;
        }

        numSteps++;
        SetEvent(hUpdateEvent);
        ReleaseMutex(road->hMutex);
        Sleep(road->dwSpeed);

        if (road->dwTimeStoped > 0) {
            Sleep(road->dwTimeStoped * 1000);
            road->dwTimeStoped = 0;
            road->way = road->lastWay;
        }
    }
    ExitThread(0);
}

void moveObject(OBJECT* objData, WAY way) {
    if (way != STOP) {
        objData->dwLastX = objData->dwX;
        objData->dwLastY = objData->dwY;
    }

    switch (way) {
    case UP: {
        if (objData->dwY - 1 <= 0)break;
        objData->dwY--;
        break;
    }
    case DOWN: {
        if (objData->dwY + 1 >= MAX_ROADS)break;
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
void commandExecutor(TCHAR command[], ROAD* road) {
    TCHAR cmd[TAM];
    DWORD index;
    _stscanf_s(command, _T("%s %u"), cmd, TAM, &index);

    if (index<0 || index>=MAX_ROADS) return;

    if (!_tcscmp(cmd, _T("pause"))) {
        DWORD value;
        _stscanf_s(command, _T("%s %u %u"), cmd, TAM, &index, &value);
        road[index].lastWay = road[index].way;
        road[index].way = STOP;
        road[index].dwTimeStoped = value;
    }
    else if (!_tcscmp(cmd, _T("insert"))) {
        DWORD num = road[index].dwNumOfObjects++;
        road[index].objects[num].c = _T('X');
        road[index].objects[num].dwX = rand() % MAX_WIDTH;
        road[index].objects[num].dwY = index+2 ;
    }
    else if (!_tcscmp(cmd, _T("invert"))) {
        if (road[index].way == LEFT)road[index].way = RIGHT;
        else if (road[index].way == RIGHT)road[index].way = LEFT;
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
        roads[i].dwNumOfObjects = 0;
        roads[i].dwNumOfCars = rand() % MAX_CARS_PER_ROAD + 1;
        roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / MAX_CARS_PER_ROAD) + 2;
        roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
        roads[i].dwSpeed = dwInitSpeed * 100;
        roads[i].dwTimeStoped = 0;
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

void initPlayers(OBJECT* players, DWORD dwNumRoads) {
    int xRepeated = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].c = FROG;
        players[i].dwX = rand() % MAX_WIDTH;
        if (xRepeated == -1) xRepeated = players[i].dwX;
        else if (players[i].dwX == xRepeated) players[i].dwX = ++xRepeated;
        players[i].dwY = dwNumRoads + 3;
        players[i].dwLastX = players[i].dwX;
        players[i].dwLastY = players[i].dwY;
    }
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
#endif
    //Controla quantos servidores podem correr
    hExistServer = CreateMutex(NULL, TRUE, MUTEX_SERVER);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        _tprintf(_T("[ERRO] Ja existe um servidor a correr\n"));
        ExitProcess(1);
    }
    //Evento para assinalar os operadores que o servidor fechou
    hExitEvent = CreateEvent(NULL, TRUE, FALSE, EXIT_EVENT);
    if (hExitEvent == NULL) {
        _tprintf(_T("[ERRO] Evento não criado\n\n"));
        ExitProcess(1);
    }
    //Evento para assinalar que houve uma atualização do 'tabuleiro'
    hUpdateEvent = CreateEvent(NULL, TRUE, FALSE, UPDATE_EVENT);
    if (hUpdateEvent == NULL) {
        _tprintf(_T("[ERRO] Evento não criado\n\n"));
        ExitProcess(1);
    }
    srand(time(NULL));
    //criar semaforo que conta as escritas
    game.sharedData.hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    //criar semaforo que controla a leitura
    game.sharedData.hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    game.sharedData.hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR);
    if (game.sharedData.hSemEscrita == NULL || game.sharedData.hSemLeitura == NULL || game.sharedData.hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        return -1;
    }

    hFileMap = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SHARED_MEMORY), //tamanho da memoria partilhada
        SHARED_MEMORY_NAME);//nome do filemapping. nome que vai ser usado para partilha entre processos
    if (hFileMap == NULL) {
        _tprintf(TEXT("Erro no CreateFileMapping\n"));
        return -1;
    }

    game.sharedData.memPar = (SHARED_MEMORY*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (game.sharedData.memPar == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }

    game.sharedData.memPar->bufferCircular.nConsumidores = 0;
    game.sharedData.memPar->bufferCircular.nProdutores = 0;
    game.sharedData.memPar->bufferCircular.posE = 0;
    game.sharedData.memPar->bufferCircular.posL = 0;
    game.sharedData.terminar = 0;

    //temos de usar o mutex para aumentar o nConsumidores para termos os ids corretos
    WaitForSingleObject(game.sharedData.hMutex, INFINITE);
    game.sharedData.memPar->bufferCircular.nConsumidores++;
    game.sharedData.id = game.sharedData.memPar->bufferCircular.nConsumidores;
    ReleaseMutex(game.sharedData.hMutex);

    game.dwShutDown = 0;
    game.dwLevel = 1;
    game.bPaused = FALSE;
    initRegestry(&game);
    initSharedBoard(&game.sharedData.memPar->sharedBoard, game.dwInitNumOfRoads + 4);
    initRoads(game.roads, game.dwInitSpeed);
    initPlayers(game.players, game.dwInitNumOfRoads);
    //lançamos as estradas ao mesmo tempo
    hMutexRoad = CreateMutex(NULL, FALSE, MUTEX_ROAD);
    for (int i = 0; i < MAX_ROADS; i++) {
        game.roads[i].hMutex = hMutexRoad;
        ResumeThread(game.roads[i].hThread);
    }

    //lancamos a thread
    hCMDThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, (LPVOID)&game, 0, NULL);
    hUpdateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, (LPVOID)&game, 0, NULL);
    hThreadConsumidor = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadConsumidor, (LPVOID)&game, 0, NULL);

    WaitForSingleObject(hUpdateThread, INFINITE);
    WaitForSingleObject(hCMDThread, INFINITE);

    UnmapViewOfFile(game.sharedData.memPar);
    SetEvent(hExitEvent);
    CloseHandle(hFileMap);
    CloseHandle(game.hKey);
    CloseHandle(hExitEvent);
    CloseHandle(hMutexRoad);
    CloseHandle(hUpdateEvent);
    CloseHandle(hExistServer);

    return 0;
}