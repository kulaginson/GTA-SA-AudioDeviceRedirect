#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#include <fstream>
#include <string>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "ole32.lib")

static HMODULE g_module = nullptr;

static std::string GetDllDirectory()
{
    char path[MAX_PATH] = {};

    DWORD len = GetModuleFileNameA(
        g_module,
        path,
        MAX_PATH
    );

    if (len == 0 || len >= MAX_PATH)
        return ".\\";

    std::string result(path, len);

    size_t slash = result.find_last_of("\\/");

    if (slash == std::string::npos)
        return ".\\";

    return result.substr(0, slash + 1);
}


static void Log(const std::string& text)
{
    std::string filename =
        GetDllDirectory() +
        "AudioDeviceRedirect.log";

    std::ofstream file(
        filename,
        std::ios::app
    );

    if (!file)
        return;

    SYSTEMTIME time;
    GetLocalTime(&time);

    file << "["
         << std::setfill('0')
         << std::setw(2) << time.wHour << ":"
         << std::setw(2) << time.wMinute << ":"
         << std::setw(2) << time.wSecond << "."
         << std::setw(3) << time.wMilliseconds
         << "] "
         << text
         << std::endl;
}


static std::string WideToUtf8(const std::wstring& text)
{
    if (text.empty())
        return {};

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0)
        return {};

    std::string result(size, '\0');

    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.c_str(),
        static_cast<int>(text.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );

    return result;
}


static void LogCurrentAudioDevice()
{
    HRESULT hr = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED
    );

    if (FAILED(hr))
    {
        Log("CoInitializeEx failed.");
        return;
    }


    IMMDeviceEnumerator* enumerator = nullptr;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );

    if (FAILED(hr))
    {
        Log("CoCreateInstance(MMDeviceEnumerator) failed.");
        CoUninitialize();
        return;
    }


    IMMDevice* device = nullptr;

    hr = enumerator->GetDefaultAudioEndpoint(
        eRender,
        eMultimedia,
        &device
    );

    if (FAILED(hr))
    {
        Log("GetDefaultAudioEndpoint failed.");
        enumerator->Release();
        CoUninitialize();
        return;
    }


    LPWSTR deviceId = nullptr;

    hr = device->GetId(&deviceId);

    if (SUCCEEDED(hr) && deviceId)
    {
        Log(
            "Default render device ID: " +
            WideToUtf8(deviceId)
        );

        CoTaskMemFree(deviceId);
    }
    else
    {
        Log("IMMDevice::GetId failed.");
    }


    IPropertyStore* properties = nullptr;

    hr = device->OpenPropertyStore(
        STGM_READ,
        &properties
    );

    if (SUCCEEDED(hr))
    {
        PROPVARIANT value;

        PropVariantInit(&value);

        hr = properties->GetValue(
            PKEY_Device_FriendlyName,
            &value
        );

        if (SUCCEEDED(hr) &&
            value.vt == VT_LPWSTR &&
            value.pwsz)
        {
            Log(
                "Default render device name: " +
                WideToUtf8(value.pwsz)
            );
        }

        PropVariantClear(&value);

        properties->Release();
    }


    device->Release();
    enumerator->Release();

    CoUninitialize();
}


static DWORD WINAPI DiagnosticThread(LPVOID)
{
    Log("========================================");
    Log("AudioDeviceRedirect diagnostic build");
    Log("Plugin loaded successfully.");
    Log("Process ID: " +
        std::to_string(GetCurrentProcessId()));

    Log("Waiting for GTA initialization...");

    Sleep(3000);

    Log("Checking current Windows audio device...");

    LogCurrentAudioDevice();

    Log("Starting device monitoring.");


    std::string lastDevice;


    while (true)
    {
        // We intentionally DO NOT modify GTA audio.
        // We only read the Windows default device.

        HRESULT hr = CoInitializeEx(
            nullptr,
            COINIT_MULTITHREADED
        );

        if (SUCCEEDED(hr))
        {
            IMMDeviceEnumerator* enumerator = nullptr;

            hr = CoCreateInstance(
                __uuidof(MMDeviceEnumerator),
                nullptr,
                CLSCTX_ALL,
                __uuidof(IMMDeviceEnumerator),
                reinterpret_cast<void**>(&enumerator)
            );

            if (SUCCEEDED(hr))
            {
                IMMDevice* device = nullptr;

                hr = enumerator->GetDefaultAudioEndpoint(
                    eRender,
                    eMultimedia,
                    &device
                );

                if (SUCCEEDED(hr))
                {
                    LPWSTR deviceId = nullptr;

                    hr = device->GetId(&deviceId);

                    if (SUCCEEDED(hr) && deviceId)
                    {
                        std::string current =
                            WideToUtf8(deviceId);

                        if (current != lastDevice)
                        {
                            Log(
                                "Windows default audio device changed: " +
                                current
                            );

                            lastDevice = current;

                            CoTaskMemFree(deviceId);

                            device->Release();
                            enumerator->Release();

                            CoUninitialize();

                            Sleep(500);

                            continue;
                        }

                        CoTaskMemFree(deviceId);
                    }

                    device->Release();
                }

                enumerator->Release();
            }

            CoUninitialize();
        }

        Sleep(500);
    }

    return 0;
}


BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID reserved
)
{
    UNREFERENCED_PARAMETER(reserved);

    if (reason == DLL_PROCESS_ATTACH)
    {
        g_module = hModule;

        DisableThreadLibraryCalls(hModule);

        // Do not perform any audio manipulation here.
        // Only start the diagnostic thread.

        HANDLE thread = CreateThread(
            nullptr,
            0,
            DiagnosticThread,
            nullptr,
            0,
            nullptr
        );

        if (thread)
        {
            CloseHandle(thread);
        }
    }

    return TRUE;
}
