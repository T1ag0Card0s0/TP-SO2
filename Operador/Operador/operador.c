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

typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;

    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;
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
typedef struct SHARED_MEMORY {
    BUFFER_CIRCULAR bufferCircular;
    SHARED_BOARD sharedBoard;
}SHARED_MEMORY;

//estrutura de apoio
typedef struct SHARED_DATA {
    SHARED_MEMORY* sharedMemory; //ponteiro para a memoria partilhada
    HANDLE hSemEscrita; //handle para o semaforo que controla as escritas (controla quantas posicoes estao vazias)
    HANDLE hSemLeitura; //handle para o semaforo que controla as leituras (controla quantas posicoes estao preenchidas)
    HANDLE hMutex;
    int terminar; // 1 para sair, 0 em caso contr?rio
    int id;
}SHARED_DATA;

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

DWORD WINAPI ShowBoard(LPVOID param) {

    SHARED_BOARD* sharedBoard = (SHARED_BOARD*)param;

    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(-1);
    }
    COORD pos = { 0,1 };
    DWORD written;
    while (TRUE) {
        fflush(stdin); fflush(stdout);

        for (int i = 0; i < sharedBoard->dwHeight; i++) {
            WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), sharedBoard->board[i], sharedBoard->dwWidth, pos, &written);
            pos.Y++;
        }
        WaitForSingleObject(hEvent, INFINITE);
        pos.Y = 1;
        for (int i = 0; i < MAX_ROADS + 4; i++) {
            FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
            pos.Y++;
        }
        ResetEvent(hEvent);
        pos.Y = 1;

    }
    ExitThread(0);
}

DWORD WINAPI CheckIfServerExit(LPVOID param) {
    DWORD* shutdown = (DWORD*)param;
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EXIT_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(7);
    }
    // Esperar pelo evento
    WaitForSingleObject(hEvent, INFINITE);
   * shutdown = 1;
    _tprintf(_T("Desconectado...\n"));
    // Server saiu entao sai
    CloseHandle(hEvent);
    ExitProcess(0);
}


DWORD WINAPI ThreadProdutor(LPVOID param) {
    SHARED_DATA* dados = (SHARED_DATA*)param;
    COORD pos = { 0,0 };
    DWORD written;
    CELULA_BUFFER cel;
    while (!dados->terminar) {
        cel.id = dados->id;
        fflush(stdin); fflush(stdout);
        GoToXY(pos.X, pos.Y);
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        _tprintf(_T("[OPERADOR]$ "));
        _tcscanf_s(_T("%s"), cel.str, TAM);

        //esperamos por uma posicao para escrevermos
        WaitForSingleObject(dados->hSemEscrita, INFINITE);
        //esperamos que o mutex esteja livre
        WaitForSingleObject(dados->hMutex, INFINITE);

        CopyMemory(&dados->sharedMemory->bufferCircular.buffer[dados->sharedMemory->bufferCircular.posE], &cel, sizeof(CELULA_BUFFER));
        dados->sharedMemory->bufferCircular.posE++; //incrementamos a posicao de escrita para o proximo produtor escrever na posicao seguinte

        //se apos o incremento a posicao de escrita chegar ao fim, tenho de voltar ao inicio
        if (dados->sharedMemory->bufferCircular.posE == TAM_BUF)
            dados->sharedMemory->bufferCircular.posE = 0;
        //libertamos o mutex
        ReleaseMutex(dados->hMutex);
        //libertamos o semaforo para leitura
        ReleaseSemaphore(dados->hSemLeitura, 1, NULL);
        if (!_tcscmp(cel.str, _T("exit")))dados->terminar = 1;

    }
    ExitThread(0);
}

int _tmain(int argc, TCHAR* argv[])
{
    DWORD initX = 0, initY = 0;
    HANDLE hShowBoardTh;
    HANDLE hServerTh;
    HANDLE hThreadProdutor;
    HANDLE hFileMap;
    SHARED_DATA dados;


#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif
    srand(time(NULL));
    //criar semaforo que conta as escritas
    dados.hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    //criar um semafro para a leitura
    dados.hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    //criar mutex para os produtores
    dados.hMutex = CreateMutex(NULL, FALSE, MUTEX_PROD);
    if (dados.hSemEscrita == NULL || dados.hSemLeitura == NULL || dados.hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        return -1;
    }

    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hFileMap == NULL) {
        _tprintf(_T("Erro a abrir o fileMapping \n"));
        return -1;
    }
    //mapeamos o bloco de memoria para o espaco de enderaçamento do nosso processo
    dados.sharedMemory = (SHARED_MEMORY*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (dados.sharedMemory == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }

    dados.terminar = 0;

    //temos de usar o mutex para aumentar o nProdutores para termos os ids corretos
    WaitForSingleObject(dados.hMutex, INFINITE);
    dados.sharedMemory->bufferCircular.nProdutores++;
    dados.id = dados.sharedMemory->bufferCircular.nProdutores;
    ReleaseMutex(dados.hMutex);

    //lancamos a thread
    hShowBoardTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ShowBoard, (LPVOID)&dados.sharedMemory->sharedBoard, 0, NULL);
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, (LPVOID)&dados.terminar, 0, NULL);
    hThreadProdutor = CreateThread(NULL, 0, ThreadProdutor, (LPVOID)&dados, 0, NULL);

    WaitForSingleObject(hThreadProdutor, INFINITE);

    UnmapViewOfFile(dados.sharedMemory);
    CloseHandle(hShowBoardTh);
    CloseHandle(hServerTh);
    CloseHandle(hThreadProdutor);
    ExitProcess(0);
    return 0;

}
