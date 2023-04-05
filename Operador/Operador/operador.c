#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#define MAX_WIDTH 60
#define NUM_ROADS 5
#define NUM_CARS_PER_ROAD 4
#define CAR _T(" C ")
#define FROG _T(" F ")
/*#define LIGEIRO1_TO_LEFT _T("    ___ \n\
 __/_|_|_  \n\
|________| \n\
 O      O  \n")
#define LIGEIRO1_TO_RIGHT _T("    ___ \n\
  _|_|_\\__\n\
 |________|\n\
  O      O\n")

#define LIGEIRO2_TO_LEFT _T("     ____  \n\
 ___/_|__| \n\
|________| \n\
 O      O \n")
#define LIGEIRO2_TO_RIGHT _T("  ____ \n\
 |__|_\\___\n\
 |________|\n\
  O      O\n")
#define PESADO_TO_LEFT _T("  ________ \n\
 /__|__|__| \n\
|_________| \n\
  O      O \n")
#define PESADO_TO_RIGHT _T("  ________\n\
 |__|__|__\\\n\
 |_________|\n\
  O      O\n")
#define SAPO1 _T(" ___\n\
/O O\\\n\
\\_1_/\n")
#define SAPO2 _T(" ___\n\
/O O\\\n\
\\_2_/\n")*/
#define PASSEIO _T(" ___________________________________________________________")

typedef enum ObjectWay { UP, DOWN, LEFT, RIGHT, STOP }ObjectWay;
typedef struct ObjectData {
    DWORD dwXCoord,dwYCoord;//coordenada de onde se localiza o objeto na estrada
    TCHAR object[3];//representacao do objeto na consola
    ObjectWay objWay;//sentido de movimento
}ObjectData;
typedef struct RoadData {
    DWORD dwSpeed;
    DWORD dwSpaceBetween;
    DWORD dwNumObjects;
    ObjectData objects[NUM_CARS_PER_ROAD];
}RoadData;

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
    if(pos.X<MAX_WIDTH&&pos.X>1)
       WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), &objData.object, 3, pos, &written);
    else
        WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), _T("   "),3, pos, &written);
}
void moveObject(ObjectData *objData) {
    switch (objData->objWay) {
        case UP: {
            if (objData->dwYCoord - 1 < 0)break;
            objData->dwYCoord--;
            break;
        }
        case DOWN: {
            if (objData->dwYCoord + 1 > NUM_ROADS)break;
            objData->dwYCoord++;
            break;
        }
        case LEFT: {
            if (objData->dwXCoord - 1 <= 0) objData->dwXCoord = MAX_WIDTH;
            objData->dwXCoord--;
            break;
        }
        case RIGHT: {
            if (objData->dwXCoord + 1 > MAX_WIDTH) objData->dwXCoord = 0;
            objData->dwXCoord++;
            break;
        }
    }
}
DWORD WINAPI RoadMove(LPVOID param) {
    RoadData* road = (RoadData*)param;
    int runningCars=0,numSteps=0;
    while (TRUE) {
        if (numSteps % road->dwSpaceBetween == 0&&runningCars<road->dwNumObjects) { numSteps = 0; runningCars++; }
        for (int i = 0; i < runningCars; i++) {
            moveObject(&road->objects[i]);
            Draw(road->objects[i]);
        }
        Sleep(road->dwSpeed);
        numSteps++;
    }
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
ObjectData setObjectData(DWORD dwYCoord, DWORD dwXCoord, const TCHAR object[3], ObjectWay objWay) {
    ObjectData objData;
    objData.dwYCoord = dwYCoord;
    objData.dwXCoord = dwXCoord;
    objData.objWay = objWay;
    for (int i = 0; i < 3; i++) {
        objData.object[i] = object[i];
    }
    return objData;
}
int _tmain(int argc, TCHAR* argv[]) {
    DWORD initX = 0, initY = 0;
    HANDLE hServerTh;
    HANDLE hObjectsMove[NUM_ROADS*NUM_CARS_PER_ROAD];
    RoadData roads[NUM_ROADS];
    ObjectData players[2];
    TCHAR STR[5];
#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    //Verifica se o servidor esta a correr
    /*if (OpenMutex(SYNCHRONIZE, FALSE, _T("Servidor")) == NULL) {
        _tprintf(_T("O servidor ainda nao esta a correr\n"));
        return 1;
    }*/
    //Comeca a mostrar o jogo
    srand(time(NULL));
    _tprintf(_T("\n%s"), PASSEIO);
    getCurrentCursorPosition(&initX, &initY);
    GoToXY(0, initY +1+ NUM_ROADS );
    _tprintf(_T("%s"), PASSEIO);

    //Sapos
    players[0] = setObjectData(initY + 2 + NUM_ROADS, 2, FROG, STOP);
    players[1] = setObjectData(initY + 2 + NUM_ROADS, 5, FROG, STOP);
    Draw(players[0]);
    Draw(players[1]);

    //carros
    for (int i = 0; i < NUM_ROADS; i++) {
        roads[i].dwSpaceBetween =rand()%(MAX_WIDTH / NUM_CARS_PER_ROAD)+10;
        roads[i].dwSpeed = rand()%1000;
        roads[i].dwNumObjects = rand()%NUM_CARS_PER_ROAD+1;
        for (int j = 0; j < NUM_CARS_PER_ROAD; j++) {
            if (i % 2 == 0)
                roads[i].objects[j] = setObjectData(initY+i+1,0,CAR,RIGHT) ;//apenas para teste
            else
                roads[i].objects[j] = setObjectData(initY + i + 1, MAX_WIDTH, CAR, LEFT);  
        }
        hObjectsMove[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoadMove, (LPVOID)&roads[i], 0, NULL);
    }
    // algumas cenas a null para ja so para funcionar
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);
    GoToXY(0, initY + NUM_ROADS+3);
    //Pede input do utilizador
    while (1) {
        _tprintf(_T("OPERADOR\n\nEscreve quit para sair\n"));
        _tscanf_s(_T("%s"), STR, 5);
        if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }
    for (int i = 0; i < NUM_ROADS; i++) {
        CloseHandle(hObjectsMove[i]);
    }
    ExitProcess(0);
}