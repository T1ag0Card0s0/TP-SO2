#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hMutex = CreateMutex(NULL, TRUE, _T("Servidor"));
    TCHAR STR[5];
#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        _tprintf(_T("Ja existe um servidor a correr"));
        //Apenas um servidor pode existir
        return 0;
    }
    //Este ciclo apenas serviu para verificar se funciona ou nao a validaçao de instancias
    while (1) {
        _tprintf(_T("Escreve quit para sair\n"));
        _tscanf_s(_T("%s"), STR,5);
        if (_tcscmp(STR,_T("QUIT")) == 0|| _tcscmp(STR, _T("quit")) == 0) break;
    }
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    return 0;
}