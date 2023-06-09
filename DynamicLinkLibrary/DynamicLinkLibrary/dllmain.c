// dllmain.cpp : Define o ponto de entrada para o aplicativo DLL.
#include "dll.h"

int consumidor(SHARED_DATA* sharedData,ROAD *road,DWORD dwNumRoads) {
    HANDLE hFileMap;
    initGameData(sharedData, &hFileMap);
    initSharedBoard(&sharedData->memPar->sharedBoard, dwNumRoads + 4);
    CELULA_BUFFER cel;

    while (!sharedData->terminar) {
        //esperamos por uma posicao para lermos
        WaitForSingleObject(sharedData->hSemLeitura, INFINITE);
        //esperamos que o mutex esteja livre
        WaitForSingleObject(sharedData->hMutex, INFINITE);

        CopyMemory(&cel, &sharedData->memPar->bufferCircular.buffer[sharedData->memPar->bufferCircular.posL], sizeof(CELULA_BUFFER));
        sharedData->memPar->bufferCircular.posL++; //incrementamos a posicao de leitura para o proximo consumidor ler na posicao seguinte
        //se apos o incremento a posicao de leitura chegar ao fim, tenho de voltar ao inicio
        if (sharedData->memPar->bufferCircular.posL == TAM_BUF)
            sharedData->memPar->bufferCircular.posL = 0;

        //libertamos o mutex
        ReleaseMutex(sharedData->hMutex);
        //libertamos o semaforo. temos de libertar uma posicao de escrita
        ReleaseSemaphore(sharedData->hSemEscrita, 1, NULL);

        commandExecutor(cel.str, road);
    }
    UnmapViewOfFile(sharedData->memPar);
    CloseHandle(hFileMap);
    return 0;
}


int produtor(SHARED_DATA* sharedData) {

    HANDLE hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hFileMap == NULL) {
        _tprintf(_T("Erro a abrir o fileMapping \n"));
        return -1;
    }
    //mapeamos o bloco de memoria para o espaco de enderaçamento do nosso processo
    sharedData->memPar = (SHARED_MEMORY*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (sharedData->memPar == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }
    sharedData->hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    //criar um semafro para a leitura
    sharedData->hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    //criar mutex para os produtores
    sharedData->hMutex = CreateMutex(NULL, FALSE, MUTEX_PROD);
    if (sharedData->hSemEscrita == NULL || sharedData->hSemLeitura == NULL || sharedData->hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        return -1;
    }

    sharedData->terminar = 0;

    //temos de usar o mutex para aumentar o nProdutores para termos os ids corretos
    WaitForSingleObject(sharedData->hMutex, INFINITE);
    sharedData->memPar->bufferCircular.nProdutores++;
    sharedData->id = sharedData->memPar->bufferCircular.nProdutores;
    ReleaseMutex(sharedData->hMutex);

    COORD pos = { 0,0 };
    DWORD written;
    CELULA_BUFFER cel;
    TCHAR cmd[TAM];
    while (!sharedData->terminar) {
        cel.id = sharedData->id;
        fflush(stdin); fflush(stdout);
        GoToXY(pos.X, pos.Y);
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        _tprintf(_T("[OPERADOR]$ "));
        _fgetts(cel.str, TAM, stdin);
        _stscanf_s(cel.str, _T("%s"), cmd, TAM);

        //esperamos por uma posicao para escrevermos
        WaitForSingleObject(sharedData->hSemEscrita, INFINITE);
        //esperamos que o mutex esteja livre
        WaitForSingleObject(sharedData->hMutex, INFINITE);

        CopyMemory(&sharedData->memPar->bufferCircular.buffer[sharedData->memPar->bufferCircular.posE], &cel, sizeof(CELULA_BUFFER));
        sharedData->memPar->bufferCircular.posE++; //incrementamos a posicao de escrita para o proximo produtor escrever na posicao seguinte

        //se apos o incremento a posicao de escrita chegar ao fim, tenho de voltar ao inicio
        if (sharedData->memPar->bufferCircular.posE == TAM_BUF)
            sharedData->memPar->bufferCircular.posE = 0;
        //libertamos o mutex
        ReleaseMutex(sharedData->hMutex);
        //libertamos o semaforo para leitura
        ReleaseSemaphore(sharedData->hSemLeitura, 1, NULL);
        if (!_tcscmp(cmd, _T("exit")))sharedData->terminar = 1;

    }
    UnmapViewOfFile(sharedData->memPar);
    CloseHandle(hFileMap);
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
        if (road[index].way == LEFT) road[index].way = RIGHT;
        else if (road[index].way == RIGHT)road[index].way = LEFT;
    }
}
void initGameData(SHARED_DATA* sharedData, HANDLE* hFileMap) {
    //criar semaforo que conta as escritas
    sharedData->hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    //criar semaforo que controla a leitura
    sharedData->hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    sharedData->hMutex = CreateMutex(NULL, FALSE, MUTEX_CONSUMIDOR);
    if (sharedData->hSemEscrita == NULL || sharedData->hSemLeitura == NULL || sharedData->hMutex == NULL) {
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

    sharedData->memPar = (SHARED_MEMORY*)MapViewOfFile(*hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (sharedData->memPar == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }

    sharedData->memPar->bufferCircular.nConsumidores = 0;
    sharedData->memPar->bufferCircular.nProdutores = 0;
    sharedData->memPar->bufferCircular.posE = 0;
    sharedData->memPar->bufferCircular.posL = 0;
    sharedData->terminar = 0;

    //temos de usar o mutex para aumentar o nConsumidores para termos os ids corretos
    WaitForSingleObject(sharedData->hMutex, INFINITE);
    sharedData->memPar->bufferCircular.nConsumidores++;
    sharedData->id = sharedData->memPar->bufferCircular.nConsumidores;
    ReleaseMutex(sharedData->hMutex);
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

void GoToXY(int column, int line) {//coloca o cursor no local desejado
    COORD coord = { column,line };
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

