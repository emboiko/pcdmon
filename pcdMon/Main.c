#include <Windows.h>
#include <Pdh.h>
#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>

#define FPS 60
#define TARGET_FRAMETIME (1000 / FPS)

SDL_Renderer* renderer = NULL;
SDL_Window* window = NULL;
int running = FALSE;
int lastFrameTime = 0;
int windowWidth = 1000;
int windowHeight = 1000;
float blitScaleFactor = 1.0f;
double blit = 0;
double maxBlit = 0;
long* blits = NULL;

HQUERY hQuery = NULL;
HCOUNTER hCounter = NULL;
PDH_FMT_COUNTERVALUE pValue;


int initPerfCounter(char* counterPath) {
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
		counterPath,
		0,
		&hCounter
	);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhAddCounterW failed: 0x%x\n", pdhStatus);
		return FALSE;
	}

	return TRUE;
}


void pollPerfCounter(void) {
	PDH_STATUS pdhStatus;

	// Poll for the query data
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhCollectQueryData failed: 0x%x\n", pdhStatus);
		return;
	}

	// counter is in bytes/sec, so poll on a 1 second interval:
	Sleep(1000);

	// Poll again for the delta so we have something to calculate
	pdhStatus = PdhCollectQueryData(hQuery);
	if (pdhStatus != ERROR_SUCCESS) {
		fwprintf(stderr, L"PdhCollectQueryData failed: 0x%x\n", pdhStatus);
		return;
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
		return;
	}
}


void poll(void) {
	while (running) { pollPerfCounter(); }
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

	return TRUE;
}


int setup(void) {
	if (!createPollThread()) return FALSE;

	(long*)blits = (long*)calloc(windowWidth, sizeof(long));
	if (blits == NULL) return FALSE;

	return TRUE;
}


void destroyWindow(void) {
	if (blits != NULL) free(blits);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
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
		
		break;
	
	case SDL_WINDOWEVENT:
		if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
			SDL_GetWindowSize(window, &windowWidth, &windowHeight);
			if (blits != NULL) {
				free(blits);
				(long*)blits = (long*)calloc(windowWidth,sizeof(long));
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

	for (int i = 0; i < windowWidth; i++) {
		if ((blits[i] * blitScaleFactor) > windowHeight) {
			blitScaleFactor *= 0.75;
			break;
		}
	}

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

	long currentCounterData = pValue.longValue;
	double difference;

	if (blit > currentCounterData) {
		difference = blit - currentCounterData;
		blit -= difference * 5 * deltaTime;
		shiftBlitBuffer(blit);
	}
	else if (blit < currentCounterData) {
		difference = currentCounterData - blit;
		blit += difference * 5 * deltaTime;
		shiftBlitBuffer(blit);
	}
	
	long max = 0;
	for (int i = 0; i < windowWidth; i++) {
		if (blits[i] > max) max = blits[i];
	}
	maxBlit = max;
}


void draw(void) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);

	for (int i = 0; i < windowWidth-1; i++) {
		//Shade in the bulk
		long shade = scaleBetween(blits[i], 0, 255, 0, maxBlit);
		SDL_SetRenderDrawColor(renderer, shade, shade, shade, 255);
		SDL_RenderDrawLine(
			renderer, 
			i, 
			windowHeight - (blits[i] * blitScaleFactor), 
			i, 
			windowHeight
		);

		//Trace the top
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
		fwprintf(stderr, L"Usage: pcdmon {Counter Path}\n");
		return ERROR_INVALID_COMMAND_LINE;
	}
	if (!initPerfCounter(argv[1])) {
		fwprintf(stderr, L"Usage: pcdmon {Counter Path}\n");
		return ERROR_INVALID_COMMAND_LINE;
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
