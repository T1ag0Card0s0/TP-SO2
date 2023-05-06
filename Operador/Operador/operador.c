#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#define TAM 200

#define MAX_ROADS 8
#define MAX_WIDTH 20

#define MUTEX_SERVER _T("Server")
#define BOARD_SHARED_MEMORY _T("ShareBoard")
#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")

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
    while (TRUE) {
        GoToXY(0, 0);

        WaitForSingleObject(hEvent, INFINITE);
        _tprintf(_T("PASSEI AQUI %d\n"), i++);
        for (int i = 0; i < sharedBoard->dwHeight; i++) {
            for (int j = 0; j < sharedBoard->dwWidth; j++) {
                _tprintf(_T("%c"), sharedBoard->board[i][j]);
            }
            _tprintf(_T("\n"));
        }
        ResetEvent(hEvent);

    }
    UnmapViewOfFile(sharedBoard);
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

int _tmain(int argc, TCHAR* argv[]) {
    DWORD initX = 0, initY = 0;
    HANDLE hReadTh;
    HANDLE hServerTh;


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

    hReadTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadSharedMemory, NULL, 0, NULL);
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    WaitForSingleObject(hServerTh, INFINITE);

    CloseHandle(hReadTh);
    CloseHandle(hServerTh);
    ExitProcess(0);
}


