#include "ImgUI/imgui.h"
#include "ImgUI/imgui_impl_win32.h"
#include "ImgUI/imgui_impl_dx11.h"
#include "Threads.h"
#include <d3d11.h>
#include <Windows.h>
#include <thread>
#include <atomic>

// DirectX Variablen
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Autoclicker State – atomic damit Thread-sicher




extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) { return true; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // ───── Klick-Thread starten ─────
    std::thread clickThread(ClickerThread);
    std::thread inputThread(RawInputThread);
	std::thread replayThread(ReplayThread);
    // ────────────────────────────────
    OutputDebugStringA("Hallo Output!\n");

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L,
                       GetModuleHandle(nullptr), nullptr, nullptr, nullptr,
                       nullptr, L"AutoClicker", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"AutoClicker",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        100, 100, 800, 600,
        nullptr, nullptr, wc.hInstance, nullptr);

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        nullptr, 0, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, nullptr, &g_pd3dDeviceContext);

    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();

    ShowWindow(hwnd, SW_SHOWDEFAULT);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 22.0f);
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("AutoClicker", nullptr, flags);

        ImGui::Text("Einstellungen");
        ImGui::Separator();

        // Lokale Kopien für ImGui (atomic direkt geht nicht mit ImGui
        int  cps = clicksPerSecond.load();
        int  btn = mouseButton.load();

        if (FKeypressed(6)) {
            autoClickEnabled = !autoClickEnabled.load();
        }
        if (FKeypressed(9)) {
			NewRecording();
            recording = !recording.load();
        }
        if (FKeypressed(10)) {
            playing = !playing.load();
        }//more ugly code yay

        if (ImGui::InputInt("Klicks / Sekunde", &cps)) {
            clicksPerSecond = cps;
        }

        const char* btnLabels[] = { "Links", "Rechts" };
        if (ImGui::Combo("Maustaste", &btn, btnLabels, 2)) {
            mouseButton = btn;
        }

        ImGui::Spacing();

        // Status anzeigen
        

        ImGui::Spacing();
        if (ImGui::Button("Start (F6)", ImVec2(150, 35))) { autoClickEnabled = true; }
        ImGui::SameLine();
        if (ImGui::Button("Stop (F6)", ImVec2(150, 35))) { autoClickEnabled = false; }

        if (autoClickEnabled.load()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "● Klickt"); }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "■ Inaktiv"); }

        ImGui::Spacing();
        if (ImGui::Button("Start Recording (F9)", ImVec2(150, 35))) { recording = true; NewRecording();}
        ImGui::SameLine();
        if (ImGui::Button("Stop Recording (F9)", ImVec2(150, 35))) { recording = false; }
        if (recording.load()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "● Nimmt auf"); }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "■ Inaktiv"); }

        ImGui::Spacing();
        if (ImGui::Button("Start Replay (F10)", ImVec2(150, 35))) { playing = true; }
        ImGui::SameLine();
        if (ImGui::Button("Stop Replay (F10)", ImVec2(150, 35))) { playing = false; }
        if (playing.load()) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "● Spielt ab"); }
        else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "■ Inaktiv"); }

        ImGui::End();
        ImGui::PopStyleVar();

        const float clear[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // ───── Sauber beenden ─────
    appRunning = false;
    recording = false;
	playing = false;
	replayThread.join();
    clickThread.join();
    inputThread.join();
    // ──────────────────────────

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    return 0;
}