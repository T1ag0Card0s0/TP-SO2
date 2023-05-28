#include "operador.h"

DWORD WINAPI ShowBoard(LPVOID param) {

    SHARED_DATA* dados = (SHARED_DATA*)param;

    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(-1);
    }
    COORD pos = { 0,1 };
    DWORD written;
    while (TRUE) {
        fflush(stdin); fflush(stdout);
        WaitForSingleObject(hEvent, INFINITE);
        for (int i = 0; i < MAX_ROADS + 4; i++) {
            if (i < dados->memPar->sharedBoard.dwHeight)
                WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), dados->memPar->sharedBoard.board[i], dados->memPar->sharedBoard.dwWidth, pos, &written);
            else
                FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', MAX_WIDTH, pos, &written);
            pos.Y++;
        }
        pos.Y = 1;
        ResetEvent(hEvent);


    }
    ExitThread(0);
}

DWORD WINAPI CheckIfServerExit(LPVOID param) {
    DWORD* shutdown = (DWORD*)param;
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EXIT_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(7);
    }
    // Esperar pelo evento
    WaitForSingleObject(hEvent, INFINITE);
    *shutdown = 1;
    _tprintf(_T("Desconectado...\n"));
    // Server saiu entao sai
    CloseHandle(hEvent);
    ExitProcess(0);
}


DWORD WINAPI ThreadProdutor(LPVOID param) {
    SHARED_DATA* dados = (SHARED_DATA*)param;
    PFUNC_PROD pfunc;
    if ((pfunc = (PFUNC_PROD)GetProcAddress(dados->hdll, "produtor")) == NULL) ExitThread(-1);
    ExitThread(pfunc(dados));
    ExitThread(0);
}

int _tmain(int argc, TCHAR* argv[])
{
    DWORD initX = 0, initY = 0;
    HANDLE hShowBoardTh;
    HANDLE hServerTh;
    HANDLE hThreadProdutor;
    SHARED_DATA dados;


#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif
    srand(time(NULL));
    if ((dados.hdll = LoadLibrary(_T("..\\..\\DynamicLinkLibrary\\x64\\Debug\\DynamicLinkLibrary.dll"))) == NULL) ExitProcess(-1);

    //lancamos a thread
    hThreadProdutor = CreateThread(NULL, 0, ThreadProdutor, (LPVOID)&dados, 0, NULL);
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, (LPVOID)&dados.terminar, 0, NULL);
    hShowBoardTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ShowBoard, (LPVOID)&dados, 0, NULL);

    WaitForSingleObject(hThreadProdutor, INFINITE);

    CloseHandle(hShowBoardTh);
    CloseHandle(hServerTh);
    CloseHandle(hThreadProdutor);
    ExitProcess(0);
    return 0;

}
