#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#define LIGEIRO1_TO_LEFT _T("    ___\n\
 __/_|_|_\n\
|________|\n\
  O     O\n")
#define LIGEIRO1_TO_RIGHT _T("   ___\n\
 _|_|_\\__\n\
|________|\n\
 O     O\n")

#define LIGEIRO2_TO_LEFT _T("     _____\n\
 ___/_|___|\n\
|_________|\n\
   O     O\n")
#define LIGEIRO2_TO_RIGHT _T(" _____\n\
|___|_\\___\n\
|_________|\n\
 O      O\n")
#define PESADO_TO_LEFT _T("  ________\n\
 /__|__|__|\n\
|_________|\n\
  O      O\n")
#define PESADO_TO_RIGHT _T(" ________\n\
|__|__|__\\\n\
|_________|\n\
O      O\n")
#define SAPO _T(" ___\n\
/O O\\\n\
\\_%d_/\n")
#define PASSEIO _T("_________________________________________________________________________________________________________\n")

/*
* CARROS
      ___   
   __/_|_|_
  |________|
    O     O
      ________
     /__|__|__|
    |_________|
       O     O
       _____
   ___/_|___|
  |_________|
     O     O    
*SAPO
   ___
  /O O\
  \_1_/
   ___
  /O O\
  \_2_/
 */
void GoToXY(int column, int line) {
    COORD coord;
    coord.X = column;
    coord.Y = line;

    //Obter um handle para o ecra da consola
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

    // chaama a funcao de posicao do cursor call
    if (!SetConsoleCursorPosition(hConsole, coord))
    {
        // a funcao de posicao do cursor deu erro
    }
}

void Draw(const TCHAR* obj, int idSapo,int column,int line) {
    //Guarda a posicao anterior à do desenho que vai ser feito
    //depois de desenhar volta à posicao anterior
    CONSOLE_SCREEN_BUFFER_INFO consoleInfo;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &consoleInfo);
    COORD originalPosition = consoleInfo.dwCursorPosition;
    TCHAR object[200];
    //Se o objeto a desenhar é o sapo mete o id
    if (_tcscmp(obj, SAPO) == 0) {
        _stprintf_s(object, _countof(object), obj, idSapo);
    }
    else {
        _tcscpy_s(object, sizeof(object) / sizeof(TCHAR), obj);
    }
    int colAux = column;
    //mostra o objeto recebido
    //foi a melhor forma que consegui fazer
        for (int i = 0; object[i] != '\0'; i++) {
            if (object[i] == '\n') { line++; colAux = column; }
            else colAux++;
            GoToXY(colAux, line);
            _tprintf(_T("%c"), object[i]);
        }
      
    GoToXY(originalPosition.X, originalPosition.Y);
}
DWORD WINAPI CheckIfServerExit(LPVOID lpParam) {

    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "ExitServer");
    if (hEvent == NULL)
    {
        _tprintf(_T("Erro a abrir o evento\n\n"));
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
    //para representar os carros os sapos e os passeios
    _tprintf(PASSEIO);
    _tprintf(_T("%s%s%s%s%s%s"),LIGEIRO1_TO_LEFT,LIGEIRO1_TO_RIGHT,LIGEIRO2_TO_LEFT,LIGEIRO2_TO_RIGHT,PESADO_TO_LEFT,PESADO_TO_RIGHT);
    _tprintf(PASSEIO);
    _tprintf(SAPO, 1);
    //esta funcao leva 4 atributos, o array de TCHAR
    //o id do SAPO se o array de TCHAR enviado nao for o sapo esta variavel nao faz nada
    // posicao nas colunas, posicao nas linhas
    Draw(SAPO,2,50, 5);
  
    // algumas cenas a null para ja so para funcionar
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);

    while (1) {
        _tprintf(_T("OPERADOR\n\nEscreve quit para sair\n"));
        _tscanf_s(_T("%s"), STR, 5);
        if (_tcscmp(STR, _T("QUIT")) == 0 || _tcscmp(STR, _T("quit")) == 0) break;
    }
    ExitProcess(0);
}
