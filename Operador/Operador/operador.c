#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

DWORD WINAPI CheckIfServerExit(LPVOID lpParam) {

    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "ExitServer");
    if (hEvent == NULL)
    {
        _tprintf(_T("Deu merda..\n\n"));
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
    HANDLE hServerTh;
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

    // algumas cenas a null para ja so para funcionar
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    while (1) {
        _tprintf(_T("OPERADOR\n\nEscreve quit para sair\n"));
        _tscanf_s(_T("%s"), STR, 5);
        if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }
    ExitProcess(0);
}