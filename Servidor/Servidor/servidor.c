#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#define TAM 200
#define MAX_FAIXAS 8

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

DWORD WINAPI cmdThread(LPVOID lparam) {
    TCHAR cmd[TAM];
    TCHAR linha[TAM];
    keyDados* p = (keyDados*)lparam;
    TCHAR value[TAM];

    do {
        _tprintf(_T("\n[SERV] $ "));
        _fgetts(linha, TAM, stdin);
        _stscanf_s(linha, _T("%s %s\n"), cmd, TAM, value, TAM); //falta verificações de comandos

        if (!(_tcscmp(cmd, _T("setf")))) {

            if (_wtoi(value) < 1 || _wtoi(value) > MAX_FAIXAS) {
                _tprintf(_T("Insira um valor entre 1 e 8\n"), value);
            }
            else {
                if (RegSetValueEx(p->hkey, _T("Faixas"), 0, REG_SZ, (LPCBYTE)&value, sizeof(TCHAR) * (_tcslen(value) + 1)) != ERROR_SUCCESS)
                    _tprintf(TEXT("[AVISO] O atributo nao foi alterado nem criado!\n"));
                else
                    _tprintf(TEXT("Faixas = %s\n"), value);
            }
        }
        else if (!(_tcscmp(cmd, _T("setv")))) {

            if (RegSetValueEx(p->hkey, _T("Velocidade"), 0, REG_SZ, (LPCBYTE)&value, sizeof(TCHAR) * (_tcslen(value) + 1)) != ERROR_SUCCESS)
                _tprintf(TEXT("[AVISO] O atributo nao foi alterado nem criado!\n"));
            else
                _tprintf(TEXT("Velocidade = %s\n"), value);
        }
        else if (!(_tcscmp(cmd, _T("help")))) {

            _tprintf(_T("\n-- setv x : definir a velocidade dos carros ( x inteiro maior que 1)"));
            _tprintf(_T("\n-- setf x : definir o numero de faixas ( x inteiro entre 1 e 8)"));
            _tprintf(_T("\n-- exit : sair\n\n"));
        }

        fflush(stdin);
        fflush(stdout);
    } while ((_tcscmp(cmd, _T("exit"))));

    RegCloseKey(p->hkey);
    ExitThread(0);
}

int _tmain(int argc, TCHAR* argv[]) {
    HANDLE hMutex;
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

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        _tprintf(_T("[ERRO] Ja existe um servidor a correr\n"));
        //Apenas um servidor pode existir
        ExitProcess(1);
    }

    _tprintf(_T("Bem vindo ao Servidor, digite 'help' se precisar de ajuda!\n"));

    // cria a chave e verifca os erros
    if (RegCreateKeyEx(HKEY_CURRENT_USER, TEXT("Software\\TP-SO2\\"), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &(key.hkey), &(key.dwEstado)) != ERROR_SUCCESS) {
        _tprintf(TEXT("[ERRO]Chave nao foi nem criada nem aberta! ERRO!"));
        ExitProcess(1);
    }
    else {// se criou a chave vai verificar se já há dados de velocidade e numero de faixas
        initValues.velocidade[0] = '\0';
        initValues.faixas[0] = '\0';
        DWORD tamanho = sizeof(initValues.velocidade);
        // se houver dados já guardados mostra
        if (RegQueryValueEx(key.hkey, _T("Velocidade"), 0, NULL, (LPBYTE)initValues.velocidade, &tamanho) == ERROR_SUCCESS) {
            _tprintf(TEXT("\nVelocidade = %s\n"), initValues.velocidade);
        }
        else {
            _tprintf(_T("\n[AVISO] É necessário definir uma velocidade antes de começar.\n"));
        }

        if (RegQueryValueEx(key.hkey, _T("Faixas"), 0, NULL, (LPBYTE)initValues.faixas, &tamanho) == ERROR_SUCCESS) {
            _tprintf(TEXT("\nNúmero de Faixas = %s\n"), initValues.faixas);
        }
        else {
            _tprintf(_T("\n[AVISO] É necessário definir o número de faixas antes de começar.\n"));
        }
    }

    hMutex = CreateMutex(NULL, TRUE, _T("Servidor"));

    // Criar o evento para fechar os outros processos
    hEvent = CreateEvent(NULL, TRUE, FALSE, "ExitServer");

    if (hEvent == NULL)
    {
        _tprintf(_T("[ERRO] Evento não criado\n\n"));
        ExitProcess(1);
    }

    // cria thread para comandos do utilizador
    hCmdTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)cmdThread, &(key), NULL, NULL);

    WaitForSingleObject(hCmdTh, INFINITE);

    // saiu da thread e quer fechar
    SetEvent(hEvent);

    CloseHandle(hEvent);
    CloseHandle(hCmdTh);
    RegCloseKey(key.hkey);
    ReleaseMutex(hMutex);
    CloseHandle(hMutex);
    ExitProcess(0);
}