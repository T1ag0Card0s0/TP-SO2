#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#define TAM 200
#define FILE_MAPPING _T("shared")
#define MAX_WIDTH 60
#define NUM_ROADS 8
#define NUM_CARS_PER_ROAD 8
#define CAR _T(" C ")
#define FROG _T(" F ")
#define EVENT_NAME _T("MyEvent")
#define EVENT_NAME2 _T("MyEvent2")
#define FILE_MAPPING_NAME _T("MySharedMemory")
#define CMD_SHARED_NAME _T("SharedCmd")
#define PASSEIO _T(" ___________________________________________________________")

typedef TCHAR Buffer[];

typedef enum ObjectWay { UP, DOWN, LEFT, RIGHT, STOP }ObjectWay;

typedef struct ObjectData {
    DWORD dwXCoord, dwYCoord;//coordenada de onde se localiza o objeto na estrada
    TCHAR object[3];//representacao do objeto na consola
    ObjectWay objWay;//sentido de movimento
}ObjectData;

typedef struct RoadData {
    HANDLE hEventSh;
    HANDLE hMutexSh;
    DWORD dwSpeed;
    DWORD dwSpaceBetween;
    DWORD dwNumObjects;
    ObjectData objects[NUM_CARS_PER_ROAD];
}RoadData;

typedef enum CmdType
{
    STOP_MOVE,
    CHANGE_DIR,
    PUT_OBSTACLE

}CmdType;

typedef struct Cmd {
    HANDLE hMutexCmd;
    CmdType type;
    int value;
}Cmd;

HANDLE hEventSh = NULL;
HANDLE hMutexSh = NULL;

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

void Draw(ObjectData objData) {// mostra no ecra o objeto
    fflush(stdout);
    COORD pos = { objData.dwXCoord, objData.dwYCoord };
    DWORD written;
    if (pos.X < MAX_WIDTH && pos.X>1)
        WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), CAR, 3, pos, &written);
    else
        WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), _T("   "), 3, pos, &written);
}

DWORD WINAPI ReadSharedMemory(LPVOID lpParam) {

    HANDLE hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, FILE_MAPPING_NAME);
    if (hMapFile == NULL) {
        _tprintf(_T("Could not open file mapping object (%d)\n"), GetLastError());
        return 1;
    }

    RoadData* pData = (RoadData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(RoadData));

    if (pData == NULL) {
        _tprintf(_T("Could not map view of file (%d)\n"), GetLastError());
        CloseHandle(hMapFile);
        return 1;
    }

    pData->hEventSh = OpenEvent(EVENT_ALL_ACCESS, FALSE, EVENT_NAME);
    if (pData->hEventSh == NULL) {
        _tprintf(_T("OpenEvent failed (%d)\n"), GetLastError());
        return 1;
    }

    while (TRUE) {
        WaitForSingleObject(pData->hEventSh, INFINITE);

        for (int i = 0; i <= sizeof(pData); i++) {
            for (int j = 0; j < pData[i].dwNumObjects; j++) {
                WaitForSingleObject(pData->hMutexSh, INFINITE);
                Draw(pData[i].objects[j]);
                ReleaseMutex(pData->hMutexSh, INFINITE);
            }
        }
    }

    UnmapViewOfFile(hMapFile);
    ExitThread(0);
}

DWORD WINAPI CheckIfServerExit(LPVOID lpParam) {

    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "ExitServer");
    if (hEvent == NULL)
    {
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

#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    //Verifica se o servidor esta a correr
    if (OpenMutex(SYNCHRONIZE, FALSE, _T("Servidor")) == NULL) {
        _tprintf(_T("O servidor ainda nao esta a correr\n"));
        return 1;
    }

    //Comeca a mostrar o jogo
    srand(time(NULL));
    _tprintf(_T("\n%s"), PASSEIO);
    getCurrentCursorPosition(&initX, &initY);
    GoToXY(0, 20);
    _tprintf(_T("%s"), PASSEIO);

    HANDLE hReadTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadSharedMemory, NULL, 0, NULL);
    HANDLE hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    WaitForSingleObject(hServerTh, INFINITE);

    GoToXY(0, initY + NUM_ROADS + 3);
    CloseHandle(hReadTh);
    CloseHandle(hServerTh);
    ExitProcess(0);
}