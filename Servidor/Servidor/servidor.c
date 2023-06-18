#include "servidor.h"

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
DWORD WINAPI AFKCounter(LPVOID param) {
    GAME* game = (GAME*)param;
    while (!game->dwShutDown) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game->pipeData.playerData[i].active) {
                if (game->pipeData.playerData[i].dwAFKseg >= 10) {
                    game->pipeData.playerData[i].dwAFKseg = 0;
                    game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;
                    game->pipeData.playerData[i].dwPoints -= 5;
                }
                if(game->pipeData.playerData[i].obj.dwY!=game->dwInitNumOfRoads+3)
                    game->pipeData.playerData[i].dwAFKseg++;
            }
        }
        Sleep(1000);
    }
    ExitThread(0);
}
DWORD WINAPI ReceivePipeThread(LPVOID param) {
    GAME* game = (GAME*)param;
    HANDLE hEvents[MAX_PLAYERS];
    DWORD ret,i,n,count = 0;
    TCHAR c;
    BOOL sideWalk = FALSE;
    HANDLE hSendEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("SendPipeEvent"));
    if (hSendEvent == NULL) {
        _tprintf(_T("[ERRO] Abrir evento para envio pelo pipe\n"));
        ExitThread(-1);
    }
    for (int i = 0; i < MAX_PLAYERS; i++) {
        hEvents[i] = game->pipeData.playerData[i].overlapRead.hEvent;
    }
    while (!game->dwShutDown) {
        ret = WaitForMultipleObjects(MAX_PLAYERS, hEvents, FALSE, INFINITE);
        i = ret - WAIT_OBJECT_0;
        if (i >= 0 && i < MAX_PLAYERS) {
            if (GetOverlappedResult(game->pipeData.playerData[i].hPipe, &game->pipeData.playerData[i].overlapRead, &n, FALSE)) {
                if (n > 0) {
                    if (!game->pipeData.playerData[i].active) {
                        game->pipeData.dwNumClients++;
                        playerArrived(game, i);
                    }
                    if (!runClientRequest(game, c, i)) {
                        disconectClient(game, i);
                        hEvents[i] = game->pipeData.playerData[i].overlapRead.hEvent;
                        continue;
                    }
                    else {
                        SetEvent(hSendEvent);
                    }
                }
                ZeroMemory(&c, sizeof(c));
                ResetEvent(game->pipeData.playerData[i].overlapRead.hEvent);
                ReadFile(game->pipeData.playerData[i].hPipe, &c, sizeof(c), &n, &game->pipeData.playerData[i].overlapRead);
            }
        }
    }
    ExitThread(0);
}
DWORD WINAPI WritePipeThread(LPVOID param) {
    GAME* game = (GAME*)param;
    PIPE_GAME_DATA pipeGameData;
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
                pipeGameData.dwLevel = game->dwLevel;
                if (i == 0) {
                    pipeGameData.dwPlayer1Points = game->pipeData.playerData[0].dwPoints; 
                    pipeGameData.dwPlayer2Points = game->pipeData.playerData[1].dwPoints;
                }
                else {
                    pipeGameData.dwPlayer1Points = game->pipeData.playerData[1].dwPoints;
                    pipeGameData.dwPlayer2Points = game->pipeData.playerData[0].dwPoints;
                }
                pipeGameData.dwX = game->pipeData.playerData[i].obj.dwX; pipeGameData.dwY = game->pipeData.playerData[i].obj.dwY;
                pipeGameData.dwNEndLevel = game->pipeData.playerData[i].dwNEndLevel;
                pipeGameData.sharedBoard = game->sharedData.memPar->sharedBoard;
                pipeGameData.bWaiting = game->pipeData.playerData[i].bWaiting;
                pipeGameData.gameType = game->pipeData.playerData[i].gameType;
                if (pipeGameData.gameType == SINGLE_PLAYER) {
                    if (i == 0 && game->pipeData.playerData[1].active) {
                        pipeGameData.sharedBoard.board[game->pipeData.playerData[1].obj.dwY][game->pipeData.playerData[1].obj.dwX] = _T(' ');
                        pipeGameData.sharedBoard.board[game->pipeData.playerData[0].obj.dwY][game->pipeData.playerData[0].obj.dwX] = game->pipeData.playerData[0].obj.c;
                    }
                    else if (i == 1 && game->pipeData.playerData[0].active) {
                        pipeGameData.sharedBoard.board[game->pipeData.playerData[0].obj.dwY][game->pipeData.playerData[0].obj.dwX] = _T(' ');
                        pipeGameData.sharedBoard.board[game->pipeData.playerData[1].obj.dwY][game->pipeData.playerData[1].obj.dwX] = game->pipeData.playerData[1].obj.c;
                    }
                }
                WriteFile(game->pipeData.playerData[i].hPipe,
                    &pipeGameData,
                    sizeof(pipeGameData),
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
        if (i >= 0 && i < MAX_PLAYERS) {
            if (GetOverlappedResult(game->pipeData.playerData[i].hPipe, &game->pipeData.playerData[i].overlapRead, &nBytes, FALSE)) {
                ResetEvent(game->pipeData.hEvents[i]);
                if (nBytes == 0) {
                    WaitForSingleObject(game->pipeData.hMutex, INFINITE);
                    // inicializar playerData
                    game->pipeData.dwNumClients++;
                    playerArrived(game, i);
                    ReleaseMutex(game->pipeData.hMutex);
                    _tprintf(_T("[SERVIDOR] Chegou o jogador %u\n"), i);
                }
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
   HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EXIT_EVENT);
    if (hEvent == NULL)
    {
        ExitThread(7);
    }
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

        if (!_tcscmp(cmd, _T("exit"))) { game->dwShutDown = 1; game->sharedData.terminar = 1; SetEvent(hEvent); ExitProcess(0); break; }

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
            restartGame(game);
        }
        else if (!(_tcscmp(cmd, _T("help")))) {
            _tprintf(_T("\n-- setv x : definir a velocidade dos carros ( x inteiro maior que 1)"));
            _tprintf(_T("\n-- setf x : definir o numero de faixas ( x inteiro entre 1 e 8)"));
            _tprintf(_T("\n-- pause : pausar o jogo"));
            _tprintf(_T("\n-- unpause : resumir o jogo"));
            _tprintf(_T("\n-- restart : recome�ar o jogo"));
            _tprintf(_T("\n-- exit : sair\n\n"));
        }
    }

    ExitThread(0);
}
DWORD WINAPI UpdateThread(LPVOID param) {
    GAME* game = (GAME*)param;
    DWORD n, count = 0;
    TCHAR underSymbol;
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
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game->pipeData.playerData[i].active) {
                    game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;
                }
            }
            continue;
        }
        for (int i = 0; i < game->dwInitNumOfRoads + 4; i++) {
            for (int j = 0; j < MAX_WIDTH; j++) {
                if (i == 1 || i == game->dwInitNumOfRoads + 2) {
                    game->sharedData.memPar->sharedBoard.board[i][j] = _T('-');
                }
                else {
                    game->sharedData.memPar->sharedBoard.board[i][j] = _T(' ');
                }
            }
        }
        for (int i = 0; i < game->dwInitNumOfRoads; i++) {
            for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                OBJECT obj = game->roads[i].cars[j];
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
                PLAYER_DATA playerData = game->pipeData.playerData[i];
                if (game->sharedData.memPar->sharedBoard.board[playerData.obj.dwY][playerData.obj.dwX] == _T('>') ||
                    game->sharedData.memPar->sharedBoard.board[playerData.obj.dwY][playerData.obj.dwX] == _T('<')) {
                    if(game->pipeData.playerData[i].dwPoints>10)
                        game->pipeData.playerData[i].dwPoints -=10;
                    changeLevel(game, FALSE);
                }
                else {
                    game->sharedData.memPar->sharedBoard.board[playerData.obj.dwY][playerData.obj.dwX] = playerData.obj.c;
                }
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
        if (road->bChanged) {
            runningCars = 0; numSteps = 0;
            road->bChanged = FALSE;
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
                road->cars[j].c = (road->way == RIGHT ? CAR_LEFT : CAR_RIGHT);
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
BOOL runClientRequest(GAME* game, TCHAR c, DWORD i) {
    WAY way = STOP;
    if (c == _T('Q')) return FALSE;
    if (c == _T('U')) {
        game->pipeData.playerData[i].dwPoints++;
        way = UP;
    }
    else if (c == _T('R')) {
        way = RIGHT;
    }
    else if (c == _T('D')) {
        way = DOWN;
        if (game->pipeData.playerData[i].dwPoints > 5) {
            game->pipeData.playerData[i].dwPoints -= 5;
        }
    }
    else if (c == _T('L')) {
        way = LEFT;
    }
    else if (c == _T('P')) {
        if (!game->bPaused) {
            game->bPaused = TRUE;
            for (int j = 0; j < game->dwInitNumOfRoads; j++) {
                game->roads[j].lastWay = game->roads[j].way;
                game->roads[j].way = STOP;
            }
        }
        else {
            game->bPaused = FALSE;
            for (int j = 0; j < game->dwInitNumOfRoads; j++) {
                game->roads[j].way = game->roads[j].lastWay;
            }
        }
    }
    else if (c == _T('B')) {
        game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;

    }
    else if (c == _T('1')) {
        game->pipeData.playerData[i].gameType = SINGLE_PLAYER;
        if (game->pipeData.dwNumClients < 2) {
            game->pipeData.playerData[i].bWaiting = FALSE;
            restartGame(game);
        }
        else {
            if (i == 0) {
                if (game->pipeData.playerData[1].gameType == MULTI_PLAYER) {
                    game->pipeData.playerData[1].bWaiting = TRUE;
                    restartGame(game);
                }
                else if (game->pipeData.playerData[1].gameType == SINGLE_PLAYER) {
                    game->pipeData.playerData[0].bWaiting = TRUE;
                }
                else if (game->pipeData.playerData[1].gameType == NONE) {
                    game->pipeData.playerData[1].bWaiting = TRUE;
                    restartGame(game);
                }
            }
            else if (i == 1) {
                if (game->pipeData.playerData[0].gameType == MULTI_PLAYER) {
                    game->pipeData.playerData[0].bWaiting = TRUE;
                    restartGame(game);
                }
                else if (game->pipeData.playerData[0].gameType == SINGLE_PLAYER) {
                    game->pipeData.playerData[1].bWaiting = TRUE;
                }
                else if (game->pipeData.playerData[0].gameType == NONE) {
                    game->pipeData.playerData[0].bWaiting = TRUE;
                    restartGame(game);
                }
            }
        }
    }
    else if (c == _T('2')) {
        game->pipeData.playerData[i].gameType = MULTI_PLAYER;
        if (game->pipeData.dwNumClients < 2) {
            game->pipeData.playerData[i].bWaiting = TRUE;
        }
        else {
            if (i == 0) {
                if (game->pipeData.playerData[1].gameType == SINGLE_PLAYER) {
                    game->pipeData.playerData[0].bWaiting = TRUE;
                }
                else if (game->pipeData.playerData[1].gameType == MULTI_PLAYER) {
                    game->pipeData.playerData[0].bWaiting = FALSE; game->pipeData.playerData[1].bWaiting = FALSE;
                    restartGame(game);
                }
                else if (game->pipeData.playerData[1].gameType == NONE) {
                    game->pipeData.playerData[0].bWaiting = TRUE;
                }
            }
            else if (i == 1) {
                if (game->pipeData.playerData[0].gameType == SINGLE_PLAYER) {
                    game->pipeData.playerData[1].bWaiting = TRUE;
                }
                else if (game->pipeData.playerData[0].gameType == MULTI_PLAYER) {
                    game->pipeData.playerData[0].bWaiting = FALSE; game->pipeData.playerData[1].bWaiting = FALSE;
                    restartGame(game);
                }
                else if (game->pipeData.playerData[0].gameType == NONE) {
                    game->pipeData.playerData[1].bWaiting = TRUE;
                }
            }
        }
    }

    if (game->pipeData.playerData[0].bWaiting && game->pipeData.playerData[1].bWaiting) {
        game->pipeData.playerData[0].bWaiting = FALSE; game->pipeData.playerData[1].bWaiting = FALSE;
        game->pipeData.playerData[0].gameType = MULTI_PLAYER; game->pipeData.playerData[1].gameType = MULTI_PLAYER;
        restartGame(game);
    }
    moveObject(&game->pipeData.playerData[i].obj, way);
    if (game->pipeData.playerData[i].obj.dwY == 0) {
        game->pipeData.playerData[i].dwPoints += 10;
        game->pipeData.playerData[i].dwNEndLevel++;
        game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;
        changeLevel(game, TRUE);
    }
    game->pipeData.playerData[i].dwAFKseg = 0;
    return TRUE;
}
void disconectClient(GAME* game, DWORD i) {
    WaitForSingleObject(game->pipeData.hMutex, INFINITE);
    game->pipeData.dwNumClients--;
    if (i == 0 && game->pipeData.playerData[1].gameType == MULTI_PLAYER) {
        game->pipeData.playerData[1].bWaiting = TRUE;
    }
    else if (i == 1 && game->pipeData.playerData[0].gameType == MULTI_PLAYER) {
        game->pipeData.playerData[0].bWaiting = TRUE;
    }
    game->pipeData.playerData[i].dwNEndLevel = 0;
    game->pipeData.playerData[i].dwPoints = 0;
    game->pipeData.playerData[i].dwAFKseg = 0;
    game->pipeData.playerData[i].gameType = NONE;
    game->pipeData.playerData[i].bWaiting = FALSE;
    game->pipeData.playerData[i].active = FALSE;
    DisconnectNamedPipe(game->pipeData.playerData[i].hPipe);
    ConnectNamedPipe(game->pipeData.playerData[i].hPipe, &game->pipeData.playerData[i].overlapRead);
    _tprintf(_T("[SERVIDOR]Saiu o jogador %u\n"), i);
    ReleaseMutex(game->pipeData.hMutex);
}
void moveObject(OBJECT* objData, WAY way) {
    switch (way) {
        case UP: {
            if (objData->dwY <= 0)break;
            objData->dwY--;
            break;
        }
        case DOWN: {
            if (objData->dwY >= MAX_ROADS+4)break;
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
void changeLevel(GAME* game,BOOL bNextLevel) {
    if (bNextLevel) {
        game->dwLevel++;
        if (game->dwInitNumOfRoads < MAX_ROADS) {
            game->dwInitNumOfRoads++;
            for (int i = 0; i < MAX_ROADS; i++) {
                game->roads[i].dwNumOfObjects = 0;
                game->roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / game->roads[i].dwNumOfCars) + 2;
                game->roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
                game->roads[i].dwTimeStoped = 0;
                if (rand() % 2 == 0)
                    game->roads[i].dwSpeed *= 0.5;
                else 
                    game->roads[i].dwSpeed *= 2;
                game->roads[i].bChanged = TRUE;
                for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                    game->roads[i].cars[j].c = (game->roads[i].way == RIGHT ? CAR_LEFT : CAR_RIGHT);
                    game->roads[i].cars[j].dwX = (game->roads[i].way == RIGHT ? 0 : MAX_WIDTH);
                    game->roads[i].cars[j].dwY = i + 2;

                }
            }
        }
        else {
            for (int i = 0; i < MAX_ROADS; i++) {
                if(game->roads[i].dwNumOfCars<MAX_CARS_PER_ROAD&&rand()%2==0)
                    game->roads[i].dwNumOfCars++;
                game->roads[i].dwNumOfObjects = 0;
                game->roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / game->roads[i].dwNumOfCars) + 2;
                game->roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
                game->roads[i].dwTimeStoped = 0;
                if (rand() % 2 == 0)
                    game->roads[i].dwSpeed *= 0.7;
                else
                    game->roads[i].dwSpeed *= 1.3;
                game->roads[i].bChanged = TRUE;
                for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
                    game->roads[i].cars[j].c = (game->roads[i].way == RIGHT ? CAR_LEFT : CAR_RIGHT);
                    game->roads[i].cars[j].dwX = (game->roads[i].way == RIGHT ? 0 : MAX_WIDTH);
                    game->roads[i].cars[j].dwY = i + 2;

                }
            }
        }
        
    }
 
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads+3;
    }
  
}
void restartGame(GAME* game) {
    game->dwLevel = 1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;
        game->pipeData.playerData[i].dwNEndLevel = 0;
        game->pipeData.playerData[i].dwPoints = 0;
    }
    for (int i = 0; i < MAX_ROADS; i++) {
        game->roads[i].dwNumOfCars = 1;
        game->roads[i].dwNumOfObjects = 0;
        game->roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / game->roads[i].dwNumOfCars) + 2;
        game->roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
        game->roads[i].dwTimeStoped = 0;
        game->roads[i].dwSpeed = game->dwInitSpeed * 500;
        game->roads[i].bChanged = TRUE;
        for (int j = 0; j < game->roads[i].dwNumOfCars; j++) {
            game->roads[i].cars[j].c = (game->roads[i].way == RIGHT ? CAR_LEFT : CAR_RIGHT);
            game->roads[i].cars[j].dwX = (game->roads[i].way == RIGHT ? 0 : MAX_WIDTH);
            game->roads[i].cars[j].dwY = i + 2;

        }
    }
}
void playerArrived(GAME* game, DWORD i) {
    game->pipeData.playerData[i].obj.dwX = rand() % MAX_WIDTH + 2;
    game->pipeData.playerData[i].obj.dwY = game->dwInitNumOfRoads + 3;
    game->pipeData.playerData[i].obj.c = FROG;
    game->pipeData.playerData[i].active = TRUE;
    game->pipeData.playerData[i].dwPoints = 0;
    game->pipeData.playerData[i].bWaiting = FALSE;
    game->pipeData.playerData[i].dwAFKseg = 0;
    game->pipeData.playerData[i].dwNEndLevel = 0;
    game->pipeData.playerData[i].gameType = NONE;
}
void initRegestry(GAME* data) {
    DWORD estado;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, KEY_DIR, 0, KEY_ALL_ACCESS, &(data->hKey)) != ERROR_SUCCESS) {
        _tprintf(_T("AQUI"));
        if (RegCreateKeyEx(HKEY_CURRENT_USER, KEY_DIR, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &(data->hKey), &estado) != ERROR_SUCCESS) {
            _tprintf(TEXT("\n[ERRO]Chave nao foi nem criada nem aberta! ERRO!"));
            ExitProcess(1);
        }
        else {
            
            _tprintf(_T("\n[AVISO] � necess�rio definir valores de velocidade e de n�mero de faixas antes de come�ar!\n"));
            do {
                _tprintf(_T("Velocidade:"));
                _tscanf_s(_T("%u"), &data->dwInitSpeed);
            } while (data->dwInitSpeed <= 0);

            if (RegSetValueEx(data->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&data->dwInitSpeed, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %d\n"), data->dwInitSpeed);

            do {
                _tprintf(_T("\nN�mero de faixas (MAX 8):"));
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
        DWORD valueType;
        if (RegQueryValueExW(data->hKey, KEY_SPEED, NULL, &valueType, (LPBYTE) & (data->dwInitSpeed), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("\n[AVISO] � necess�rio definir valores de velocidade antes de come�ar!\n"));
            do {
                _tprintf(_T("Velocidade:"));
                _tscanf_s(_T("%u"), &data->dwInitSpeed);
            } while (data->dwInitSpeed <= 0);

            if (RegSetValueEx(data->hKey, KEY_SPEED, 0, REG_DWORD, (LPCBYTE)&data->dwInitSpeed, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %d\n"), data->dwInitSpeed);

        }
        if (RegQueryValueExW(data->hKey, KEY_ROADS, NULL, &valueType, (LPBYTE) & (data->dwInitNumOfRoads), &dwSize) != ERROR_SUCCESS) {
            _tprintf(_T("\n[AVISO] � necess�rio definir valores de faixas antes de come�ar!\n"));

            do {
                _tprintf(_T("\nN�mero de faixas (MAX 8):"));
                _tscanf_s(_T("%u"), &data->dwInitNumOfRoads);
            } while (data->dwInitNumOfRoads > MAX_ROADS || data->dwInitNumOfRoads <= 0);

            if (RegSetValueEx(data->hKey, KEY_ROADS, 0, REG_DWORD, (LPCBYTE)&data->dwInitNumOfRoads, sizeof(DWORD)) != ERROR_SUCCESS)
                _tprintf(TEXT("\n[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Faixas = %u\n"), data->dwInitNumOfRoads);

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
        roads[i].dwNumOfCars = 1;
        roads[i].dwSpaceBetween = rand() % (MAX_WIDTH / MAX_CARS_PER_ROAD) + 2;
        roads[i].way = (rand() % 2 == 0 ? RIGHT : LEFT);
        roads[i].dwSpeed = dwInitSpeed * 500;
        roads[i].dwTimeStoped = 0;
        roads[i].hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RoadMove, (LPVOID)&roads[i], CREATE_SUSPENDED, NULL);
        roads[i].bChanged = FALSE;
        for (int j = 0; j < roads[i].dwNumOfCars; j++) {
            roads[i].cars[j].c = (roads[i].way == RIGHT ? CAR_LEFT : CAR_RIGHT);
            roads[i].cars[j].dwX = (roads[i].way == RIGHT ? 0 : MAX_WIDTH);
            roads[i].cars[j].dwY = i + 2; 
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
            sizeof(PIPE_GAME_DATA),
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
        pipeData->playerData[i].dwNEndLevel = 0;
        pipeData->playerData[i].dwPoints = 0;    
        pipeData->playerData[i].dwAFKseg = 0;
        pipeData->playerData[i].gameType = NONE;
        pipeData->playerData[i].bWaiting = FALSE;
        ConnectNamedPipe(pipeData->playerData[i].hPipe, &pipeData->playerData[i].overlapRead);
        ReadFile(pipeData->playerData[i].hPipe, NULL, 0, NULL, &pipeData->playerData[i].overlapRead);
    }
}
int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hExistServer, hUpdateEvent, hExitEvent,hSendPipeEvent;
    HANDLE hFileMap;
   
    HANDLE hThread[7];
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
    if ((game.sharedData.hdll = LoadLibrary(_T("DynamicLinkLibrary.dll"))) == NULL) {
        _tprintf(_T("[ERRO] Carregar dll\n"));
        ExitProcess(-1);
    }

    srand((unsigned int)time(NULL));
    game.dwShutDown = 0;
    game.dwLevel = 1;
    game.bPaused = FALSE;
    initRegestry(&game);
    hThread[0] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ThreadConsumidor, (LPVOID)&game, 0, NULL);
    initRoads(game.roads, game.dwInitSpeed);
    initPipeData(&game.pipeData);
    //lan�amos as estradas ao mesmo tempo
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
    hThread[6] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AFKCounter, (LPVOID)&game, 0, NULL);
    WaitForMultipleObjects(7, hThread, TRUE, INFINITE);

    SetEvent(hExitEvent);
    CloseHandle(game.hKey);
    CloseHandle(hExitEvent);
    CloseHandle(hMutexRoad);
    CloseHandle(hUpdateEvent);
    CloseHandle(hExistServer);

    return 0;
}