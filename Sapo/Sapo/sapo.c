#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

int CheckNumberOfInstances(HANDLE hSemaphore) {
    if (OpenMutex(SYNCHRONIZE, FALSE, _T("Servidor")) == NULL) {
        _tprintf(_T("O servidor ainda nao esta a correr\n"));
        return 0;
    }
    if (hSemaphore == NULL) {
        _tprintf(_T("Erro ao criar o sem�foro. C�digo do erro: %d\n"), GetLastError());
        return 0;
    }
    DWORD res = WaitForSingleObject(hSemaphore, 0); // tenta obter acesso ao sem�foro
    if (res == WAIT_FAILED) {
        _tprintf(_T("Erro ao esperar pelo sem�foro. C�digo do erro: %d\n"), GetLastError());
        return 0;
    }
    else if (res == WAIT_TIMEOUT) { // se o sem�foro n�o estiver dispon�vel
        _tprintf(_T("J� existem 2 inst�ncias deste programa em execu��o. Encerrando...\n"));
        CloseHandle(hSemaphore);
        return 0;
    }
    return 1;
}


DWORD WINAPI CheckIfServerExit(LPVOID lpParam) {
    // Abrir o evento
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "ExitServer");
    if (hEvent == NULL)
    {
        _tprintf(_T("Deu merda..\n\n"));
        ExitThread(7);
    }

    // Esperar pelo evento
    WaitForSingleObject(hEvent, INFINITE);
    // Server saiu entao sai
    CloseHandle(hEvent);
    ExitProcess(0);
}

int _tmain(int argc, TCHAR* argv[]) {
    TCHAR STR[5];
    HANDLE hSemaphore; 
    HANDLE hServerTh;
#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif

    hSemaphore = CreateSemaphore(NULL, 2, 2, _T("Sapo")); // cria o objeto de sem�foro com valor inicial 2
    
    if (!CheckNumberOfInstances(hSemaphore)) {
        return 1;
    }

    // algumas cenas a null para ja so para funcionar
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    // aqui vai o c�digo do programa
    //isto � so para testar
    _tprintf(_T("Inst�ncia permitida!\n"));

    while (1) {
      _tprintf(_T("SAPO\n\nEscreve quit para sair\n"));
      _tscanf_s(_T("%s"), STR, 5);
      if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }

    ReleaseSemaphore(hSemaphore, 1, NULL); // libera o sem�foro
    CloseHandle(hSemaphore); // fecha o objeto de sem�foro
    return 0;
}