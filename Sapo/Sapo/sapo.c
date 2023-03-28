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
        _tprintf(_T("Erro ao criar o semáforo. Código do erro: %d\n"), GetLastError());
        return 0;
    }
    DWORD res = WaitForSingleObject(hSemaphore, 0); // tenta obter acesso ao semáforo
    if (res == WAIT_FAILED) {
        _tprintf(_T("Erro ao esperar pelo semáforo. Código do erro: %d\n"), GetLastError());
        return 0;
    }
    else if (res == WAIT_TIMEOUT) { // se o semáforo não estiver disponível
        _tprintf(_T("Já existem 2 instâncias deste programa em execução. Encerrando...\n"));
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

    hSemaphore = CreateSemaphore(NULL, 2, 2, _T("Sapo")); // cria o objeto de semáforo com valor inicial 2
    
    if (!CheckNumberOfInstances(hSemaphore)) {
        return 1;
    }

    // algumas cenas a null para ja so para funcionar
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    // aqui vai o código do programa
    //isto é so para testar
    _tprintf(_T("Instância permitida!\n"));

    while (1) {
      _tprintf(_T("SAPO\n\nEscreve quit para sair\n"));
      _tscanf_s(_T("%s"), STR, 5);
      if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }

    ReleaseSemaphore(hSemaphore, 1, NULL); // libera o semáforo
    CloseHandle(hSemaphore); // fecha o objeto de semáforo
    return 0;
}