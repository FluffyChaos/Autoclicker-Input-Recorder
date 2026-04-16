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
#include <chrono>

//Thread übergreifende Variablen

//General stuff
std::atomic<bool> autoClickEnabled = false;
std::atomic<int>  clicksPerSecond = 10;
std::atomic<int>  mouseButton = 0; // 0 = Links, 1 = Rechts
std::atomic<bool> appRunning = true; // Zum sauberen Beenden
std::atomic<bool> recording = false;

//Keys
std::atomic<bool> keyF6Down = false;
std::atomic<bool> mouseLeftDown = false;
std::atomic<bool> mouseRightDown = false;
std::atomic<bool> mouseMiddleDown = false;
std::unordered_map<USHORT, bool> keyStates;
std::mutex keyStatesMutex;

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
			// Nicht aktiv – kurz schlafen um CPU zu schonen
			Sleep(10);
		}
	}
}
// ────────────────────────


//Setting up the window for input keys or smth
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
					keyF6Down = isDown;
				}
				else {
					keyStatesMutex.lock();
					keyStates[key] = isDown;
					std::string msg = "Taste: " + std::to_string(key) + "\n";
					OutputDebugStringA(msg.c_str());
					if (key == 65) {
						OutputDebugStringA("Thats an A! \n");
					}
					keyStatesMutex.unlock();
				}
			}

			// Mouse
			if (raw->header.dwType == RIM_TYPEMOUSE) {
				USHORT flags = raw->data.mouse.usButtonFlags;

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

//Input Thread 
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
			if (msg.message == WM_QUIT)
				return;

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		Sleep(1);
	}
}

//f6 toggle stuff
bool lastf6state = false;
bool f6pressed() {
	bool f6current = keyF6Down.load();
	if (f6current && !lastf6state) {
		lastf6state = f6current;
		return true;
	}
	else {
		lastf6state = f6current;
		return false;
	}
}

//──── Recording Thread ─────
void RecordingThread() {

	while (recording) {

	}
}