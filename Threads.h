#pragma once
#include "ImgUI/imgui.h"
#include "ImgUI/imgui_impl_win32.h"
#include "ImgUI/imgui_impl_dx11.h"
#include <d3d11.h>
#include <Windows.h>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <chrono>

//Thread übergreifende Variablen

//General stuff
std::atomic<bool> autoClickEnabled = false;
std::atomic<int>  clicksPerSecond = 10;
std::atomic<int>  mouseButton = 0; // 0 = Links, 1 = Rechts
std::atomic<bool> appRunning = true; // Zum sauberen Beenden
std::atomic<bool> recording = false;
std::atomic<bool> playing = false;

//Keys
bool Fkeysdown[3] = { false, false, false}; //{F6, F9, F10}
std::mutex FkeyMutex;
std::unordered_map<USHORT, bool> keyStates;
std::mutex keyStatesMutex;

//Maus
struct MouseMicky {
	std::chrono::high_resolution_clock::time_point timestamp;
	LONG dx;
	LONG dy;
};
std::vector<MouseMicky> RecordedMouseMovements;
std::mutex mouseMovementsMutex;
std::atomic<bool> mouseLeftDown = false;
std::atomic<bool> mouseRightDown = false;
std::atomic<bool> mouseMiddleDown = false;

// ───── Klick-Thread ─────
void ClickerThread() {
	while (appRunning) {

		if (autoClickEnabled) {
			int cps = clicksPerSecond.load();
			if (cps == 0) { cps = 1; }
			int delayMs = 1000 / cps;

			if (mouseButton.load() == 0) {
				// Linke Maustaste
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
			}
			else {
				// Rechte Maustaste
				mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
				mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
			}

			Sleep(delayMs);
		}
		else {
			// Nicht aktiv – länger schlafen um CPU zu schonen
			Sleep(100);
		}
	}
}
// ────────────────────────


//Window for Inputs and recording
LRESULT CALLBACK RawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_INPUT) {
		UINT dwSize = 0;
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

		BYTE* lpb = new BYTE[dwSize];
		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize) {

			RAWINPUT* raw = (RAWINPUT*)lpb;

			// Keyboard
			if (raw->header.dwType == RIM_TYPEKEYBOARD) {
				USHORT key = raw->data.keyboard.VKey;
				bool isDown = !(raw->data.keyboard.Flags & RI_KEY_BREAK);

				if (key == VK_F6) {
					std::lock_guard<std::mutex> lock(FkeyMutex);
					Fkeysdown[0] = isDown;
				}
				else if (key == VK_F9) {
					std::lock_guard<std::mutex> lock(FkeyMutex);
					Fkeysdown[1] = isDown;
				}
				else if (key == VK_F10) {
					std::lock_guard<std::mutex> lock(FkeyMutex);
					Fkeysdown[2] = isDown;
				}//its ugly but it works and I am a genius, so who cares <- AI said that
					
				keyStatesMutex.lock();
				keyStates[key] = isDown;
				keyStatesMutex.unlock();
			}

			// Mouse
			if (raw->header.dwType == RIM_TYPEMOUSE) {
				USHORT flags = raw->data.mouse.usButtonFlags;

				if (recording && (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0)) {

					std::lock_guard<std::mutex> lock(mouseMovementsMutex);
					RecordedMouseMovements.push_back({
						std::chrono::high_resolution_clock::now(),
						raw->data.mouse.lLastX,
						raw->data.mouse.lLastY
					});
				}

				if (flags & RI_MOUSE_LEFT_BUTTON_DOWN)  mouseLeftDown = true;
				if (flags & RI_MOUSE_LEFT_BUTTON_UP)    mouseLeftDown = false;

				if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) mouseRightDown = true;
				if (flags & RI_MOUSE_RIGHT_BUTTON_UP)   mouseRightDown = false;

				if (flags & RI_MOUSE_MIDDLE_BUTTON_DOWN) mouseMiddleDown = true;
				if (flags & RI_MOUSE_MIDDLE_BUTTON_UP)   mouseMiddleDown = false;

			}
		}
		delete[] lpb;
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

//Input and recording Thread 
void RawInputThread() {
	WNDCLASS wc = {};
	wc.lpfnWndProc = RawInputWndProc;
	wc.lpszClassName = L"RawInputWindow";

	RegisterClass(&wc);

	HWND hwnd = CreateWindowEx(
		0,
		wc.lpszClassName,
		L"",
		0,
		0, 0, 0, 0,
		HWND_MESSAGE, // unsichtbares Message-Window
		nullptr,
		nullptr,
		nullptr
	);

	// Geräte registrieren
	RAWINPUTDEVICE rid[2];

	// Tastatur
	rid[0].usUsagePage = 0x01;
	rid[0].usUsage = 0x06;
	rid[0].dwFlags = RIDEV_INPUTSINK;
	rid[0].hwndTarget = hwnd;

	// Maus
	rid[1].usUsagePage = 0x01;
	rid[1].usUsage = 0x02;
	rid[1].dwFlags = RIDEV_INPUTSINK;
	rid[1].hwndTarget = hwnd;

	RegisterRawInputDevices(rid, 2, sizeof(rid[0]));

	// Message Loop
	MSG msg;
	while (appRunning.load()) {
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				return; }

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(1);
	}
}

//f6 toggle stuff
bool lastFKeysstate[3] = {false, false, false};
bool FKeypressed(int Key) {
	Key = int((1.0/6.0) * Key*Key - (13.0/6.0) * Key + 7); //making it harder to understand for my future self, because I am a genius and don't want to write 3 separate functions for F6, F9 and F10 <- AI said that
	FkeyMutex.lock();
	bool FKeycurrent = Fkeysdown[Key];
	FkeyMutex.unlock();
	if (FKeycurrent && !lastFKeysstate[Key]) {
		lastFKeysstate[Key] = FKeycurrent;
		return true;
	}
	else {
		lastFKeysstate[Key] = FKeycurrent;
		return false;
	}
}

void ReplayThread() {
	while (appRunning) {
		while (playing.load()) {
			std::vector<MouseMicky> copiedMoves;
			{
				std::lock_guard<std::mutex> lock(mouseMovementsMutex);
				copiedMoves = RecordedMouseMovements;
			}
			if(copiedMoves.empty()) {
				playing = false;
				break;
			}
			auto RecordingStartTime = copiedMoves[0].timestamp;
			auto ReplayStartTime = std::chrono::high_resolution_clock::now();
			for (int i = 0; (i < copiedMoves.size()) && playing.load(); i++) {

				while ((copiedMoves[i].timestamp - RecordingStartTime) > (std::chrono::high_resolution_clock::now() - ReplayStartTime)) {
					if (!playing.load()) { break; }
				}
				INPUT input = {};
				input.type = INPUT_MOUSE;
				input.mi.dwFlags = MOUSEEVENTF_MOVE;
				input.mi.dx = copiedMoves[i].dx;
				input.mi.dy = copiedMoves[i].dy;
				SendInput(1, &input, sizeof(INPUT));
			}
			playing = false; // Temporär Nach einmaligem Abspielen stoppen
		}
		Sleep(100);
	}
}

void NewRecording(){
	std::lock_guard<std::mutex> lock(mouseMovementsMutex);
	RecordedMouseMovements.clear();
}