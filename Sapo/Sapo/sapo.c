#include "sapo.h"

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
	static PIPE_DATA pipeData;
	static HANDLE hThread;
	PAINTSTRUCT ps;
	HANDLE hdc;
	HBITMAP hBitMap1,hBitMap2;
	HDC hdcMem;
	switch (messg) {
	case WM_CREATE:
		if (initPipeData(&pipeData)) DestroyWindow(hWnd);
		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadPipeThread, (LPVOID)&pipeData, 0, NULL);
		break;
	case WM_PAINT :
		hdc = BeginPaint(hWnd,&ps);
		hdcMem = CreateCompatibleDC(hdc);
		hBitMap1 = (HBITMAP)LoadImage(NULL, _T("car.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBitMap2 = (HBITMAP)LoadImage(NULL, _T("frog.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		for (int i = 0; i < pipeData.sharedBoard.dwHeight; i++) {
			for (int j = 0; j < pipeData.sharedBoard.dwWidth; j++) {
				if (pipeData.sharedBoard.board[i][j] == _T('C')) {
					SelectObject(hdcMem, hBitMap1);
					BitBlt(hdc,0, 0, 50, 50, hdcMem, 0, 0, SRCCOPY);
				}
			else if (pipeData.sharedBoard.board[i][j] == _T('F')) {
					SelectObject(hdcMem, hBitMap2);
					BitBlt(hdc, 50, 50, 50, 50, hdcMem, 0, 0, SRCCOPY);
				}
			}
		}
		DeleteDC(hdcMem);
		DeleteObject(hBitMap1);
		DeleteObject(hBitMap2);
		EndPaint(hWnd, &ps);
		break;
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_UP:
			writee(&pipeData, _T('U'));
			break;
		case VK_RIGHT:
			writee(&pipeData, _T('R'));
			break;
		case VK_DOWN:
			writee(&pipeData, _T('D'));
			break;
		case VK_LEFT:
			writee(&pipeData, _T('L'));
			break;
		case VK_SPACE:
			writee(&pipeData, _T('P'));
			break;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		CloseHandle(hThread);
		if (IDYES == MessageBox(hWnd, _T("Deseja sair?"), _T("Sair"), MB_YESNO | MB_ICONQUESTION))
			DestroyWindow(hWnd);
		break;
	default:
		return(DefWindowProc(hWnd, messg, wParam, lParam));
		break;
	}
	return(0);
}
DWORD WINAPI ReadPipeThread(LPVOID param) {
	PIPE_DATA* pipeData = (PIPE_DATA*)param;
	SHARED_BOARD sharedBoard;
	DWORD n;
	while (!pipeData->dwShutDown) {
		if (GetOverlappedResult(pipeData->hPipe, &pipeData->overlapRead, &n, TRUE)) {			
			ZeroMemory(&pipeData->sharedBoard, sizeof(pipeData->sharedBoard));
			ReadFile(pipeData->hPipe, &pipeData->sharedBoard, sizeof(pipeData->sharedBoard), &n, &pipeData->overlapRead);
		}
	}
	ExitThread(0);
}
DWORD WINAPI CheckIfServerExit(LPVOID lpParam) {
	// Abrir o evento
	HANDLE hEvent;
	hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "ExitServer");
	if (hEvent == NULL)
	{
		_tprintf(_T("Erro a abrir evento..\n\n"));
		ExitThread(7);
	}
	// Esperar pelo evento
	WaitForSingleObject(hEvent, INFINITE);
	_tprintf(_T("Desconectado...\n"));
	// Server saiu entao sai
	CloseHandle(hEvent);
	ExitProcess(0);
}
int CheckNumberOfInstances(HANDLE hSemaphore) {
	/* if (OpenMutex(SYNCHRONIZE, FALSE, _T("Servidor")) == NULL) {
		 _tprintf(_T("O servidor ainda nao esta a correr\n"));
		 return 0;
	 }*/
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
		_tprintf(_T("Já existem 2 instâncias deste programa em execução. Encerrar...\n"));
		CloseHandle(hSemaphore);
		return 0;
	}
	return 1;
}
int initPipeData(PIPE_DATA* pipeData) {
	pipeData->hPipe = CreateFile(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (pipeData->hPipe == INVALID_HANDLE_VALUE) {
		return 1;
	}
	ZeroMemory(&pipeData->overlapRead, sizeof(pipeData->overlapRead));
	ZeroMemory(&pipeData->overlapWrite, sizeof(pipeData->overlapWrite));
	pipeData->overlapRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	pipeData->overlapWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	pipeData->dwShutDown = 0;
	pipeData->bNewBoard = FALSE;
	return 0;
}
int writee(PIPE_DATA* pipeData, TCHAR c) {
	DWORD n;
	if (!WriteFile(pipeData->hPipe, &c, sizeof(c), &n, &pipeData->overlapWrite)) {
		return 1;
	}
	return 0;
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nCmdShow) {

	HANDLE hSemaphore;
	HANDLE hServerTh;
	if (OpenMutex(SYNCHRONIZE, FALSE, MUTEX_SERVER) == NULL) {
		_tprintf(_T("O servidor ainda nao esta a correr\n"));
		return 1;
	}

	hSemaphore = CreateSemaphore(NULL, 2, 2, _T("Sapo")); // cria o objeto de semáforo com valor inicial 2

	if (!CheckNumberOfInstances(hSemaphore)) {
		return 1;
	}

	// algumas cenas a null para ja so para funcionar
	hServerTh = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckIfServerExit, NULL, 0, NULL);
	TCHAR szProgName[] = TEXT("Base");
	HWND hWnd;		// hWnd é o handler da janela, gerado mais abaixo por CreateWindow()
	MSG lpMsg;		// MSG é uma estrutura definida no Windows para as mensagens
	WNDCLASSEX wcApp;	// WNDCLASSEX é uma estrutura cujos membros servem para 
	srand(time(NULL));

	wcApp.cbSize = sizeof(WNDCLASSEX);      // Tamanho da estrutura WNDCLASSEX
	wcApp.hInstance = hInst;		         // Instância da janela actualmente exibida 
	// ("hInst" é parâmetro de WinMain e vem 
		  // inicializada daí)
	wcApp.lpszClassName = szProgName;       // Nome da janela (neste caso = nome do programa)
	wcApp.lpfnWndProc = TrataEventos;       // Endereço da função de processamento da janela
	wcApp.style = CS_HREDRAW | CS_VREDRAW;  // Estilo da janela: Fazer o redraw se for
	// modificada horizontal ou verticalmente

	wcApp.hIcon = LoadIcon(NULL, IDI_APPLICATION);   // "hIcon" = handler do ícon normal
	// "NULL" = Icon definido no Windows
	// "IDI_AP..." Ícone "aplicação"
	wcApp.hIconSm = (HICON)LoadImage(NULL, _T("frogger.ico"), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE | LR_SHARED);

	// "NULL" = Icon definido no Windows
	// "IDI_INF..." Ícon de informação
	wcApp.hCursor = LoadCursor(NULL, IDC_ARROW);	// "hCursor" = handler do cursor (rato) 
	// "NULL" = Forma definida no Windows
	// "IDC_ARROW" Aspecto "seta" 
	wcApp.lpszMenuName = NULL;			// Classe do menu que a janela pode ter
	// (NULL = não tem menu)
	wcApp.cbClsExtra = 0;				// Livre, para uso particular
	wcApp.cbWndExtra = 0;				// Livre, para uso particular
	wcApp.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	// "hbrBackground" = handler para "brush" de pintura do fundo da janela. Devolvido por
	// "GetStockObject".Neste caso o fundo será branco

	if (!RegisterClassEx(&wcApp))
		return(0);

	hWnd = CreateWindow(
		szProgName,			// Nome da janela (programa) definido acima
		TEXT("Frogger"),// Texto que figura na barra do título
		WS_OVERLAPPEDWINDOW,	// Estilo da janela (WS_OVERLAPPED= normal)
		CW_USEDEFAULT,		// Posição x pixels (default=à direita da última)
		CW_USEDEFAULT,		// Posição y pixels (default=abaixo da última)
		CW_USEDEFAULT,		// Largura da janela (em pixels)
		CW_USEDEFAULT,		// Altura da janela (em pixels)
		(HWND)HWND_DESKTOP,	// handle da janela pai (se se criar uma a partir de
		// outra) ou HWND_DESKTOP se a janela for a primeira, 
		// criada a partir do "desktop"
		(HMENU)NULL,			// handle do menu da janela (se tiver menu)
		(HINSTANCE)hInst,		// handle da instância do programa actual ("hInst" é 
		// passado num dos parâmetros de WinMain()
		0);

	ShowWindow(hWnd, nCmdShow);	// "hWnd"= handler da janela, devolvido por 
	// "CreateWindow"; "nCmdShow"= modo de exibição (p.e. 
	// normal/modal); é passado como parâmetro de WinMain()
	UpdateWindow(hWnd);		// Refrescar a janela (Windows envia à janela uma 
	// mensagem para pintar, mostrar dados, (refrescar)… 

	while (GetMessage(&lpMsg, NULL, 0, 0)) {
		TranslateMessage(&lpMsg);	// Pré-processamento da mensagem (p.e. obter código 
		// ASCII da tecla premida)
		DispatchMessage(&lpMsg);	// Enviar a mensagem traduzida de volta ao Windows, que

	}

	ReleaseSemaphore(hSemaphore, 1, NULL); // libera o semáforo
	CloseHandle(hSemaphore); // fecha o objeto de semáforo
	return((int)lpMsg.wParam);	// Retorna sempre o parâmetro wParam da estrutura lpMsg
}