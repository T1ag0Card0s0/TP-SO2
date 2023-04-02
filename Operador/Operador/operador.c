#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#define MAX_WIDTH 50
#define MAX_ROADS 4
#define NUM_OBJECTS 4
#define LIGEIRO1_TO_LEFT _T("     ___   \n\
 ___/_|_|_  \n\
|_________| \n\
  O      O  \n")
#define LIGEIRO1_TO_RIGHT _T("    ___\n\
  _|_|_\\___\n\
 |_________|\n\
  O      O\n")

#define LIGEIRO2_TO_LEFT _T("     _____ \n\
 ___/_|___| \n\
|_________| \n\
   O     O \n")
#define LIGEIRO2_TO_RIGHT _T("  _____\n\
 |___|_\\___\n\
 |_________|\n\
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
\\_2_/\n")
#define PASSEIO _T("_________________________________________________________________________________________________________\n")

/*
* CARROS
       ___   
   ___/_|_|_
  |_________|
    O      O
      ________
     /__|__|__|
    |_________|
       O     O
       _____
   ___/_|___|
  |_________|
     O     O    
*SAPO
   ___
  /O O\
  \_1_/
   ___
  /O O\
  \_2_/
 */
typedef enum ObjectWay {UP,DOWN,LEFT,RIGHT}ObjectWay;
typedef struct ObjectData {
    DWORD dwinitYCoord;//coordenada de onde comeca a estrada
    DWORD dwXCoord;//coordenada de onde se localiza o objeto na estrada
    DWORD dwRoadNum;// numero da faixa de rodagem onde se localiza 0...N
    TCHAR *object;//representacao do objeto na consola
    ObjectWay objWay;//sentido de movimento
    HANDLE *hMutex;//handle para as threads de cada carro nao escreverem
                   //caracteres pertencentes ao objeto no sitio errado
}ObjectData;

void GoToXY(int column, int line) {//coloca o cursor no local desejado
    COORD coord = {column,line};
    //Obter um handle para o ecra da consola
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // chaama a funcao de posicao do cursor call
    if (!SetConsoleCursorPosition(hConsole, coord))
    {
        // a funcao de posicao do cursor deu erro
    }
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
    COORD pos = { objData.dwXCoord, objData.dwinitYCoord+objData.dwRoadNum*4};
    DWORD written;
    for (int i = 0; i < lstrlen(objData.object); i++) {
        if (objData.object[i] == '\n') {
            pos.Y++;
            pos.X = objData.dwXCoord;
        }
        else {
            WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), &objData.object[i], 1, pos, &written);
            pos.X++;
        }
    }
}
DWORD WINAPI ObjectMove(LPVOID param) {//faz mover os carros sapos e assim
    ObjectData* objData = (ObjectData*)param;
    DWORD len = lstrlen(objData->object);
    while (TRUE) {
        switch (objData->objWay) {
            case UP: {
                if (objData->dwRoadNum - 1 < 0)ExitThread(1);
                objData->dwRoadNum--;
                break;
            }
            case DOWN: {
                if (objData->dwRoadNum + 1 > MAX_ROADS)ExitThread(1);
                objData->dwRoadNum++;
                break;
            }
            case LEFT: {
                if (objData->dwXCoord - 1 < 2) ExitThread(1);
                objData->dwXCoord--;
                break;
            }
            case RIGHT: {
                if (objData->dwXCoord + 1 > MAX_WIDTH) ExitThread(1);
                objData->dwXCoord++;
                break;
            }
        }
        WaitForSingleObject(*(objData->hMutex), INFINITE);
        Draw(*objData);
        Sleep(50);  
        ReleaseMutex(*(objData->hMutex));
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
 ObjectData setObjectData(DWORD dwinitYCoord, DWORD dwXCoord, DWORD dwRoadNum, const TCHAR* object, ObjectWay objWay) {
     ObjectData objData;
     objData.dwinitYCoord = dwinitYCoord;
     objData.dwXCoord = dwXCoord;
     objData.dwRoadNum = dwRoadNum;
     objData.objWay = objWay;
     objData.object = object;
     return objData;
 }
 ObjectData RandObject(DWORD RoadNumber,DWORD initY) {//funcao apenas para testes enquanto ainda nao aprendemos memoria partilhada
     TCHAR *leftObjects[] = {LIGEIRO1_TO_LEFT,LIGEIRO2_TO_LEFT,PESADO_TO_LEFT};
    TCHAR *rightObjects[] = {LIGEIRO1_TO_RIGHT,LIGEIRO2_TO_RIGHT,PESADO_TO_RIGHT};
     if(rand()%100<50)
        return setObjectData(initY,MAX_WIDTH,RoadNumber,leftObjects[rand()%3], LEFT);
     else
        return setObjectData(initY,0,RoadNumber,rightObjects[rand()%3], RIGHT);

 }
int _tmain(int argc, TCHAR* argv[]) {
    DWORD initX=0, initY=0;
    HANDLE hServerTh,hObjectMoveMutex;
    HANDLE hObjectsMove[NUM_OBJECTS];
    ObjectData objects[NUM_OBJECTS];
    TCHAR STR[5];
#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    if (OpenMutex(SYNCHRONIZE, FALSE, _T("Servidor")) == NULL) {
        _tprintf(_T("O servidor ainda nao esta a correr\n"));
        return 1;
    }
    srand(time(NULL));
    getCurrentCursorPosition(&initX, &initY);
    hObjectMoveMutex = CreateMutex(NULL, FALSE, NULL);
    for (int i = 0; i < NUM_OBJECTS; i++) {
        objects[i] = RandObject(i,initY);//apenas para teste
        objects[i].hMutex = &hObjectMoveMutex;
        hObjectsMove[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ObjectMove,(LPVOID)&objects[i], 0, NULL);
    }
    //Draw((ObjectData) {initY,20,4,LIGEIRO1_TO_LEFT});
    // algumas cenas a null para ja so para funcionar
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    while (1) {
        GoToXY(0, initY + NUM_OBJECTS * 4);
        _tprintf(_T("OPERADOR\n\nEscreve quit para sair\n"));
        _tscanf_s(_T("%s"), STR, 5);
        if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }
    for (int i = 0; i < NUM_OBJECTS; i++) {
        CloseHandle(hObjectsMove[i]);
    }
    CloseHandle(hObjectMoveMutex);
    ExitProcess(0);
}
