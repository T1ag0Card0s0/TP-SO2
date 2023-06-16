#pragma once
#include <tchar.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <windowsx.h>
#include <time.h>
#define TAM 256
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20
#define MAX_CARS_PER_ROAD 8
#define MAX_PLAYERS 2
#define PIPE_NAME _T("\\\\.\\pipe\\TP-SO2")
#define NUM_BMP_FILES 8

typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;
    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;
typedef enum GAME_TYPE {
    SINGLE_PLAYER, MULTI_PLAYER, NONE
}GAME_TYPE;
typedef struct PIPE_GAME_DATA {
    SHARED_BOARD sharedBoard;
    DWORD dwLevel;
    DWORD dwPlayer1Points, dwPlayer2Points;
    DWORD dwX, dwY;
    DWORD dwNEndLevel;//numero de vezes que chegou ao fim
    GAME_TYPE gameType;
    BOOL bWaiting;
}PIPE_GAME_DATA;
typedef struct BUTTON {
    TCHAR label[50];
    DWORD dwX, dwY;
    DWORD dwWidth, dwHeight;
}BUTTON;
typedef struct PAINT_DATA {
    HDC bmpDC[NUM_BMP_FILES];
    HDC* memDC;
    BITMAP bmp[NUM_BMP_FILES];
    HWND hWnd;
    HANDLE hMutex;
    BUTTON buttons[2];
    DWORD *XOffset, *YOffset;
}PAINT_DATA;
typedef struct PIPE_DATA {
    HANDLE hPipe;
    OVERLAPPED overlapRead, overlapWrite;
    PIPE_GAME_DATA pipeGameData;
    PAINT_DATA paintData;
    DWORD dwShutDown;
    BOOL bNewBoard;
    TCHAR sentido;
}PIPE_DATA;
#define MUTEX_SERVER _T("Server")

LRESULT CALLBACK TrataEventos(HWND, UINT, WPARAM, LPARAM);
DWORD WINAPI CheckIfServerExit(LPVOID lpParam);
DWORD WINAPI ReadPipeThread(LPVOID param);
int writee(PIPE_DATA* pipeData, TCHAR c);
int CheckNumberOfInstances(HANDLE hSemaphore);
int initPipeData(PIPE_DATA* pipeData);
void drawText(PIPE_DATA* pipeData, RECT rect);
void drawBoard(PIPE_DATA* pipeData, RECT rect);
