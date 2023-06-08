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
DWORD WINAPI ReceivePipeThread(LPVOID param) {
    GAME* game = (GAME*)param;
    HANDLE hEvents[MAX_PLAYERS];
    DWORD ret,i,n;
    TCHAR c;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        hEvents[i] = game->pipeData.playerData[i].overlapRead.hEvent;
    }
    while (!game->dwShutDown) {
        ret = WaitForMultipleObjects(MAX_PLAYERS, hEvents, FALSE, INFINITE);
        i = ret - WAIT_OBJECT_0;
        if (i >= 0 && i < MAX_PLAYERS) {
            if (GetOverlappedResult(game->pipeData.playerData[i].hPipe, &game->pipeData.playerData[i].overlapRead, &n, FALSE)) {
                if (n > 0) {
                    _tprintf(TEXT("[SERVIDOR] Recebi %d bytes: '%c' cliente %d... (ReadFile)\n"), n, c, i);
                    WAY way;
                    if (c == _T('U')) {
                        way = UP;
                    }
                    else if (c == _T('R')) {
                        way = RIGHT;
                    }
                    else if (c == _T('D')) {
                        way = DOWN;
                    }
                    else if (c == _T('L')) {
                        way = LEFT;
                    }
                    else if (c == _T('P')) {
                        game->bPaused = TRUE;
                        way = STOP;
                    }
                    else {
                        way = STOP;
                    }
                    moveObject(&game->pipeData.playerData[i].obj, way);
                }
                ZeroMemory(&n, sizeof(n));
                ResetEvent(game->pipeData.playerData[i].overlapRead.hEvent);
                ReadFile(game->pipeData.playerData[i].hPipe, &c, sizeof(c), &n, &game->pipeData.playerData[i].overlapRead);
            }
        }
    }
    ExitThread(0);
}
DWORD WINAPI WritePipeThread(LPVOID param) {
    GAME* game = (GAME*)param;
    DWORD n;
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("SendPipeEvent"));
    if (hEvent == NULL) {
        _tprintf(_T("[ERRO] Abrir evento para envio pelo pipe\n"));
        ExitThread(-1);
    }
    while (!game->dwShutDown) {
        WaitForSingleObject(hEvent, INFINITE);
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->pipeData.playerData[i].active) {
                WriteFile(game->pipeData.playerData[i].hPipe,
                    &game->sharedData.memPar->sharedBoard,
                    sizeof(game->sharedData.memPar->sharedBoard),
                    &n,
                    &game->pipeData.playerData[i].overlapWrite);

            }
        }
        ResetEvent(hEvent);
    }
    ExitThread(0);
}
DWORD WINAPI PipeManagerThread(LPVOID param) {
    GAME* game = (GAME*)param;
    DWORD nBytes;
    game->pipeData.dwNumClients = 0;
    while (!game->dwShutDown) {
        DWORD offset = WaitForMultipleObjects(MAX_PLAYERS, game->pipeData.hEvents, FALSE, INFINITE);
        DWORD i = offset - WAIT_OBJECT_0;
        if (i >= 0 && i < MAX_PLAYERS&&game->pipeData.playerData[i].active == FALSE) {
            if (GetOverlappedResult(game->pipeData.playerData[i].hPipe, &game->pipeData.playerData[i].overlapRead, &nBytes, FALSE)) {
                ResetEvent(game->pipeData.hEvents[i]);
                WaitForSingleObject(game->pipeData.hMutex,INFINITE);
                // inicializar playerData
                game->pipeData.playerData[i].obj.dwX = rand() % MAX_WIDTH;
                game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;
                game->pipeData.playerData[i].obj.dwLastX = game->pipeData.playerData[i].obj.dwX;
                game->pipeData.playerData[i].obj.dwLastY = game->pipeData.playerData[i].obj.dwY;
                game->pipeData.playerData[i].obj.c = FROG;
                game->pipeData.playerData[i].active = TRUE;
                ReleaseMutex(game->pipeData.hMutex);
                game->pipeData.dwNumClients++;
                _tprintf(_T("[SERVIDOR] Chegou um novo jogador\n"));
            }
        }
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!DisconnectNamedPipe(game->pipeData.playerData[i].hPipe)) {
            _tprintf(TEXT("[ERRO] Desligar o pipe! (DisconnectNamedPipe)"));
            ExitThread(-1);
        }
    }
    ExitThread(0);
}
DWORD WINAPI ThreadConsumidor(LPVOID param) {
    //dll chamar consumidor
    GAME* game = (GAME*)param;
    PFUNC_CONS pfunc;
    if ((pfunc = (PFUNC_CONS)GetProcAddress(game->sharedData.hdll, "consumidor")) == NULL) ExitThread(-1);
    pfunc(&game->sharedData,game->roads,game->dwInitNumOfRoads);
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

        if (!_tcscmp(cmd, _T("exit"))) { game->dwShutDown = 1; game->sharedData.terminar = 1; break; }

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
    GAME* game = (GAME*)param;
    DWORD n;
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT),hSendPipeEvent = OpenEvent(EVENT_ALL_ACCESS,FALSE,_T("SendPipeEvent"));
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(-1);
    }
    
    PFUNC_INIT_SHARED_BOARD pfunc;
    if ((pfunc = (PFUNC_INIT_SHARED_BOARD)GetProcAddress(game->sharedData.hdll, "initSharedBoard")) == NULL) ExitThread(-1);
    while (!game->dwShutDown) {
        WaitForSingleObject(hEvent, INFINITE);
        if (game->dwInitNumOfRoads != game->sharedData.memPar->sharedBoard.dwHeight - 4 ||
            game->dwInitSpeed * 100 != game->roads[0].dwSpeed) {
            pfunc(&game->sharedData.memPar->sharedBoard, game->dwInitNumOfRoads + 4);
            for (int i = 0; i < game->dwInitNumOfRoads; i++) {
                game->roads[i].dwSpeed = game->dwInitSpeed * 100;
            }
            continue;
        }

        for (int i = 0; i < game->dwInitNumOfRoads; i++) {
            for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                OBJECT obj = game->roads[i].cars[j];
                game->sharedData.memPar->sharedBoard.board[obj.dwLastY][obj.dwLastX] = _T(' ');
                game->sharedData.memPar->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;

            }
            if (game->roads[i].dwNumOfObjects != 0) {
                for (int j = 0; j < game->roads[i].dwNumOfObjects; j++) {
                    OBJECT obj = game->roads[i].objects[j];
                    game->sharedData.memPar->sharedBoard.board[obj.dwY][obj.dwX] = obj.c;
                }
            }
        }
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->pipeData.playerData[i].active) {
                game->sharedData.memPar->sharedBoard.board[game->pipeData.playerData[i].obj.dwLastY][game->pipeData.playerData[i].obj.dwLastX] = _T(' ');
                game->sharedData.memPar->sharedBoard.board[game->pipeData.playerData[i].obj.dwY][game->pipeData.playerData[i].obj.dwX] = game->pipeData.playerData[i].obj.c;
            }
        }
        SetEvent(hSendPipeEvent);
        ResetEvent(hEvent);
    }
    ExitThread(0);
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
                if (((road->cars[j].dwX == road->cars[i].dwX + 1) ||
                    (road->cars[j].dwX == 1 && road->cars[i].dwX == MAX_WIDTH - 1))
                    && road->way == RIGHT)
                    bNextToObstacle = TRUE;
                else if (((road->cars[j].dwX == road->cars[i].dwX - 1) ||
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
void initPipeData(PIPE_DATA* pipeData) {
    HANDLE hEventTemp;
    pipeData->hMutex = CreateMutex(NULL, FALSE, NULL);
    if (pipeData->hMutex == NULL) {
        _tprintf(_T("[ERRO] Criar Mutex para pipe\n"));
        return;
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        pipeData->playerData[i].hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
            MAX_PLAYERS,
            sizeof(TCHAR),
            sizeof(SHARED_BOARD),
            NMPWAIT_USE_DEFAULT_WAIT,
            NULL);
        if (pipeData->playerData[i].hPipe == INVALID_HANDLE_VALUE) {
            _tprintf(_T("[ERRO] Criar pipe para o jogador %d\n"), i);
            return;
        }
        hEventTemp = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (hEventTemp == NULL) {
            _tprintf(_T("[ERRO] Criar evento para o jogador %d\n"), i);
            return;
        }
        pipeData->playerData[i].active = FALSE;
        ZeroMemory(&pipeData->playerData[i].overlapRead, sizeof(pipeData->playerData[i].overlapRead));
        ZeroMemory(&pipeData->playerData[i].overlapWrite, sizeof(pipeData->playerData[i].overlapWrite));
        pipeData->playerData[i].overlapRead.hEvent = hEventTemp;
        pipeData->playerData[i].overlapWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        pipeData->hEvents[i] = hEventTemp;

        ConnectNamedPipe(pipeData->playerData[i].hPipe, &pipeData->playerData[i].overlapRead);
        ReadFile(pipeData->playerData[i].hPipe, NULL, 0, NULL, &pipeData->playerData[i].overlapRead);

    }
}

int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hExistServer, hUpdateEvent, hExitEvent,hSendPipeEvent;
    HANDLE hFileMap;
   
    HANDLE hThread[6];
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
    hSendPipeEvent = CreateEvent(NULL, TRUE, FALSE, _T("SendPipeEvent"));
    if (GetLastError() == ERROR_ALREADY_EXISTS || hExitEvent == NULL || hUpdateEvent == NULL||hSendPipeEvent == NULL) {
        _tprintf(_T("[ERRO] Erro a iniciar o programa\n"));
        ExitProcess(1);
    }
    if ((game.sharedData.hdll = LoadLibrary(_T("..\\..\\DynamicLinkLibrary\\x64\\Debug\\DynamicLinkLibrary.dll"))) == NULL) ExitProcess(-1);

    srand((unsigned int)time(NULL));
    game.dwShutDown = 0;
    game.dwLevel = 1;
    game.bPaused = FALSE;
    initRegestry(&game);
    hThread[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadConsumidor, (LPVOID)&game, 0, NULL);
    initRoads(game.roads, game.dwInitSpeed);
    initPipeData(&game.pipeData);
    //lançamos as estradas ao mesmo tempo
    hMutexRoad = CreateMutex(NULL, FALSE, MUTEX_ROAD);
    for (int i = 0; i < MAX_ROADS; i++) {
        game.roads[i].hMutex = hMutexRoad;
        ResumeThread(game.roads[i].hThread);
    }
    hThread[1] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CMDThread, (LPVOID)&game, 0, NULL);
    hThread[2] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)UpdateThread, (LPVOID)&game, 0, NULL);
    hThread[3] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PipeManagerThread, (LPVOID)&game, 0, NULL);
    hThread[4] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WritePipeThread, (LPVOID)&game, 0, NULL);
    hThread[5] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReceivePipeThread, (LPVOID)&game, 0, NULL);
    WaitForMultipleObjects(6, hThread, TRUE, INFINITE);

    SetEvent(hExitEvent);
    CloseHandle(game.hKey);
    CloseHandle(hExitEvent);
    CloseHandle(hMutexRoad);
    CloseHandle(hUpdateEvent);
    CloseHandle(hExistServer);

    return 0;
}
