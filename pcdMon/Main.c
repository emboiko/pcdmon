#include <Windows.h>
#include <Pdh.h>
#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#define FPS 60
#define TARGET_FRAMETIME (1000 / FPS)
#define CHART_STEP 5

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
int running = FALSE;
int lastFrameTime = 0;
int windowWidth = 1000;
int windowHeight = 1000;
int pollingInterval = 1000;
float xProjectionOffset = 0.5;
float yProjectionOffset = 0.75;
float blitScaleFactor = 1.0f;
double blit = 0;
double maxBlit = 0;
long currentCounterData = 0;
long maxCounterData = 0;
long* blits = NULL;
char* counterPath = NULL;
int terminalColumns = 0;
int terminalRows = 0;
HANDLE hStdOut = NULL;
HQUERY hQuery = NULL;
HCOUNTER hCounter = NULL;
PDH_FMT_COUNTERVALUE pValue;
COORD origin = {.X=0, .Y=0};
CONSOLE_SCREEN_BUFFER_INFO csbi;


void hideCursor() {
	CONSOLE_CURSOR_INFO info = {.dwSize=100, .bVisible=FALSE};
	SetConsoleCursorInfo(hStdOut, &info);
}


int initPerfCounter(char* cPath) {
	PDH_STATUS pdhStatus;

	// Get a handle to a new query object
	pdhStatus = PdhOpenQuery(NULL, 0, &hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhOpenQuery failed: 0x%x\n", pdhStatus);
		return FALSE;
	}

	// Add the counter [path] to the query object
	pdhStatus = PdhAddCounterA(
		hQuery,
		cPath,
		0,
		&hCounter
	);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhAddCounterW failed: 0x%x\n", pdhStatus);
		return FALSE;
	}

	counterPath = cPath;
	return TRUE;
}


long pollPerfCounter(void) {
	PDH_STATUS pdhStatus;

	// Poll for the query data
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhCollectQueryData failed: 0x%x\n", pdhStatus);
		return -9999;
	}

	Sleep(pollingInterval);

	// Poll again for the delta so we have something to calculate
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhCollectQueryData failed: 0x%x\n", pdhStatus);
		return-9999;
	}

	// For time-based rate counters, it makes sense to get the formatted 
	// value, rather than going directly with PdhGetRawCounterValue() and
	// a PDH_RAW_COUNTER struct.
	pdhStatus = PdhGetFormattedCounterValue(
		hCounter,
		PDH_FMT_LONG,
		NULL,
		&pValue
	);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhGetFormattedCounterValue failed: 0x%x\n", pdhStatus);
		return-9999;
	}

	return (long)pValue.longValue;
}


void poll(void) {
	while (running) { 
		currentCounterData = pollPerfCounter(); 
		if (currentCounterData > maxCounterData) maxCounterData = currentCounterData;
		
		SetConsoleCursorPosition(hStdOut, origin);
		GetConsoleScreenBufferInfo(hStdOut, &csbi);
		int currentTerminalRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		int currentTerminalColumns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
		if (
			(currentTerminalColumns != terminalColumns) || 
			(currentTerminalRows != terminalRows)
			) {

			for (int i = 0; i < currentTerminalRows; i++) {
				for (int j = 0; j < currentTerminalColumns; j++) {
					printf(" ");
				}
				printf("\n");
			}
		}
		terminalColumns = currentTerminalColumns;
		terminalRows = currentTerminalRows;
		SetConsoleCursorPosition(hStdOut, origin);

		if (terminalRows > 11) {
			printf("                _                   \n");
			printf("               | |                  \n");
			printf(" ____   ____ __| |____   ___  ____  \n");
			printf("|  _ \\ / ___) _  |    \\ / _ \\|  _ \\ \n");
			printf("| |_| ( (__( (_| | | | | |_| | | | |\n");
			printf("|  __/ \\____)____|_|_|_|\\___/|_| |_|\n");
			printf("|_|                                 \n\n");
		}
		if (terminalRows > 3) {
			printf("Counter:      %s           \n", counterPath);
			printf("Interval:     %d ms        \n", pollingInterval);
			printf("Peak:         %ld          \n", maxCounterData);
			printf("Current:      %ld            ", currentCounterData);
		}
	}
}


void closeCounter(void) {
	PDH_STATUS pdhStatus = PdhCloseQuery(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"Error releasing query resources\n");
	}
}


int createPollThread(void) {
	HANDLE hThread = CreateThread(
		NULL,	// default security descriptor
		0,		// default stack size
		poll,	// entry point
		NULL,	// application defined lparam passed to the thread
		0,		// run immediately
		NULL	// receieve no thread ID
	);
	if (hThread == NULL) return FALSE;
	return TRUE;
}


void shiftBlitBuffer(long inData) {
	for (int i = 0; i < windowWidth-1; i++) {
		blits[i] = blits[i + 1];
	}
	blits[windowWidth - 1] = inData;
}


long scaleBetween(long num, long minScale, long maxScale, long min, long max) {
	if (!num || !max) return 0;
	return (maxScale - minScale) * (num - min) / (max - min) + minScale;
}


void exitHandler(sig) {
	running = FALSE;
}


void destroyWindow(void) {
	if (blits != NULL) free(blits);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}


int initWindow(void) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		fprintf(stderr, "Error initializing SDL.\n");
		return FALSE;
	}

	window = SDL_CreateWindow(
		"pcdMon",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		windowWidth,
		windowHeight,
		SDL_WINDOW_RESIZABLE
	);

	if (!window) {
		fprintf(stderr, "Error creating SDL window.\n");
		return FALSE;
	}

	renderer = SDL_CreateRenderer(
		window,
		-1, // Default rendering driver
		0 // No flags
	);

	if (!renderer) {
		fprintf(stderr, "Error creating SDL renderer.\n");
		return FALSE;
	}

	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

	return TRUE;
}


int setup(void) {
	if (!createPollThread()) return FALSE;

	(long*)blits = (long*)calloc(windowWidth, sizeof(long));
	if (blits == NULL) return FALSE;

	hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD prev_mode;
	GetConsoleMode(hStdOut, &prev_mode);
	SetConsoleMode(
		hStdOut, 
		ENABLE_EXTENDED_FLAGS | (prev_mode &= ~ENABLE_QUICK_EDIT_MODE)
	);
	hideCursor();

	GetConsoleScreenBufferInfo(hStdOut, &csbi);
	terminalColumns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	terminalRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

	SetConsoleCursorPosition(hStdOut, origin);
	for (int i = 0; i < terminalRows; i++) {
		for (int j = 0; j < terminalColumns; j++) {
			printf(" ");
		}
		printf("\n");
	}

	signal(SIGINT, exitHandler);
	signal(SIGBREAK, exitHandler);

	return TRUE;
}


void processInput(void) {
	SDL_Event event;
	SDL_PollEvent(&event);

	switch (event.type) {
	case SDL_QUIT:
		running = FALSE;
		break;
	
	case SDL_KEYDOWN:
		if (event.key.keysym.sym == SDLK_ESCAPE) running = FALSE;

		if (event.key.keysym.sym == SDLK_F1) {
			SDL_SetWindowBordered(window, SDL_TRUE);
		}
		
		if (event.key.keysym.sym == SDLK_F2) {
			SDL_SetWindowBordered(window, SDL_FALSE);
		}

		if (event.key.keysym.sym == SDLK_w) {
			yProjectionOffset -= .025;
		}

		if (event.key.keysym.sym == SDLK_a) {
			xProjectionOffset -= .025;
		}

		if (event.key.keysym.sym == SDLK_s) {
			yProjectionOffset += .025;
		}

		if (event.key.keysym.sym == SDLK_d) {
			xProjectionOffset += .025;
		}

		if (event.key.keysym.sym == SDLK_r) {
			xProjectionOffset = 0.5;
			yProjectionOffset = 0.75;
		}

		if (event.key.keysym.sym == SDLK_UP) {
if (pollingInterval > 10000) break;
pollingInterval += 100;
		}

		if (event.key.keysym.sym == SDLK_DOWN) {
			if (pollingInterval < 100) break;
			pollingInterval -= 100;
		}

		break;

	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
			int lastWindowWidth = windowWidth;
			SDL_GetWindowSize(window, &windowWidth, &windowHeight);
			if (blits != NULL) {
				long* blitsBuff;
				(long*)blitsBuff = (long*)calloc(lastWindowWidth, sizeof(long));
				if (blitsBuff != NULL) {
					for (int i = 0; i < lastWindowWidth; i++) {
						blitsBuff[i] = blits[i];
					}

					free(blits);
					(long*)blits = (long*)calloc(windowWidth, sizeof(long));

					if (blits != NULL) {
						for (int i = 0; i < lastWindowWidth; i++) {
							if (i >= windowWidth) break;
							blits[i] = blitsBuff[i];
						}

						free(blitsBuff);
					}
				}
			}
			else {
				fwprintf(stderr, L"malloc returned nullptr\n");
				running = FALSE;
			}
		}

		break;
	}
}


void update(void) {
	// Lock execution until we reach target frametime:
	//while (
	//	!SDL_TICKS_PASSED(SDL_GetTicks(), lastFrameTime + TARGET_FRAMETIME)
	//);

	// Without spinlock (releasing execution):
	int wait = TARGET_FRAMETIME - (SDL_GetTicks() - lastFrameTime);
	if ((wait > 0) && (wait <= TARGET_FRAMETIME)) SDL_Delay(wait);

	// Ticks from last frame converted to seconds:
	// Objects should update as a function of pixels per second,
	// rather than a function of pixels per frame. 
	double deltaTime = (SDL_GetTicks() - lastFrameTime) / 1000.0f;
	lastFrameTime = SDL_GetTicks(); // Ticks since SDL_Init()

	///////////////////////////////////////////////////////////////

	// Scaling up
	for (int i = 0; i < windowWidth; i++) {
		if ((blits[i] * blitScaleFactor) > windowHeight) {
			blitScaleFactor *= 0.75;
			break;
		}
	}

	// Scaling down
	float blitThreshold = windowHeight * 0.9;
	int blitThresholdMet = FALSE;
	for (int i = 0; i < windowWidth; i++) {
		if ((blits[i] * blitScaleFactor) > blitThreshold) {
			blitThresholdMet = TRUE;
			break;
		}
	}
	if (!blitThresholdMet && blitScaleFactor <= 1) {
		blitScaleFactor *= 1.25;
	}

	// Blit towards the current counter data
	double difference;
	if (blit > currentCounterData) {
		difference = blit - currentCounterData;
		blit -= difference * CHART_STEP * deltaTime;
		shiftBlitBuffer(blit);
	}
	else if (blit < currentCounterData) {
		difference = currentCounterData - blit;
		blit += difference * CHART_STEP * deltaTime;
		shiftBlitBuffer(blit);
	}

	// Get the current max blit to scale the blit's shade in draw()
	long max = 0;
	for (int i = 0; i < windowWidth; i++) {
		if (blits[i] > max) max = blits[i];
	}
	maxBlit = max;
}


void draw(void) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	long shade;

	 // Draw all shadows first so they're overlapped by the bulk shading
	for (int i = 0; i < windowWidth - 1; i++) {
		//Trace the shadow
		shade = scaleBetween(blits[i], 25, 200, 0, maxBlit);
		SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
		SDL_RenderDrawLine(
			renderer,
			i,
			windowHeight - (blits[i] * blitScaleFactor),
			windowWidth * xProjectionOffset,
			windowHeight * yProjectionOffset
		);
	}

	for (int i = 0; i < windowWidth-1; i++) {
		// Shade in the bulk
		shade = scaleBetween(blits[i], 0, 200, 0, maxBlit);
		SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
		SDL_RenderDrawLine(
			renderer,
			i,
			windowHeight - (blits[i] * blitScaleFactor),
			i,
			windowHeight
		);

		// Trace the top
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderDrawLine(
			renderer,
			i,
			windowHeight - (blits[i] * blitScaleFactor) - 1,
			i,
			windowHeight - (blits[i + 1] * blitScaleFactor) - 1
		);
	}

	SDL_RenderPresent(renderer);
}


int main(int argc, char** argv) {
	if (argc < 2) {
		fwprintf(stderr, L"Missing argument\n");
		fwprintf(stderr, L"Usage: pcdmon {Counter Path}  [Polling Interval]\n");
		return ERROR_INVALID_COMMAND_LINE;
	}
	if (!initPerfCounter(argv[1])) {
		fwprintf(stderr, L"Invalid counter path\n");
		fwprintf(stderr, L"Usage: pcdmon {Counter Path}  [Polling Interval]\n");
		return ERROR_INVALID_COMMAND_LINE;
	}
	if (argc > 2) {
		pollingInterval = atoi(argv[2]);
		if (!pollingInterval || pollingInterval > 10000) {
			fwprintf(stderr, L"Bad polling interval\n");
			fwprintf(stderr, L"Usage: pcdmon {Counter Path}  [Polling Interval]\n");
			return ERROR_INVALID_COMMAND_LINE;
		}
	}

	running = initWindow();

	if (!setup()) {
		fwprintf(stderr, L"Setup failed\n");
		running = FALSE;
	}

	while (running) {
		processInput();
		update();
		draw();
	}
	destroyWindow();
	closeCounter();
	return EXIT_SUCCESS;
}
