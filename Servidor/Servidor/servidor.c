#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#define FILE_MAPPING _T("shared")
#define TAM 200
#define MAX_FAIXAS 8
#define KEY_VELOCIDADE TEXT("Velocidade")
#define KEY_FAIXAS TEXT("Faixas")
#define KEY_DIR TEXT("Software\\TPSO2\\")
#define MAX_WIDTH 60
#define NUM_ROADS 8
#define NUM_CARS_PER_ROAD 8
#define CAR _T(" C ")
#define PASSEIO _T(" ___________________________________________________________")
#define FROG _T(" F ")
#define SHARED_MEMORY_NAME _T("MySharedMemory")
#define EVENT_NAME _T("MyEvent")
#define CMD_SHARED_NAME _T("SharedCmd")
#define EVENT_NAME2 _T("MyEvent2")

typedef TCHAR Buffer[10000];
typedef enum CmdType
{
    STOP_MOVE,
    CHANGE_DIR,
    PUT_OBSTACLE

}CmdType;

typedef struct Cmd {
    HANDLE hEventC;
    CmdType type;
    int value;
}Cmd;
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

typedef struct {
    DWORD velocidade;
    DWORD n_faixas;
    HANDLE hkey;
}GAME_DATA;

DWORD WINAPI cmdThread(LPVOID lparam) {
    TCHAR linha[TAM];
    TCHAR cmd[TAM];
    DWORD value;
    GAME_DATA* data = (GAME_DATA*)lparam;

    do {
        _tprintf(_T("\n[SERV] $ "));
        _fgetts(linha, TAM, stdin);
        _stscanf_s(linha, _T("%s %u"), cmd, TAM, &value);

        if (!(_tcscmp(cmd, _T("setf")))) {
            if (value < 1 || value > MAX_FAIXAS) {
                _tprintf(_T("Insira um valor entre 1 e 8\n"));
            }
            else {
                data->n_faixas = value;
                if (RegSetValueEx(data->hkey, KEY_FAIXAS, 0, REG_DWORD, (LPCBYTE)&data->n_faixas, sizeof(value)) != ERROR_SUCCESS)
                    _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
                else
                    _tprintf(TEXT("Faixas = %u\n"), data->n_faixas);
            }
        }
        else if (!(_tcscmp(cmd, _T("setv")))) {
            data->velocidade = value;
            if (RegSetValueEx(data->hkey, KEY_VELOCIDADE, 0, REG_DWORD, (LPCBYTE)&data->velocidade, sizeof(value)) != ERROR_SUCCESS)
                _tprintf(TEXT("[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %u\n"), data->velocidade);
        }
        else if (!(_tcscmp(cmd, _T("help")))) {

            _tprintf(_T("\n-- setv x : definir a velocidade dos carros ( x inteiro maior que 1)"));
            _tprintf(_T("\n-- setf x : definir o numero de faixas ( x inteiro entre 1 e 8)"));
            _tprintf(_T("\n-- exit : sair\n\n"));
        }

        fflush(stdin);
        fflush(stdout);
    } while ((_tcscmp(cmd, _T("exit"))));

    RegCloseKey(data->hkey);
    ExitThread(0);
}


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
        WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), &objData.object, 3, pos, &written);
    else
        WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), _T("   "), 3, pos, &written);
}

void moveObject(ObjectData* objData) {
    
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
    road->hEventSh = CreateEvent(NULL, FALSE, FALSE, EVENT_NAME);
    int runningCars = 0, numSteps = 0;
    while (TRUE) {
        WaitForSingleObject(road->hMutexSh, INFINITE);
        if (numSteps % road->dwSpaceBetween == 0 && runningCars < road->dwNumObjects) { numSteps = 0; runningCars++; }
        
        for (int i = 0; i < runningCars; i++) {
            moveObject(&road->objects[i]);
        }

        numSteps++;
        Sleep(road->dwSpeed);
        ReleaseMutex(road->hMutexSh);
        // asinala evento para atualizar memoria
        SetEvent(road->hEventSh);
    }
    ExitThread(0);
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
    GAME_DATA data;
    DWORD estado;
    HANDLE fileMap;

    HANDLE hObjectsMove[NUM_ROADS * NUM_CARS_PER_ROAD];
    //ObjectData players[2];
    DWORD initX = 0, initY = 0;

#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    
    HANDLE hMutex = CreateMutex(NULL, TRUE, _T("Servidor"));
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        _tprintf(_T("[ERRO] Ja existe um servidor a correr\n"));
        ExitProcess(1);
    }

    _tprintf(_T("Bem vindo ao Servidor, digite 'help' se precisar de ajuda!\n"));

    if (RegOpenKeyExW(HKEY_CURRENT_USER, KEY_DIR, 0, KEY_ALL_ACCESS, &(data.hkey)) != ERROR_SUCCESS) {

        if (RegCreateKeyEx(HKEY_CURRENT_USER, KEY_DIR, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &(data.hkey), &estado) != ERROR_SUCCESS) {
            _tprintf(TEXT("\n[ERRO]Chave nao foi nem criada nem aberta! ERRO!"));
            ExitProcess(1);
        }
        else {
            _tprintf(_T("\n[AVISO] É necessário definir valores de velocidade e de número de faixas antes de começar!\n"));
            do {
                _tprintf(_T("Velocidade:"));
                _tscanf_s(_T("%u"), &data.velocidade);
            } while (data.velocidade <= 0);

            if (RegSetValueEx(data.hkey, KEY_VELOCIDADE, 0, REG_DWORD, (LPCBYTE)&data.velocidade, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %d\n"), data.velocidade);

            do {
                _tprintf(_T("\nNúmero de faixas (MAX 8):"));
                _tscanf_s(_T("%u"), &data.n_faixas);
            } while (data.n_faixas > MAX_FAIXAS || data.n_faixas <= 0);

            if (RegSetValueEx(data.hkey, KEY_FAIXAS, 0, REG_DWORD, (LPCBYTE)&data.n_faixas, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Faixas = %u\n"), data.n_faixas);
        }
    }
    else {

        _tprintf(_T("\n[AVISO] Valores de velocidade e faixas definidos anteriormente\n"));
        DWORD dwSize = sizeof(DWORD);
        if (RegQueryValueExW(data.hkey, KEY_VELOCIDADE, NULL, NULL, (LPBYTE) & (data.velocidade), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("Não foi possivel obter o valor da velocidade por favor insira um novo\n"));
        }
        else {
            _tprintf(_T("   -> Velocidade = %u\n"), data.velocidade);
        }
        if (RegQueryValueExW(data.hkey, KEY_FAIXAS, NULL, NULL, (LPBYTE) & (data.n_faixas), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("Não foi possivel obter o valor das faixas por favor insira um novo\n"));
        }
        else {
            _tprintf(_T("   -> Faixas = %u\n"), data.n_faixas);
        }
    }

    // Criar o evento para fechar os outros processos
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, "ExitServer");
    if (hEvent == NULL)
    {
        _tprintf(_T("[ERRO] Evento não criado\n\n"));
        ExitProcess(1);
    }

 
    //Sapos
   /* data.roadData->objects[0] = setObjectData(initY + 2 + NUM_ROADS, 2, FROG, STOP);
    data.roadData->objects[1] = setObjectData(initY + 2 + NUM_ROADS, 5, FROG, STOP);
    _tprintf(_T("OLA"));
    Draw(data.roadData->objects[0]);
    Draw(data.roadData->objects[1]);*/

    //carros
    RoadData* pData = NULL;
 
    // cria a memória partilhada
    HANDLE hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(RoadData), SHARED_MEMORY_NAME);

    // mapeia a memória partilhada
    pData = (RoadData*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(RoadData));

    pData->hMutexSh = CreateMutex(NULL, TRUE, _T("MutexMemory"));

 
    for (int i = 0; i <= data.n_faixas; i++) {
        pData[i].dwSpaceBetween = rand() % (MAX_WIDTH / NUM_CARS_PER_ROAD) + 10;
        pData[i].dwSpeed = data.velocidade;
        pData[i].dwNumObjects = rand() % NUM_CARS_PER_ROAD + 1;
        for (int j = 0; j < NUM_CARS_PER_ROAD; j++) {
            if (i % 2 == 0) {
                pData[i].objects[j] = setObjectData(initY + i + 1, 0, CAR, RIGHT);//apenas para teste
            }
            else {
                pData[i].objects[j] = setObjectData(initY + i + 1, MAX_WIDTH, CAR, LEFT);//apenas para teste
            }
        }
        hObjectsMove[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoadMove, (LPVOID)&pData[i], 0, NULL);
    }

    HANDLE hCmdTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)cmdThread,(LPVOID)&(data), NULL, NULL);

    WaitForSingleObject(hCmdTh, INFINITE);

    // saiu da thread e quer fechar
    SetEvent(hEvent);
    UnmapViewOfFile(hMapFile);
    CloseHandle(hMapFile);
    CloseHandle(hEvent);
    CloseHandle(hCmdTh);
    RegCloseKey(data.hkey);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    ExitProcess(0);
}

