#include <tchar.h>
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <time.h>



#define TAM 200
#define TAM_BUF 10

#define MAX_ROADS 8
#define MAX_WIDTH 20

#define MUTEX_SERVER _T("Server")
#define SHARED_MEMORY_NAME _T("ShareMemory")
#define EXIT_EVENT _T("ExitEvent")
#define UPDATE_EVENT _T("UpdateEvent")
#define WRITE_SEMAPHORE _T("WriteSemaphore")
#define READ_SEMAPHORE _T("ReadSemaphore")
#define MUTEX_PROD _T("MutexProd")

typedef struct SHARED_BOARD {
    DWORD dwWidth;
    DWORD dwHeight;

    TCHAR board[MAX_ROADS + 4][MAX_WIDTH];
}SHARED_BOARD;
typedef struct CELULA_BUFFER {
    int id;
    TCHAR str[100];
}CELULA_BUFFER;

//representa a nossa memoria partilhada
typedef struct BUFFER_CIRCULAR {
    int nProdutores;
    int nConsumidores;
    int posE; //proxima posicao de escrita
    int posL; //proxima posicao de leitura
    CELULA_BUFFER buffer[TAM_BUF]; //buffer circular em si (array de estruturas)
}BUFFER_CIRCULAR;
typedef struct SHARED_MEMORY {
    BUFFER_CIRCULAR bufferCircular;
    SHARED_BOARD sharedBoard;
}SHARED_MEMORY;

//estrutura de apoio
typedef struct SHARED_DATA {
    SHARED_MEMORY* sharedMemory; //ponteiro para a memoria partilhada
    HANDLE hSemEscrita; //handle para o semaforo que controla as escritas (controla quantas posicoes estao vazias)
    HANDLE hSemLeitura; //handle para o semaforo que controla as leituras (controla quantas posicoes estao preenchidas)
    HANDLE hMutex;
    int terminar; // 1 para sair, 0 em caso contr?rio
    int id;
}SHARED_DATA;

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

DWORD WINAPI ReadSharedMemory(LPVOID param) {

    SHARED_BOARD* sharedBoard = (SHARED_BOARD*)param;

    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, UPDATE_EVENT);
    if (hEvent == NULL) {
        _tprintf(_T("Erro a abrir o evento\n\n"));
        ExitThread(7);
    }

    COORD pos = { 0,1 };
    DWORD written;
    while (TRUE) {
        fflush(stdin); fflush(stdout);
        WaitForSingleObject(hEvent, INFINITE);
        for (int i = 0; i < sharedBoard->dwHeight; i++) {
            WriteConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), sharedBoard->board[i], sharedBoard->dwWidth, pos, &written);
            pos.Y++;
        }
        ResetEvent(hEvent);
        pos.Y = 1;

    }
    ExitThread(0);
}

DWORD WINAPI CheckIfServerExit(LPVOID param) {
    HANDLE hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, EXIT_EVENT);
    if (hEvent == NULL) {
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


DWORD WINAPI ThreadProdutor(LPVOID param) {
    SHARED_DATA* dados = (SHARED_DATA*)param;
    COORD pos = { 0,0 };
    DWORD written;
    CELULA_BUFFER cel;

    while (!dados->terminar) {
        cel.id = dados->id;
        fflush(stdin); fflush(stdout);
        GoToXY(pos.X, pos.Y);
        FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), ' ', 50, pos, &written);
        _tprintf(_T("[OPERADOR]$ "));
        _fgetts(cel.str, 100, stdin);
        //aqui entramos na logica da aula teorica

        //esperamos por uma posicao para escrevermos
        WaitForSingleObject(dados->hSemEscrita, INFINITE);

        //esperamos que o mutex esteja livre
        WaitForSingleObject(dados->hMutex, INFINITE);

        //vamos copiar a variavel cel para a memoria partilhada (para a posi??o de escrita)
        CopyMemory(&dados->sharedMemory->bufferCircular.buffer[dados->sharedMemory->bufferCircular.posE], &cel, sizeof(CELULA_BUFFER));
        dados->sharedMemory->bufferCircular.posE++; //incrementamos a posicao de escrita para o proximo produtor escrever na posicao seguinte

        //se apos o incremento a posicao de escrita chegar ao fim, tenho de voltar ao inicio
        if (dados->sharedMemory->bufferCircular.posE == TAM_BUF)
            dados->sharedMemory->bufferCircular.posE = 0;

        //libertamos o mutex
        ReleaseMutex(dados->hMutex);

        //libertamos o semaforo. temos de libertar uma posicao de leitura
        ReleaseSemaphore(dados->hSemLeitura, 1, NULL);


    }

    return 0;
}

int _tmain(int argc, TCHAR* argv[])
{
    DWORD initX = 0, initY = 0;
    HANDLE hReadTh;
    HANDLE hServerTh;
    HANDLE hThreadProdutor;
    HANDLE hFileMap;
    SHARED_DATA dados;
    BOOL primeiroProcesso = FALSE;


#ifdef UNICODE
    _setmode(_fileno(stdin), _O_WTEXT);
    _setmode(_fileno(stdout), _O_WTEXT);
#endif

    srand((unsigned int)time(NULL));

    //criar semaforo que conta as escritas
    dados.hSemEscrita = CreateSemaphore(NULL, TAM_BUF, TAM_BUF, WRITE_SEMAPHORE);

    //criar semaforo que conta as leituras
    //0 porque nao ha nada para ser lido e depois podemos ir at? um maximo de 10 posicoes para serem lidas
    dados.hSemLeitura = CreateSemaphore(NULL, 0, TAM_BUF, READ_SEMAPHORE);

    //criar mutex para os produtores
    dados.hMutex = CreateMutex(NULL, FALSE, MUTEX_PROD);

    if (dados.hSemEscrita == NULL || dados.hSemLeitura == NULL || dados.hMutex == NULL) {
        _tprintf(TEXT("Erro no CreateSemaphore ou no CreateMutex\n"));
        return -1;
    }

    //o openfilemapping vai abrir um filemapping com o nome que passamos no lpName
    //se devolver um HANDLE ja existe e nao fazemos a inicializacao
    //se devolver NULL nao existe e vamos fazer a inicializacao

    hFileMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEMORY_NAME);
    if (hFileMap == NULL) {
        primeiroProcesso = TRUE;
        //criamos o bloco de memoria partilhada
        hFileMap = CreateFileMapping(
            INVALID_HANDLE_VALUE,
            NULL,
            PAGE_READWRITE,
            0,
            sizeof(SHARED_MEMORY), //tamanho da memoria partilhada
            SHARED_MEMORY_NAME);//nome do filemapping. nome que vai ser usado para partilha entre processos

        if (hFileMap == NULL) {
            _tprintf(TEXT("Erro no CreateFileMapping\n"));
            return -1;
        }
    }

    //mapeamos o bloco de memoria para o espaco de endera?amento do nosso processo
    dados.sharedMemory = (SHARED_MEMORY*)MapViewOfFile(hFileMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);


    if (dados.sharedMemory == NULL) {
        _tprintf(TEXT("Erro no MapViewOfFile\n"));
        return -1;
    }

    if (primeiroProcesso == TRUE) {
        dados.sharedMemory->bufferCircular.nConsumidores = 0;
        dados.sharedMemory->bufferCircular.nProdutores = 0;
        dados.sharedMemory->bufferCircular.posE = 0;
        dados.sharedMemory->bufferCircular.posL = 0;
    }


    dados.terminar = 0;

    //temos de usar o mutex para aumentar o nProdutores para termos os ids corretos
    WaitForSingleObject(dados.hMutex, INFINITE);
    dados.sharedMemory->bufferCircular.nProdutores++;
    dados.id = dados.sharedMemory->bufferCircular.nProdutores;
    ReleaseMutex(dados.hMutex);


    //lancamos a thread
    hReadTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadSharedMemory, (LPVOID)&dados.sharedMemory->sharedBoard, 0, NULL);
    hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);
    hThreadProdutor = CreateThread(NULL, 0, ThreadProdutor, &dados, 0, NULL);

    WaitForSingleObject(hServerTh, INFINITE);

    UnmapViewOfFile(dados.sharedMemory);
    CloseHandle(hReadTh);
    CloseHandle(hServerTh);
    ExitProcess(0);
    return 0;

}