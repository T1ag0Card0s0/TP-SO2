#include "sapo.h"

LRESULT CALLBACK TrataEventos(HWND hWnd, UINT messg, WPARAM wParam, LPARAM lParam) {
	static PIPE_DATA pipeData;
	static HANDLE hThread;
	static HDC bmpDC[NUM_BMP_FILES];
	static BITMAP bmp[NUM_BMP_FILES];
	static HANDLE hMutex;
	static HDC memDC = NULL;
	static DWORD XOffset, YOffset;
	MINMAXINFO* mmi;
	HBITMAP hBmp[NUM_BMP_FILES];
	PAINTSTRUCT ps;
	HDC hdc;
	RECT rect;
	DWORD dwXMouse, dwYMouse;
	switch (messg) {
	case WM_CREATE: {
		hBmp[0] = (HBITMAP)LoadImage(NULL, _T("car-left.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[1] = (HBITMAP)LoadImage(NULL, _T("car-right.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[2] = (HBITMAP)LoadImage(NULL, TEXT("frog-right.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[3] = (HBITMAP)LoadImage(NULL, TEXT("frog-down.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[4] = (HBITMAP)LoadImage(NULL, TEXT("frog-left.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[5] = (HBITMAP)LoadImage(NULL, TEXT("frog-up.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[6] = (HBITMAP)LoadImage(NULL, TEXT("passeio.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		hBmp[7] = (HBITMAP)LoadImage(NULL, TEXT("wood.bmp"), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		for (int i = 0; i < NUM_BMP_FILES; i++) {
			GetObject(hBmp[i], sizeof(bmp[i]), &bmp[i]);
		}
		hdc = GetDC(hWnd);
		for (int i = 0; i < NUM_BMP_FILES; i++) {
			bmpDC[i] = CreateCompatibleDC(hdc);
			SelectObject(bmpDC[i], hBmp[i]);
		}
		ReleaseDC(hWnd, hdc);

		GetClientRect(hWnd, &rect);
		XOffset = (rect.right / 2) - ((MAX_WIDTH * 30) / 2);
		YOffset = (rect.bottom / 2) - (((MAX_ROADS + 4) * 30) / 2);
		pipeData.paintData.hMutex = CreateMutex(NULL, FALSE, NULL);

		hMutex = CreateMutex(NULL, FALSE, NULL);
		pipeData.paintData.hMutex = hMutex;
		pipeData.paintData.hWnd = hWnd;
		for (int i = 0; i < NUM_BMP_FILES; i++) {
			pipeData.paintData.bmp[i] = bmp[i];
			pipeData.paintData.bmpDC[i] = bmpDC[i];
		}
		pipeData.paintData.memDC = &memDC;
		pipeData.sentido = _T('P');
		pipeData.paintData.XOffset = &XOffset;
		pipeData.paintData.YOffset = &YOffset;
		pipeData.pipeGameData.dwLevel = 0;
		pipeData.pipeGameData.dwPlayer1Lives = 3; pipeData.pipeGameData.dwPlayer2Lives = 3;
		pipeData.pipeGameData.dwPlayer1Points = 0; pipeData.pipeGameData.dwPlayer2Points = 0;
		pipeData.pipeGameData.dwX = 0;
		pipeData.pipeGameData.dwY = 0;
		if (initPipeData(&pipeData)) DestroyWindow(hWnd);
		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ReadPipeThread, (LPVOID)&pipeData, 0, NULL);
		break;
	}
	case WM_PAINT: {
		WaitForSingleObject(hMutex, INFINITE);
		hdc = BeginPaint(hWnd, &ps);
		GetClientRect(hWnd, &rect);
		if (memDC == NULL) {

			memDC = CreateCompatibleDC(hdc);
			for (int i = 0; i < NUM_BMP_FILES; i++) {
				hBmp[i] = CreateCompatibleBitmap(hdc, rect.right, rect.bottom);
				SelectObject(memDC, hBmp[i]);
				DeleteObject(hBmp[i]);
			}
			FillRect(memDC, &rect, CreateSolidBrush(RGB(0, 0, 0)));
		}
		else {
			BitBlt(hdc, 0, 0, rect.right, rect.bottom, memDC, 0, 0, SRCCOPY);
			EndPaint(hWnd, &ps);
		}
		ReleaseMutex(hMutex);
	
		break;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_LBUTTONDOWN:
		WaitForSingleObject(pipeData.paintData.hMutex, INFINITE);
		dwXMouse = GET_X_LPARAM(lParam);
		dwYMouse = GET_Y_LPARAM(lParam);
		if (dwXMouse<pipeData.pipeGameData.dwX * 30 + XOffset && dwXMouse >(pipeData.pipeGameData.dwX * 30 + XOffset) - 30) {
			writee(&pipeData, _T('L'));
		}
		else if (dwXMouse<pipeData.pipeGameData.dwX * 30 + XOffset + 60 && dwXMouse >(pipeData.pipeGameData.dwX * 30 + XOffset)+30) {
			writee(&pipeData, _T('R'));
		}
		if (dwYMouse<=pipeData.pipeGameData.dwY * 30 + YOffset && dwYMouse >=(pipeData.pipeGameData.dwY * 30 + YOffset) - 30) {
			writee(&pipeData, _T('U'));
		}
		else if (dwYMouse<=pipeData.pipeGameData.dwY * 30 + YOffset + 60&& dwYMouse >=(pipeData.pipeGameData.dwY * 30 + YOffset)+30) {
			writee(&pipeData, _T('D'));
		}
		ReleaseMutex(pipeData.paintData.hMutex);
		break;
	case WM_RBUTTONDOWN:
		WaitForSingleObject(pipeData.paintData.hMutex, INFINITE);
		dwXMouse = GET_X_LPARAM(lParam);
		dwYMouse = GET_Y_LPARAM(lParam);
		if (dwXMouse <pipeData.pipeGameData.dwX * 30 + XOffset + 30 && dwXMouse>pipeData.pipeGameData.dwX * 30 + XOffset &&
			dwYMouse< pipeData.pipeGameData.dwY * 30 + YOffset + 30 && dwYMouse>pipeData.pipeGameData.dwY * 30 + YOffset
			) {
			writee(&pipeData, _T('B'));//B = volta para posicao inicial
		}
		ReleaseMutex(pipeData.paintData.hMutex);
		break;
	case WM_SIZE:
		WaitForSingleObject(hMutex, INFINITE);
		XOffset = (LOWORD(lParam) / 2) - ((MAX_WIDTH * 30) / 2);
		YOffset = (HIWORD(lParam) / 2) - (((MAX_ROADS + 4) * 30) / 2);
		memDC = NULL;
		ReleaseMutex(hMutex);

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
		writee(&pipeData, _T('Q'));
		CloseHandle(pipeData.hPipe);
		PostQuitMessage(0);
		break;
	case WM_CLOSE:
		WaitForSingleObject(pipeData.paintData.hMutex, INFINITE);
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
	PIPE_GAME_DATA pipeGameData;
	DWORD n;
	RECT rect;
	while (!pipeData->dwShutDown) {
		if (GetOverlappedResult(pipeData->hPipe, &pipeData->overlapRead, &n, TRUE)) {	
			if (n > 0) {
				if (WaitForSingleObject(pipeData->paintData.hMutex, INFINITE) == WAIT_OBJECT_0) {
					if (*pipeData->paintData.memDC != NULL)
					{
						pipeData->pipeGameData = pipeGameData;
						SHARED_BOARD sharedBoard = pipeGameData.sharedBoard;
						GetClientRect(pipeData->paintData.hWnd, &rect);
						FillRect(*pipeData->paintData.memDC, &rect, CreateSolidBrush(RGB(0, 0, 0)));
						for (int i = 0; i < sharedBoard.dwHeight; i++) {
							for (int j = 0; j < sharedBoard.dwWidth; j++) {
								if (sharedBoard.board[i][j] == _T('<')) {
									BitBlt(*pipeData->paintData.memDC,
										*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[0].bmWidth,
										*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[0].bmHeight,
										pipeData->paintData.bmp[0].bmWidth,
										pipeData->paintData.bmp[0].bmHeight,
										pipeData->paintData.bmpDC[0],
										0, 0, SRCCOPY);
								}
								else if (sharedBoard.board[i][j] == _T('>')) {
									BitBlt(*pipeData->paintData.memDC,
										*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[1].bmWidth,
										*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[1].bmHeight,
										pipeData->paintData.bmp[1].bmWidth,
										pipeData->paintData.bmp[1].bmHeight,
										pipeData->paintData.bmpDC[1],
										0, 0, SRCCOPY);
								}
								else if (sharedBoard.board[i][j] == _T('F')&&pipeGameData.dwX==j&&pipeGameData.dwY==i) {
									if (pipeData->sentido == _T('R')) {
										BitBlt(*pipeData->paintData.memDC,
											*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[2].bmWidth,
											*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[2].bmHeight,
											pipeData->paintData.bmp[2].bmWidth,
											pipeData->paintData.bmp[2].bmHeight,
											pipeData->paintData.bmpDC[2],
											0, 0, SRCCOPY);
									}
									else if (pipeData->sentido == _T('D')) {
										BitBlt(*pipeData->paintData.memDC,
											*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[3].bmWidth,
											*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[3].bmHeight,
											pipeData->paintData.bmp[3].bmWidth,
											pipeData->paintData.bmp[3].bmHeight,
											pipeData->paintData.bmpDC[3],
											0, 0, SRCCOPY);
									}
									else  if (pipeData->sentido == _T('L')) {
										BitBlt(*pipeData->paintData.memDC,
											*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[4].bmWidth,
											*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[4].bmHeight,
											pipeData->paintData.bmp[4].bmWidth,
											pipeData->paintData.bmp[4].bmHeight,
											pipeData->paintData.bmpDC[4],
											0, 0, SRCCOPY);
									}
									else {
										BitBlt(*pipeData->paintData.memDC,
											*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[5].bmWidth,
											*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[5].bmHeight,
											pipeData->paintData.bmp[5].bmWidth,
											pipeData->paintData.bmp[5].bmHeight,
											pipeData->paintData.bmpDC[5],
											0, 0, SRCCOPY);
									}
								}
								else if (sharedBoard.board[i][j] == _T('F')) {
									BitBlt(*pipeData->paintData.memDC,
										*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[5].bmWidth,
										*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[5].bmHeight,
										pipeData->paintData.bmp[5].bmWidth,
										pipeData->paintData.bmp[5].bmHeight,
										pipeData->paintData.bmpDC[5],
										0, 0, SRCCOPY);
								}
								else if (sharedBoard.board[i][j] == _T('-')) {
									BitBlt(*pipeData->paintData.memDC,
										*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[6].bmWidth,
										*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[6].bmHeight,
										pipeData->paintData.bmp[6].bmWidth,
										pipeData->paintData.bmp[6].bmHeight,
										pipeData->paintData.bmpDC[6],
										0, 0, SRCCOPY);
								}
								else if (sharedBoard.board[i][j] == _T('X')) {
									BitBlt(*pipeData->paintData.memDC,
										*pipeData->paintData.XOffset + j * pipeData->paintData.bmp[7].bmWidth,
										*pipeData->paintData.YOffset + i * pipeData->paintData.bmp[7].bmHeight,
										pipeData->paintData.bmp[7].bmWidth,
										pipeData->paintData.bmp[7].bmHeight,
										pipeData->paintData.bmpDC[7],
										0, 0, SRCCOPY);
								}
							}
						}
					}
					ReleaseMutex(pipeData->paintData.hMutex);
					InvalidateRect(pipeData->paintData.hWnd, NULL, TRUE);
				}
			}
			ZeroMemory(&pipeGameData, sizeof(pipeGameData));
			ReadFile(pipeData->hPipe, &pipeGameData, sizeof(pipeGameData), &n, &pipeData->overlapRead);
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
	WaitForSingleObject(pipeData->paintData.hMutex, INFINITE);
	if (!WriteFile(pipeData->hPipe, &c, sizeof(c), &n, &pipeData->overlapWrite)) {
		return 1;
	}
	if (c != _T('P')) {
		pipeData->sentido = c;
	}
	ReleaseMutex(pipeData->paintData.hMutex);
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