#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "runtimeobject.lib")

using Microsoft::WRL::ComPtr;

// Windows internal audio policy interface.
// Used by Windows per-application audio routing.
struct __declspec(uuid("F8679F50-850A-41CF-9C72-430F290290C8"))
IAudioPolicyConfigFactory : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE SetPersistedDefaultAudioEndpoint(
        UINT32 processId,
        int flow,
        int role,
        HSTRING deviceId) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetPersistedDefaultAudioEndpoint(
        UINT32 processId,
        int flow,
        int role,
        HSTRING* deviceId) = 0;

    virtual HRESULT STDMETHODCALLTYPE ClearAllPersistedApplicationDefaultEndpoints() = 0;
};

static std::atomic<bool> g_running = true;
static std::wstring g_lastDevice;

static std::wstring GetDefaultRenderDevice()
{
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&enumerator)
    );

    if (FAILED(hr))
        return {};

    hr = enumerator->GetDefaultAudioEndpoint(
        eRender,
        eMultimedia,
        &device
    );

    if (FAILED(hr))
        return {};

    LPWSTR id = nullptr;

    hr = device->GetId(&id);

    if (FAILED(hr) || !id)
        return {};

    std::wstring result(id);

    CoTaskMemFree(id);

    return result;
}

static bool RouteGTAAudio(const std::wstring& device)
{
    if (device.empty())
        return false;

    HSTRING className = nullptr;
    HSTRING deviceId = nullptr;

    HRESULT hr = WindowsCreateString(
        L"Windows.Media.Internal.AudioPolicyConfig",
        static_cast<UINT32>(
            wcslen(L"Windows.Media.Internal.AudioPolicyConfig")
        ),
        &className
    );

    if (FAILED(hr))
        return false;

    hr = WindowsCreateString(
        device.c_str(),
        static_cast<UINT32>(device.size()),
        &deviceId
    );

    if (FAILED(hr))
    {
        WindowsDeleteString(className);
        return false;
    }

    ComPtr<IAudioPolicyConfigFactory> factory;

    hr = RoGetActivationFactory(
        className,
        __uuidof(IAudioPolicyConfigFactory),
        reinterpret_cast<void**>(factory.GetAddressOf())
    );

    bool success = false;

    if (SUCCEEDED(hr))
    {
        const UINT32 pid = GetCurrentProcessId();

        // eRender = 0
        // eConsole = 0
        // eMultimedia = 1
        // eCommunications = 2

        hr = factory->SetPersistedDefaultAudioEndpoint(
            pid,
            0,
            0,
            deviceId
        );

        if (SUCCEEDED(hr))
        {
            hr = factory->SetPersistedDefaultAudioEndpoint(
                pid,
                0,
                1,
                deviceId
            );
        }

        if (SUCCEEDED(hr))
        {
            hr = factory->SetPersistedDefaultAudioEndpoint(
                pid,
                0,
                2,
                deviceId
            );
        }

        success = SUCCEEDED(hr);
    }

    WindowsDeleteString(deviceId);
    WindowsDeleteString(className);

    return success;
}

static DWORD WINAPI AudioThread(void*)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
        return 0;

    // Give GTA time to initialize.
    Sleep(3000);

    while (g_running)
    {
        std::wstring current = GetDefaultRenderDevice();

        if (!current.empty() && current != g_lastDevice)
        {
            if (RouteGTAAudio(current))
            {
                g_lastDevice = current;
            }
        }

        // Check every 500 ms.
        Sleep(500);
    }

    CoUninitialize();

    return 0;
}

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID reserved)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(reserved);

    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE thread = CreateThread(
            nullptr,
            0,
            AudioThread,
            nullptr,
            0,
            nullptr
        );

        if (thread)
            CloseHandle(thread);
    }

    if (reason == DLL_PROCESS_DETACH)
    {
        g_running = false;
    }

    return TRUE;
}
