#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <mmdeviceapi.h>

#include <fstream>
#include <string>
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

static std::string GetDefaultDeviceId()
{
    IMMDeviceEnumerator* enumerator = nullptr;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator),
        reinterpret_cast<void**>(&enumerator)
    );

    if (FAILED(hr))
        return {};

    IMMDevice* device = nullptr;

    hr = enumerator->GetDefaultAudioEndpoint(
        eRender,
        eMultimedia,
        &device
    );

    if (FAILED(hr))
    {
        enumerator->Release();
        return {};
    }

    LPWSTR deviceId = nullptr;

    hr = device->GetId(&deviceId);

    std::string result;

    if (SUCCEEDED(hr) && deviceId)
    {
        result = WideToUtf8(deviceId);
        CoTaskMemFree(deviceId);
    }

    device->Release();
    enumerator->Release();

    return result;
}

static DWORD WINAPI DiagnosticThread(LPVOID)
{
    Log("========================================");
    Log("AudioDeviceRedirect diagnostic build");
    Log("Plugin loaded successfully.");
    Log(
        "Process ID: " +
        std::to_string(GetCurrentProcessId())
    );

    Sleep(3000);

    HRESULT hr = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED
    );

    if (FAILED(hr))
    {
        Log("CoInitializeEx failed.");
        return 0;
    }

    Log("Checking Windows default audio device...");

    std::string lastDevice =
        GetDefaultDeviceId();

    if (lastDevice.empty())
    {
        Log("Could not detect default render device.");
    }
    else
    {
        Log(
            "Initial default render device ID: " +
            lastDevice
        );
    }

    Log("Starting device monitoring.");

    while (true)
    {
        std::string currentDevice =
            GetDefaultDeviceId();

        if (!currentDevice.empty() &&
            currentDevice != lastDevice)
        {
            Log(
                "Windows default audio device changed!"
            );

            Log(
                "New device ID: " +
                currentDevice
            );

            lastDevice = currentDevice;
        }

        HMODULE dsound =
            GetModuleHandleW(L"dsound.dll");

        if (dsound)
        {
            static bool loggedDSound = false;

            if (!loggedDSound)
            {
                Log("dsound.dll is loaded in GTA process.");
                loggedDSound = true;
            }
        }

        Sleep(500);
    }

    // Never reached.
    CoUninitialize();

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

        HANDLE thread = CreateThread(
            nullptr,
            0,
            DiagnosticThread,
            nullptr,
            0,
            nullptr
        );

        if (thread)
            CloseHandle(thread);
    }

    return TRUE;
}
