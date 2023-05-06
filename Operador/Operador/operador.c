#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#define TAM 200
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20

#define MUTEX_SERVER _T("Server")
#define BOARD_SHARED_MEMORY _T("ShareBoard")
#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")
#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_PROD _T("MutexProd")
#define CMD_FILE_MAP _T("CMDFileMap")
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

typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;

    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;

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

DWORD WINAPI ReadSharedMemory(LPVOID param) {

    SHARED_BOARD* sharedBoard;

    HANDLE hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, BOARD_SHARED_MEMORY);
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
    sharedBoard = (SHARED_BOARD*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SHARED_BOARD));
    if (sharedBoard == NULL) {
        _tprintf(_T("Erro no mapviewoffile\n"));
        exit(-1);
    }
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(7);
    }
    int i = 0;

    COORD pos = { 0,1 };
    DWORD written;
    while (TRUE) {
        fflush(stdin); fflush(stdout);
        WaitForSingleObject(hEvent, INFINITE);
        for (int i = 0; i < sharedBoard->dwHeight; i++) {
            WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), sharedBoard->board[i], sharedBoard->dwWidth, pos, &written);
            pos.Y++;
        }
        ResetEvent(hEvent);
        pos.Y = 1;

    }
    UnmapViewOfFile(sharedBoard);
    CloseHandle(hFileMap);
    ExitThread(0);
}

DWORD WINAPI CheckIfServerExit(LPVOID param) {
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EXIT_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(7);
    }
    // Esperar pelo evento
    WaitForSingleObject(hEvent, INFINITE);
    _tprintf(_T("Desconectado...\n"));
    // Server saiu entao sai
    CloseHandle(hEvent);
    ExitProcess(0);
}

DWORD WINAPI ThreadProdutor(LPVOID param) {
    DADOS_THREAD* dados = (DADOS_THREAD*)param;
    CELULA_BUFFER cel;
    int contador = 0;
    COORD pos = { 0,0 };
    DWORD written;
    while (!dados->dwTerminar) {
        GoToXY(pos.X, pos.Y);
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        cel.id = dados->dwId;
        _tprintf(_T("[OPERADOR]$ "));
        _fgetts(cel.command, TAM, stdin);

        //esperamos por uma posicao para escrevermos
        WaitForSingleObject(dados->hSemEscrita, INFINITE);

        //esperamos que o mutex esteja livre
        WaitForSingleObject(dados->hMutex, INFINITE);

        CopyMemory(&dados->memPartilhada->buffer[dados->memPartilhada->dwPosE], &cel, sizeof(CELULA_BUFFER));
        dados->memPartilhada->dwPosE++;

        if (dados->memPartilhada->dwPosE == TAM_BUF)
            dados->memPartilhada->dwPosE = 0;

        ReleaseMutex(dados->hMutex);
        ReleaseSemaphore(dados->hSemLeitura, 1, NULL);
    }
    ExitThread(0);
}

void initDadosThread(DADOS_THREAD* dados, HANDLE* hFileMap) {
    BOOL primeiroProcesso = FALSE;
    dados->hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);
    dados->hSemLeitura = CreateSemaphore(NULL, 0, 1, READ_SEMAPHORE);
    dados->hMutex = CreateMutex(NULL, FALSE, MUTEX_PROD);
    if (dados->hSemEscrita == NULL || dados->hSemLeitura == NULL || dados->hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        exit(-1);
    }

    *hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, CMD_FILE_MAP);
    if (*hFileMap == NULL) {
        primeiroProcesso = TRUE;
        *hFileMap = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(BUFFER_CIRCULAR),
            CMD_FILE_MAP);
        if (*hFileMap == NULL) {
            _tprintf(TEXT("Erro no CreateFileMapping\n"));
            return -1;
        }
    }
    dados->memPartilhada = (BUFFER_CIRCULAR*)MapViewOfFile(*hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (dados->memPartilhada == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        exit(-1);
    }
    if (primeiroProcesso == TRUE) {
        dados->memPartilhada->dwNumCons = 0;
        dados->memPartilhada->dwNumProd = 0;
        dados->memPartilhada->dwPosE = 0;
        dados->memPartilhada->dwPosL = 0;
    }
    dados->dwTerminar = 0;

    //temos de usar o mutex para aumentar o nProdutores para termos os ids corretos
    WaitForSingleObject(dados->hMutex, INFINITE);
    dados->memPartilhada->dwNumProd++;
    dados->dwId = dados->memPartilhada->dwNumProd;
    ReleaseMutex(dados->hMutex);

}

int _tmain(int argc, TCHAR* argv[]) {
    DWORD initX = 0, initY = 0;
    HANDLE hReadTh;
    HANDLE hServerTh;
    HANDLE hThreadProdutor;
    HANDLE hFileMap;
    DADOS_THREAD dados;

#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    //Verifica se o servidor esta a correr
    if (OpenMutex(SYNCHRONIZE, FALSE, MUTEX_SERVER) == NULL) {
        _tprintf(_T("O servidor ainda nao esta a correr\n"));
        return 1;
    }

    initDadosThread(&dados, &hFileMap);

    hReadTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadSharedMemory, NULL, 0, NULL);
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);
    hThreadProdutor = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadProdutor, (LPVOID)&dados, 0, NULL);

    WaitForSingleObject(hServerTh, INFINITE);

    UnmapViewOfFile(dados.memPartilhada);
    CloseHandle(hReadTh);
    CloseHandle(hServerTh);
    ExitProcess(0);
}


