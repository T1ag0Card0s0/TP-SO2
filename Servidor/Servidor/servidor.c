#include "servidor.h"

//Posicionamento de cursor
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

DWORD WINAPI ThreadConsumidor(LPVOID param) {
   //dll chamar consumidor
    GAME *game=(GAME *)param;
    PFUNC_CONS pfunc;
    if ((pfunc = (PFUNC_CONS)GetProcAddress(game->sharedData.hdll, "consumidor")) == NULL) ExitThread(- 1);
    pfunc(game);
    ExitThread(0);
}

DWORD WINAPI CMDThread(LPVOID param) {
    GAME* game = (GAME*)param;
    DWORD x, y, written, value;
    COORD pos;
    TCHAR command_line[TAM];
    TCHAR cmd[TAM];
    getCurrentCursorPosition(&x, &y);
    while (!game->dwShutDown) {
        fflush(stdout); fflush(stdin);
        GoToXY(0, 0);
        _tprintf(_T("Velocidade: %u Numero de estradas: %u \n\n"), game->dwInitSpeed, game->dwInitNumOfRoads);
        getCurrentCursorPosition(&x, &y);
        GoToXY(x, y);
        pos.X = x; pos.Y = y;
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        _tprintf(_T("[SERVER]$ "));
        _fgetts(command_line, TAM, stdin);
        _stscanf_s(command_line, _T("%s %u"), cmd, TAM, &value);

        if (!_tcscmp(cmd, _T("exit"))) { game->dwShutDown = 1; break; }

        if (!(_tcscmp(cmd, _T("setf")))) {
            if (value < 1 || value > MAX_ROADS) {
                _tprintf(_T("Insira um valor entre 1 e 8\n"));
            }
            else {
                game->dwInitNumOfRoads = value;
                if (RegSetValueEx(game->hKey, KEY_ROADS, 0, REG_DWORD, (LPCBYTE)&value, sizeof(value)) != ERROR_SUCCESS)
                    _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            }
        }
        else if (!(_tcscmp(cmd, _T("setv")))) {
            if (value < 0 || value>5) continue;
            game->dwInitSpeed = value;
            if (RegSetValueEx(game->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&value, sizeof(value)) != ERROR_SUCCESS)
                _tprintf(TEXT("[AVISO] O atributo nao foi alterado nem criado!\n"));
        }
        else if (!_tcscmp(cmd, _T("pause"))) {
            if (game->bPaused)continue;
            game->bPaused = TRUE;
            for (int i = 0; i < MAX_ROADS; i++) {
                game->roads[i].lastWay = game->roads[i].way;
                game->roads[i].way = STOP;
            }
        }
        else if (!_tcscmp(cmd, _T("unpause"))) {
            if (!game->bPaused)continue;
            game->bPaused = FALSE;
            for (int i = 0; i < MAX_ROADS; i++) {
                game->roads[i].way = game->roads[i].lastWay;
            }
        }
        else if (!_tcscmp(cmd, _T("restart"))) {
            //implementar o comando restart
        }
        else if (!(_tcscmp(cmd, _T("help")))) {
            _tprintf(_T("\n-- setv x : definir a velocidade dos carros ( x inteiro maior que 1)"));
            _tprintf(_T("\n-- setf x : definir o numero de faixas ( x inteiro entre 1 e 8)"));
            _tprintf(_T("\n-- pause : pausar o jogo"));
            _tprintf(_T("\n-- unpause : resumir o jogo"));
            _tprintf(_T("\n-- restart : recomeçar o jogo"));
            _tprintf(_T("\n-- exit : sair\n\n"));
        }
    }

    ExitThread(0);
}

DWORD WINAPI UpdateThread(LPVOID param) {
    GAME *game= (GAME *)param;
    PFUNC_UPDATE pfunc;
    if ((pfunc = (PFUNC_UPDATE)GetProcAddress(game->sharedData.hdll, "update_board")) == NULL) ExitThread(-1);
    ExitThread(pfunc(game));
}

DWORD WINAPI RoadMove(LPVOID param) {
    ROAD* road = (ROAD*)param;
    BOOL bNextToObstacle = FALSE;
    HANDLE hUpdateEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hUpdateEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(-1);
    }
    int runningCars = 0, numSteps = 0;
    while (TRUE) {
        if (road->dwTimeStoped > 0) {
            Sleep(road->dwTimeStoped * 1000);
            road->dwTimeStoped = 0;
            road->way = road->lastWay;
        }
        WaitForSingleObject(road->hMutex, INFINITE);
        if (numSteps % road->dwSpaceBetween == 0 && runningCars < road->dwNumOfCars) {
            numSteps = 0;
            runningCars++;
        }
        for (int i = 0; i < runningCars; i++) {

            for (int j = 0; j < road->dwNumOfObjects; j++) {
                if (road->objects[j].dwY != road->cars[i].dwY)continue;
                if ((road->objects[j].dwX - 1 == road->cars[i].dwX)
                    && road->way == RIGHT)
                    bNextToObstacle = TRUE;
                else if ((road->objects[j].dwX + 1 == road->cars[i].dwX)
                    && road->way == LEFT)
                    bNextToObstacle = TRUE;
            }

            for (int j = 0; j < runningCars; j++) {
                if (road->cars[j].dwY != road->cars[i].dwY)continue;
                if (((road->cars[j].dwX == road->cars[i].dwX+1) ||
                    (road->cars[j].dwX == 1 && road->cars[i].dwX == MAX_WIDTH-1))
                    && road->way == RIGHT)
                    bNextToObstacle = TRUE;
                else if (((road->cars[j].dwX == road->cars[i].dwX-1) ||
                    (road->cars[j].dwX == MAX_WIDTH - 1 && road->cars[i].dwX == 1))
                    && road->way == LEFT)
                    bNextToObstacle = TRUE;
            }

            if (!bNextToObstacle)moveObject(&road->cars[i], road->way);
            else { 
                road->cars[i].dwLastX = road->cars[i].dwX;
                road->cars[i].dwLastY = road->cars[i].dwY;
                bNextToObstacle = FALSE; 
            }
        }

        numSteps++;
        if (road->way != STOP) {
            SetEvent(hUpdateEvent);
        }
        ReleaseMutex(road->hMutex);
        Sleep(road->dwSpeed);

    }
    ExitThread(0);
}

void moveObject(OBJECT* objData, WAY way) {
    if (way != STOP) {
        objData->dwLastX = objData->dwX;
        objData->dwLastY = objData->dwY;
    }

    switch (way) {
    case UP: {
        if (objData->dwY - 1 <= 0)break;
        objData->dwY--;
        break;
    }
    case DOWN: {
        if (objData->dwY + 1 >= MAX_ROADS)break;
        objData->dwY++;
        break;
    }
    case LEFT: {
        if (objData->dwX - 1 <= 0) objData->dwX = MAX_WIDTH;
        objData->dwX--;
        break;
    }
    case RIGHT: {
        if (objData->dwX + 1 >= MAX_WIDTH) objData->dwX = 0;
        objData->dwX++;
        break;
    }
    }
}

void initRegestry(GAME* data) {
    DWORD estado;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, KEY_DIR, 0, KEY_ALL_ACCESS, &(data->hKey)) != ERROR_SUCCESS) {

        if (RegCreateKeyEx(HKEY_CURRENT_USER, KEY_DIR, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &(data->hKey), &estado) != ERROR_SUCCESS) {
            _tprintf(TEXT("\n[ERRO]Chave nao foi nem criada nem aberta! ERRO!"));
            ExitProcess(1);
        }
        else {
            _tprintf(_T("\n[AVISO] É necessário definir valores de velocidade e de número de faixas antes de começar!\n"));
            do {
                _tprintf(_T("Velocidade:"));
                _tscanf_s(_T("%u"), &data->dwInitSpeed);
            } while (data->dwInitSpeed <= 0);

            if (RegSetValueEx(data->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&data->dwInitSpeed, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %d\n"), data->dwInitSpeed);

            do {
                _tprintf(_T("\nNúmero de faixas (MAX 8):"));
                _tscanf_s(_T("%u"), &data->dwInitNumOfRoads);
            } while (data->dwInitNumOfRoads > MAX_ROADS || data->dwInitNumOfRoads <= 0);

            if (RegSetValueEx(data->hKey, KEY_ROADS, 0, REG_DWORD, (LPCBYTE)&data->dwInitNumOfRoads, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Faixas = %u\n"), data->dwInitNumOfRoads);
        }
    }
    else {
        DWORD dwSize = sizeof(DWORD);
        if (RegQueryValueExW(data->hKey, KEY_SPEED, NULL, NULL, (LPBYTE) & (data->dwInitSpeed), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("Não foi possivel obter o valor da velocidade por favor insira um novo\n"));
        }
        if (RegQueryValueExW(data->hKey, KEY_ROADS, NULL, NULL, (LPBYTE) & (data->dwInitNumOfRoads), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("Não foi possivel obter o valor das faixas por favor insira um novo\n"));
        }
    }
    //limpa consola
    COORD pos = { 0,0 };
    DWORD written;
    for (int i = 0; i < MAX_ROADS; i++) {
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 200, pos, &written);
        pos.Y++;
    }
}
void initRoads(ROAD* roads, DWORD dwInitSpeed) {
    for (int i = 0; i < MAX_ROADS; i++) {
        roads[i].dwNumOfObjects = 0;
        roads[i].dwNumOfCars = rand() % MAX_CARS_PER_ROAD + 1;
        roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / MAX_CARS_PER_ROAD) + 2;
        roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
        roads[i].dwSpeed = dwInitSpeed * 100;
        roads[i].dwTimeStoped = 0;
        roads[i].hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoadMove, (LPVOID)&roads[i], CREATE_SUSPENDED, NULL);
        for (int j = 0; j < roads[i].dwNumOfCars; j++) {
            roads[i].cars[j].c = CAR;
            roads[i].cars[j].dwX = (roads[i].way == RIGHT ? 0 : MAX_WIDTH);
            roads[i].cars[j].dwY = i + 2;
            roads[i].cars[j].dwLastX = roads[i].cars[j].dwX;
            roads[i].cars[j].dwLastY = roads[i].cars[j].dwY;
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

int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hExistServer, hUpdateEvent, hExitEvent;
    HANDLE hFileMap;
    HANDLE hUpdateThread, hCMDThread, hThreadConsumidor;
    HANDLE hMutexRoad;
    GAME game;
    HANDLE hdll;
#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif
    //Controla quantos servidores podem correr
    hExistServer = CreateMutex(NULL, TRUE, MUTEX_SERVER);
    hExitEvent = CreateEvent(NULL, TRUE, FALSE, EXIT_EVENT);
    hUpdateEvent = CreateEvent(NULL, TRUE, FALSE, UPDATE_EVENT);
    if (GetLastError() == ERROR_ALREADY_EXISTS|| hExitEvent == NULL|| hUpdateEvent == NULL) {
        _tprintf(_T("[ERRO] Erro a iniciar o programa\n"));
        ExitProcess(1);
    }
    if ((game.sharedData.hdll = LoadLibrary(_T("DynamicLinkLibrary.dll"))) == NULL) ExitProcess(-1);

    srand((unsigned int)time(NULL));
   
    initRegestry(&game);
    hThreadConsumidor = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadConsumidor, (LPVOID)&game, 0, NULL);
    initRoads(game.roads, game.dwInitSpeed);
    initPlayers(game.players, game.dwInitNumOfRoads);
    //lançamos as estradas ao mesmo tempo
    hMutexRoad = CreateMutex(NULL, FALSE, MUTEX_ROAD);
    for (int i = 0; i < MAX_ROADS; i++) {
        game.roads[i].hMutex = hMutexRoad;
        ResumeThread(game.roads[i].hThread);
    }
    hCMDThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, (LPVOID)&game, 0, NULL);
    hUpdateThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, (LPVOID)&game, 0, NULL);

    WaitForSingleObject(hUpdateThread, INFINITE);
    WaitForSingleObject(hCMDThread, INFINITE);

    SetEvent(hExitEvent);
    CloseHandle(game.hKey);
    CloseHandle(hExitEvent);
    CloseHandle(hMutexRoad);
    CloseHandle(hUpdateEvent);
    CloseHandle(hExistServer);

    return 0;
}
