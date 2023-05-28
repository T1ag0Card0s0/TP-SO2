// dllmain.cpp : Define o ponto de entrada para o aplicativo DLL.
#include "dll.h"

int consumidor(GAME* game) {
    HANDLE hFileMap;
    initGameData(game, &hFileMap);
    initSharedBoard(&game->sharedData.memPar->sharedBoard, game->dwInitNumOfRoads + 4);
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
    UnmapViewOfFile(game->sharedData.memPar);
    CloseHandle(hFileMap);
    return 0;
}


int produtor(SHARED_DATA* dados) { 

   HANDLE hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hFileMap == NULL) {
        _tprintf(_T("Erro a abrir o fileMapping \n"));
        return -1;
    }
    //mapeamos o bloco de memoria para o espaco de enderaçamento do nosso processo
    dados->memPar = (SHARED_MEMORY*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (dados->memPar == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }
    dados->hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    //criar um semafro para a leitura
    dados->hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    //criar mutex para os produtores
    dados->hMutex = CreateMutex(NULL, FALSE, MUTEX_PROD);
    if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL || dados->hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        return -1;
    }

    dados->terminar = 0;

    //temos de usar o mutex para aumentar o nProdutores para termos os ids corretos
    WaitForSingleObject(dados->hMutex, INFINITE);
    dados->memPar->bufferCircular.nProdutores++;
    dados->id = dados->memPar->bufferCircular.nProdutores;
    ReleaseMutex(dados->hMutex);
    
    COORD pos = { 0,0 };
    DWORD written;
    CELULA_BUFFER cel;
    TCHAR cmd[TAM];
    while (!dados->terminar) {
        cel.id = dados->id;
        fflush(stdin); fflush(stdout);
        GoToXY(pos.X, pos.Y);
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        _tprintf(_T("[OPERADOR]$ "));
        _fgetts(cel.str, TAM, stdin);
        _stscanf_s(cel.str, _T("%s"), cmd, TAM);

        //esperamos por uma posicao para escrevermos
        WaitForSingleObject(dados->hSemEscrita, INFINITE);
        //esperamos que o mutex esteja livre
        WaitForSingleObject(dados->hMutex, INFINITE);

        CopyMemory(&dados->memPar->bufferCircular.buffer[dados->memPar->bufferCircular.posE], &cel, sizeof(CELULA_BUFFER));
        dados->memPar->bufferCircular.posE++; //incrementamos a posicao de escrita para o proximo produtor escrever na posicao seguinte

        //se apos o incremento a posicao de escrita chegar ao fim, tenho de voltar ao inicio
        if (dados->memPar->bufferCircular.posE == TAM_BUF)
            dados->memPar->bufferCircular.posE = 0;
        //libertamos o mutex
        ReleaseMutex(dados->hMutex);
        //libertamos o semaforo para leitura
        ReleaseSemaphore(dados->hSemLeitura, 1, NULL);
        if (!_tcscmp(cmd, _T("exit")))dados->terminar = 1;

    }
    UnmapViewOfFile(dados->memPar);
    CloseHandle(hFileMap);
    return 0; 
}


int update_board(GAME* game) {
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

        for (int i = 0; i < game->dwInitNumOfRoads; i++) {
            for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                OBJECT obj = game->roads[i].cars[j];
                game->sharedData.memPar->sharedBoard.board[obj.dwLastY][obj.dwLastX] = _T(' ');
                game->sharedData.memPar->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;

            }
            if (game->roads[i].dwNumOfObjects != 0) {
                for (int j = 0; j < game->roads[i].dwNumOfObjects; j++) {
                    OBJECT obj = game->roads[i].objects[j];
                    game->sharedData.memPar->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;
                }
            }
        }
        for (int i = 0; i < MAX_PLAYERS; i++) {
            game->sharedData.memPar->sharedBoard.board[game->players[i].dwY][game->players[i].dwX] = game->players[i].c;
        }

        //ResetEvent(hEvent);

    }
    return 0;
}


void commandExecutor(TCHAR command[], ROAD* road) {
    TCHAR cmd[TAM];
    DWORD index;
    _stscanf_s(command, _T("%s %u"), cmd, TAM, &index);

    if (index < 0 || index >= MAX_ROADS) return;

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
        road[index].objects[num].dwX = (rand() % (MAX_WIDTH - 4)) + 3;
        road[index].objects[num].dwY = index + 2;
    }
    else if (!_tcscmp(cmd, _T("invert"))) {
        if (road[index].way == LEFT)road[index].way = RIGHT;
        else if (road[index].way == RIGHT)road[index].way = LEFT;
    }
}
void initGameData(GAME* game,HANDLE *hFileMap){
    //criar semaforo que conta as escritas
    game->sharedData.hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    //criar semaforo que controla a leitura
    game->sharedData.hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    game->sharedData.hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR);
    if (game->sharedData.hSemEscrita == NULL || game->sharedData.hSemLeitura == NULL || game->sharedData.hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        return -1;
    }

    *hFileMap = CreateFileMapping(
        INVALID_HANDLE_VALUE,
        NULL,
        PAGE_READWRITE,
        0,
        sizeof(SHARED_MEMORY), //tamanho da memoria partilhada
        SHARED_MEMORY_NAME);//nome do filemapping. nome que vai ser usado para partilha entre processos
    if (*hFileMap == NULL) {
        _tprintf(TEXT("Erro no CreateFileMapping\n"));
        return -1;
    }

    game->sharedData.memPar = (SHARED_MEMORY*)MapViewOfFile(*hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (game->sharedData.memPar == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }

    game->sharedData.memPar->bufferCircular.nConsumidores = 0;
    game->sharedData.memPar->bufferCircular.nProdutores = 0;
    game->sharedData.memPar->bufferCircular.posE = 0;
    game->sharedData.memPar->bufferCircular.posL = 0;
    game->sharedData.terminar = 0;

    //temos de usar o mutex para aumentar o nConsumidores para termos os ids corretos
    WaitForSingleObject(game->sharedData.hMutex, INFINITE);
    game->sharedData.memPar->bufferCircular.nConsumidores++;
    game->sharedData.id = game->sharedData.memPar->bufferCircular.nConsumidores;
    ReleaseMutex(game->sharedData.hMutex);

    game->dwShutDown = 0;
    game->dwLevel = 1;
    game->bPaused = FALSE;
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

void GoToXY(int column, int line) {//coloca o cursor no local desejado
    COORD coord = { column,line };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

