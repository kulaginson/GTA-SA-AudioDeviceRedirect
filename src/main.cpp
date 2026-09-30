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


// ============================================================
// Windows Audio Policy Config
//
// Windows 10/11 uses this internal interface for the
// per-application output device selector.
//
// This is an undocumented interface.
// The 21H2 interface contains 19 methods before
// SetPersistedDefaultAudioEndpoint.
// ============================================================

struct __declspec(uuid("ab3d4648-e242-459f-b02f-541c70306324"))
IAudioPolicyConfigFactory : IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Unknown01() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown02() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown03() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown04() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown05() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown06() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown07() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown08() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unknown09() = 0;
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
        HSTRING deviceId
    ) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetPersistedDefaultAudioEndpoint(
        UINT32 processId,
        int flow,
        int role,
        HSTRING* deviceId
    ) = 0;

    virtual HRESULT STDMETHODCALLTYPE ClearAllPersistedApplicationDefaultEndpoints() = 0;
};


// ============================================================
// Globals
// ============================================================

static std::atomic<bool> g_running{ true };

static std::wstring g_lastDevice;


// ============================================================
// Get current Windows default playback device
// ============================================================

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


// ============================================================
// Set GTA process audio output
// ============================================================

static bool RouteGTAAudio(const std::wstring& deviceId)
{
    if (deviceId.empty())
        return false;


    // --------------------------------------------------------
    // Create WinRT class name HSTRING
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // Get IAudioPolicyConfigFactory
    // --------------------------------------------------------

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


    // --------------------------------------------------------
    // IMPORTANT:
    //
    // SetPersistedDefaultAudioEndpoint expects HSTRING,
    // NOT LPCWSTR.
    // --------------------------------------------------------

    HSTRING deviceHString = nullptr;

    hr = WindowsCreateString(
        deviceId.c_str(),
        static_cast<UINT32>(deviceId.size()),
        &deviceHString
    );

    if (FAILED(hr))
        return false;


    const UINT32 processId =
        GetCurrentProcessId();


    // --------------------------------------------------------
    // eRender = 0
    //
    // eConsole        = 0
    // eMultimedia     = 1
    // eCommunications = 2
    // --------------------------------------------------------

    bool success = true;


    hr = factory->SetPersistedDefaultAudioEndpoint(
        processId,
        0,
        0,
        deviceHString
    );

    if (FAILED(hr))
        success = false;


    if (success)
    {
        hr = factory->SetPersistedDefaultAudioEndpoint(
            processId,
            0,
            1,
            deviceHString
        );

        if (FAILED(hr))
            success = false;
    }


    if (success)
    {
        hr = factory->SetPersistedDefaultAudioEndpoint(
            processId,
            0,
            2,
            deviceHString
        );

        if (FAILED(hr))
            success = false;
    }


    WindowsDeleteString(deviceHString);

    return success;
}


// ============================================================
// Audio monitoring thread
// ============================================================

static DWORD WINAPI AudioThread(LPVOID)
{
    // Initialize COM/WinRT for this thread.
    HRESULT hr = RoInitialize(
        RO_INIT_MULTITHREADED
    );

    if (FAILED(hr))
        return 0;


    // Give GTA time to finish initialization.
    Sleep(5000);


    while (g_running)
    {
        std::wstring currentDevice =
            GetDefaultRenderDevice();


        if (!currentDevice.empty())
        {
            // Device changed?
            if (currentDevice != g_lastDevice)
            {
                if (RouteGTAAudio(currentDevice))
                {
                    g_lastDevice = currentDevice;
                }
            }
        }


        // Check every 500 ms.
        Sleep(500);
    }


    RoUninitialize();

    return 0;
}


// ============================================================
// DLL entry point
// ============================================================

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID reserved
)
{
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


    if (reason == DLL_PROCESS_DETACH)
    {
        g_running = false;
    }


    return TRUE;
}
