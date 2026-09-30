#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <atomic>
#include <string>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "runtimeobject.lib")

using Microsoft::WRL::ComPtr;

// Windows Audio Policy Config
//
// This is an undocumented Windows interface used by several
// per-application audio routing implementations.

struct __declspec(uuid("ab3d4648-e242-459f-b02f-541c70306324"))
IAudioPolicyConfigFactory : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Unknown1() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown2() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown3() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown4() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown5() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown6() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown7() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown8() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown9() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown10() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown11() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown12() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown13() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown14() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown15() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown16() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown17() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown18() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown19() = 0;

    virtual HRESULT STDMETHODCALLTYPE SetPersistedDefaultAudioEndpoint(
        UINT32 processId,
        int flow,
        int role,
        LPCWSTR deviceId
    ) = 0;
};

static std::atomic<bool> g_running{ true };
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

    LPWSTR deviceId = nullptr;

    hr = device->GetId(&deviceId);

    if (FAILED(hr) || deviceId == nullptr)
        return {};

    std::wstring result(deviceId);

    CoTaskMemFree(deviceId);

    return result;
}


static bool RouteGTAAudio(const std::wstring& deviceId)
{
    if (deviceId.empty())
        return false;

    HSTRING className = nullptr;

    const wchar_t* classNameText =
        L"Windows.Media.Internal.AudioPolicyConfig";

    HRESULT hr = WindowsCreateString(
        classNameText,
        static_cast<UINT32>(wcslen(classNameText)),
        &className
    );

    if (FAILED(hr))
        return false;

    ComPtr<IAudioPolicyConfigFactory> factory;

    hr = RoGetActivationFactory(
        className,
        __uuidof(IAudioPolicyConfigFactory),
        reinterpret_cast<void**>(
            factory.GetAddressOf()
        )
    );

    WindowsDeleteString(className);

    if (FAILED(hr))
        return false;

    const UINT32 processId = GetCurrentProcessId();

    // Audio flow:
    // eRender = 0
    //
    // Audio roles:
    // eConsole       = 0
    // eMultimedia    = 1
    // eCommunications = 2

    bool success = false;

    hr = factory->SetPersistedDefaultAudioEndpoint(
        processId,
        0,
        0,
        deviceId.c_str()
    );

    if (SUCCEEDED(hr))
    {
        hr = factory->SetPersistedDefaultAudioEndpoint(
            processId,
            0,
            1,
            deviceId.c_str()
        );
    }

    if (SUCCEEDED(hr))
    {
        hr = factory->SetPersistedDefaultAudioEndpoint(
            processId,
            0,
            2,
            deviceId.c_str()
        );
    }

    if (SUCCEEDED(hr))
        success = true;

    return success;
}


static DWORD WINAPI AudioThread(LPVOID)
{
    HRESULT hr = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED
    );

    if (FAILED(hr))
        return 0;

    // Give GTA SA time to initialize.
    Sleep(5000);

    while (g_running)
    {
        std::wstring currentDevice =
            GetDefaultRenderDevice();

        if (!currentDevice.empty())
        {
            if (currentDevice != g_lastDevice)
            {
                if (RouteGTAAudio(currentDevice))
                {
                    g_lastDevice = currentDevice;
                }
            }
        }

        // Check for device changes every 500 ms.
        Sleep(500);
    }

    CoUninitialize();

    return 0;
}


BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID reserved
)
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

        if (thread != nullptr)
        {
            CloseHandle(thread);
        }
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        g_running = false;
    }

    return TRUE;
}
