#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#define TAM 200

typedef struct keyDados {
    DWORD dwType;
    DWORD dwValue;
    DWORD dwEstado;
    DWORD dwSize;
    HANDLE hkey;
}keyDados;

typedef struct InitialValues {
    TCHAR velocidade[TAM];
    TCHAR faixas[TAM];
}InitialValues;
int isNumber(TCHAR value[]) {//se o TCHAR introduzido for numero retorna 1 se nao retorna 0
    for (int i = 0; value[i] != '\0'; i++)
        if (!isdigit(value[i])) return 0;
    return 1;
}
DWORD WINAPI cmdThread(LPVOID lparam) {
    TCHAR cmd[TAM];
    TCHAR linhaComando[TAM];
    keyDados* p = (keyDados*)lparam;
    TCHAR value[TAM];

    do {
        _tprintf(_T("\nOperation\n->"));
        _fgetts(linhaComando, TAM, stdin);
        _stscanf_s(linhaComando, _T("%s %s\n"), cmd, TAM, value, TAM); //falta verificações de comandos
        if (!isNumber(value)) continue;
        if (!(_tcscmp(cmd, _T("setv")))) { // COMANDO "SETV .." MUDA VALOR DE VELOCIDADE

            if (RegSetValueEx(p->hkey, _T("Velocidade"), 0, REG_SZ, (LPCBYTE)&value, sizeof(TCHAR) * (_tcslen(value) + 1)) != ERROR_SUCCESS)
                _tprintf(TEXT("O atributo nao foi alterado nem criado! ERRO!\n"));
            else
                _tprintf(TEXT("Velocidade = %s\n"), value);
        }
        else if (!(_tcscmp(cmd, _T("setf")))) { // COMANDO "SETF .." MUDA VALOR DE FAIXAS
            if (RegSetValueEx(p->hkey, _T("Faixas"), 0, REG_SZ, (LPCBYTE)&value, sizeof(TCHAR) * (_tcslen(value) + 1)) != ERROR_SUCCESS)
                _tprintf(TEXT("O atributo nao foi alterado nem criado! ERRO!\n"));
            else
                _tprintf(TEXT("Faixas = %s\n"), value);
        }

        fflush(stdin);
        fflush(stdout);
    } while ((_tcscmp(cmd, _T("exit"))));

    RegCloseKey(p->hkey);
    ExitThread(0);
}

int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hEvent; // handle para o evento de quando o servidor fecha
    HANDLE hCmdTh; // handle para a thread que trata dos comandos
    keyDados key; // struct para valores da key (estado, value ,...)
    InitialValues initValues; // struct para guardar valores iniciais (velocidade, numero faixas) PROVISORIO
    TCHAR chave_dir[TAM] = TEXT("Software\\TP-SO2\\");

#ifdef UNICODE 
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
    _setmode(_fileno(stderr), _O_WTEXT);
#endif
    // Criar o evento para fechar os outros processos
    hEvent = CreateEvent(NULL, TRUE, FALSE, "ExitServer");
    if (hEvent == NULL) {
        _tprintf(_T("ERRO AO CRIAR EVENTO\n\n"));
        ExitProcess(1);
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        _tprintf(_T("Ja existe um servidor a correr\n"));
        //Apenas um servidor pode existir
        SetEvent(hEvent);
        CloseHandle(hEvent);
        ExitProcess(0);
    }
    _tprintf(_T("SERVIDOR\n"));

    // cria a chave e verifca os erros
    if (RegCreateKeyEx(HKEY_CURRENT_USER, chave_dir, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &(key.hkey), &(key.dwEstado)) != ERROR_SUCCESS) {
        _tprintf(_T("Chave nao foi nem criada nem aberta! ERRO!"));
        ExitProcess(1);
    }
    else { // se criou a chave vai verificar se já ha dados de velocidade e numero de faixas
        initValues.velocidade[0] = '\0';
        initValues.faixas[0] = '\0';
        DWORD tamanho = sizeof(initValues.velocidade);
        // se houver dados já guardados mostra
        if (RegQueryValueEx(key.hkey, _T("Velocidade"), 0, NULL, (LPBYTE)initValues.velocidade, &tamanho) == ERROR_SUCCESS) {
            _tprintf(TEXT("\nAtributo Velocidade com o valor : %s\n"), initValues.velocidade);
        }

        if (RegQueryValueEx(key.hkey, _T("Faixas"), 0, NULL, (LPBYTE)initValues.faixas, &tamanho) == ERROR_SUCCESS) {
            _tprintf(TEXT("\nAtributo Faixas com o valor : %s\n"), initValues.faixas);
        }
    }
    // cria thread para comandos do utilizador
    hCmdTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)cmdThread, &(key), NULL, NULL);

    WaitForSingleObject(hCmdTh, INFINITE);
    // saiu da thread e quer fechar
    SetEvent(hEvent);
    CloseHandle(hEvent);
    CloseHandle(hCmdTh);
    RegCloseKey(key.hkey);
    return 0;
}