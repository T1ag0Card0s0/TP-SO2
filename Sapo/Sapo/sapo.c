#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>

int _tmain(int argc, TCHAR* argv[]) {
    TCHAR STR[5];
    HANDLE hSemaphore = CreateSemaphore(NULL, 2, 2, _T("Sapo")); // cria o objeto de semáforo com valor inicial 2
#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    if (hSemaphore == NULL){
        _tprintf(_T("Erro ao criar o semáforo. Código do erro: %d\n"), GetLastError());
        return 1;
    }
    DWORD res = WaitForSingleObject(hSemaphore, 0); // tenta obter acesso ao semáforo
    if (res == WAIT_FAILED) {
        _tprintf(_T("Erro ao esperar pelo semáforo. Código do erro: %d\n"), GetLastError());
        return 2;
    }else if (res == WAIT_TIMEOUT){ // se o semáforo não estiver disponível
        _tprintf(_T("Já existem 2 instâncias deste programa em execução. Encerrando...\n"));
        CloseHandle(hSemaphore);
        return 3;
    }
    // aqui vai o código do programa
    //isto é so para testar
    _tprintf(_T("Instância permitida!\n"));
    while (1) {
      _tprintf(_T("Escreve quit para sair\n"));
      _tscanf_s(_T("%s"), STR, 5);
      if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }
    ReleaseSemaphore(hSemaphore, 1, NULL); // libera o semáforo
    CloseHandle(hSemaphore); // fecha o objeto de semáforo
    return 0;
}