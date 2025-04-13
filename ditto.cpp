#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <memory> // For unique_ptr
#include <system_error> // For std::system_category

// Define if not present (e.g., in older SDKs)
#ifndef LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE
#define LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE 0x00000040
#endif
#ifndef LOAD_LIBRARY_AS_IMAGE_RESOURCE
#define LOAD_LIBRARY_AS_IMAGE_RESOURCE 0x00000020
#endif

// Helper function to get formatted error messages (same as before)
std::wstring GetLastErrorStdWstr(DWORD errorCode = GetLastError()) {
    wchar_t* msgBuf = nullptr;
    DWORD msgLen = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&msgBuf),
        0,
        nullptr);

    if (msgLen == 0) {
        return L"Unknown error (FormatMessage failed with code: " + std::to_wstring(GetLastError()) + L")";
    }
    std::unique_ptr<wchar_t, decltype(&LocalFree)> bufferPtr(msgBuf, LocalFree);
    std::wstring errorString(msgBuf, msgLen);
    while (!errorString.empty() && (errorString.back() == L'\n' || errorString.back() == L'\r')) {
        errorString.pop_back();
    }
    return errorString + L" (Code: " + std::to_wstring(errorCode) + L")";
}

// Context structure (same as before)
struct ResourceCallbackContext {
    HANDLE hTargetFile = nullptr;
    int resourcesCopied = 0;
    int resourcesFailed = 0;
    bool overallSuccess = true;
};

// --- Callback Function Prototypes (same as before) ---
BOOL CALLBACK EnumTypesFunc(HMODULE hModule, LPWSTR lpType, LONG_PTR lParam);
BOOL CALLBACK EnumNamesFunc(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam);
BOOL CALLBACK EnumLangsFunc(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLang, LONG_PTR lParam);

// --- Custom Deleters for RAII Handles (same as before) ---
struct FreeLibraryDeleter {
    using pointer = HMODULE;
    void operator()(HMODULE h) const { if (h) { FreeLibrary(h); } }
};

struct UpdateResourceHandleCloser {
     HANDLE handle = nullptr;
     bool* successFlag = nullptr;
     UpdateResourceHandleCloser(HANDLE h, bool& success) : handle(h), successFlag(&success) {}
     ~UpdateResourceHandleCloser() {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE) {
            if (!EndUpdateResourceW(handle, !(*successFlag))) { // Discard if !overallSuccess
                DWORD errorCode = GetLastError();
                std::wcerr << L"Error: Failed to end resource update: "
                           << GetLastErrorStdWstr(errorCode) << std::endl;
            } else {
                std::wcout << L"Resource update " << (*successFlag ? L"committed." : L"discarded.") << std::endl;
            }
        }
     }
     UpdateResourceHandleCloser(const UpdateResourceHandleCloser&) = delete;
     UpdateResourceHandleCloser& operator=(const UpdateResourceHandleCloser&) = delete;
     UpdateResourceHandleCloser(UpdateResourceHandleCloser&&) = delete;
     UpdateResourceHandleCloser& operator=(UpdateResourceHandleCloser&&) = delete;
     operator HANDLE() const { return handle; }
};


// --- Main Function ---
int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        std::wcerr << L"\nditto - Selective Resource Cloner (Version Info & Icons)\n"
                   << L"--------------------------------------------------------------------\n"
                   << L"Copies Version Information and Icons from a source EXE/DLL\n"
                   << L"to a target EXE/DLL, replacing existing ones if present.\n"
                   << L"Other resource types (dialogs, menus, etc.) in the target remain untouched.\n"
                   << L"Warning: While safer than cloning all resources, modifying binaries\n"
                   << L"         can still potentially affect stability or trigger security software.\n\n"
                   << L"Usage: " << (argc > 0 ? argv[0] : L"ditto_selective.exe") << L" <source_file> <target_file>\n\n"
                   << L"Example: " << (argc > 0 ? argv[0] : L"ditto_selective.exe") << L" source.dll target.dll\n"
                   << std::endl;
        return 1;
    }

    const wchar_t* sourceFile = argv[1];
    const wchar_t* targetFile = argv[2];

    std::wcout << L"Source: " << sourceFile << std::endl;
    std::wcout << L"Target: " << targetFile << std::endl;

    std::unique_ptr<std::remove_pointer<HMODULE>::type, FreeLibraryDeleter> hSourceModule(
        LoadLibraryExW(sourceFile, nullptr, LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));

    if (!hSourceModule) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: Failed to load source file '" << sourceFile << L"': "
                   << GetLastErrorStdWstr(errorCode) << std::endl;
        return 1;
    }
    std::wcout << L"Source file loaded successfully." << std::endl;

    // Begin updating resources in the target file.
    // ***** CHANGE HERE: Use FALSE to *not* delete existing resources *****
    HANDLE hTargetUpdateRaw = BeginUpdateResourceW(targetFile, FALSE);
    if (!hTargetUpdateRaw) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: Failed to begin resource update on target file '" << targetFile << L"': "
                   << GetLastErrorStdWstr(errorCode) << std::endl;
        return 1;
    }
    std::wcout << L"Resource update process started for target file (preserving existing resources)." << std::endl;

    ResourceCallbackContext context;
    context.hTargetFile = hTargetUpdateRaw;
    UpdateResourceHandleCloser updateCloser(hTargetUpdateRaw, context.overallSuccess);

    std::wcout << L"Enumerating and copying Version Info and Icon resources..." << std::endl;
    if (!EnumResourceTypesW(hSourceModule.get(), reinterpret_cast<ENUMRESTYPEPROCW>(EnumTypesFunc), reinterpret_cast<LONG_PTR>(&context))) {
         DWORD errorCode = GetLastError();
        if (errorCode != ERROR_RESOURCE_TYPE_NOT_FOUND) {
             std::wcerr << L"Warning: EnumResourceTypes failed: " << GetLastErrorStdWstr(errorCode) << std::endl;
             context.overallSuccess = false;
        } else {
             std::wcout << L"Source file contains no resources." << std::endl;
        }
    }

    // EndUpdateResource called by updateCloser destructor

    std::wcout << L"\n--- Cloning Summary (Version Info & Icons) ---" << std::endl;
    std::wcout << L"Resources Copied: " << context.resourcesCopied << std::endl;
    std::wcout << L"Resources Failed: " << context.resourcesFailed << std::endl;

    if (!context.overallSuccess) {
         std::wcerr << L"Resource cloning process finished with errors." << std::endl;
         return 1;
    } else {
         std::wcout << L"Resource cloning process finished successfully." << std::endl;
         return 0;
    }
}

// --- Callback Implementations ---

BOOL CALLBACK EnumTypesFunc(HMODULE hModule, LPWSTR lpType, LONG_PTR lParam) {
    ResourceCallbackContext* context = reinterpret_cast<ResourceCallbackContext*>(lParam);

    // ***** FILTERING: Only proceed if the type is potentially one we want *****
    // We still need to enumerate all types, but the real filtering happens in EnumLangsFunc
    // However, we could add a coarse check here if needed (e.g., skip known irrelevant string types)
    // For now, let EnumNamesFunc handle passing it down.

    if (!EnumResourceNamesW(hModule, lpType, reinterpret_cast<ENUMRESNAMEPROCW>(EnumNamesFunc), lParam)) {
         DWORD errorCode = GetLastError();
         if(errorCode != ERROR_RESOURCE_NAME_NOT_FOUND) {
            std::wstring typeStr = IS_INTRESOURCE(lpType) ? L"#" + std::to_wstring(reinterpret_cast<UINT_PTR>(lpType)) : lpType;
            std::wcerr << L"Warning: EnumResourceNames failed for Type '" << typeStr
                       << L"': " << GetLastErrorStdWstr(errorCode) << std::endl;
         }
    }
    return TRUE;
}

BOOL CALLBACK EnumNamesFunc(HMODULE hModule, LPCWSTR lpType, LPWSTR lpName, LONG_PTR lParam) {
    ResourceCallbackContext* context = reinterpret_cast<ResourceCallbackContext*>(lParam);
    if (!EnumResourceLanguagesW(hModule, lpType, lpName, reinterpret_cast<ENUMRESLANGPROCW>(EnumLangsFunc), lParam)) {
         DWORD errorCode = GetLastError();
         if (errorCode != ERROR_RESOURCE_LANG_NOT_FOUND) {
            std::wstring typeStr = IS_INTRESOURCE(lpType) ? L"#" + std::to_wstring(reinterpret_cast<UINT_PTR>(lpType)) : lpType;
            std::wstring nameStr = IS_INTRESOURCE(lpName) ? L"#" + std::to_wstring(reinterpret_cast<UINT_PTR>(lpName)) : lpName;
             std::wcerr << L"Warning: EnumResourceLanguages failed for Type '" << typeStr
                       << L"', Name '" << nameStr << L"': " << GetLastErrorStdWstr(errorCode) << std::endl;
         }
    }
    return TRUE;
}

BOOL CALLBACK EnumLangsFunc(HMODULE hModule, LPCWSTR lpType, LPCWSTR lpName, WORD wLang, LONG_PTR lParam) {
    ResourceCallbackContext* context = reinterpret_cast<ResourceCallbackContext*>(lParam);

    // ***** FILTERING LOGIC *****
    bool shouldCopy = false;
    if (IS_INTRESOURCE(lpType)) {
        UINT_PTR resourceID = reinterpret_cast<UINT_PTR>(lpType);
        // Check if the resource type ID matches VERSION, ICON, or GROUP_ICON
        if (resourceID == reinterpret_cast<UINT_PTR>(RT_VERSION) ||
            resourceID == reinterpret_cast<UINT_PTR>(RT_ICON) ||
            resourceID == reinterpret_cast<UINT_PTR>(RT_GROUP_ICON)) {
            shouldCopy = true;
        }
    }
    // We generally don't expect standard VERSION/ICON resources to have string names for type,
    // but if custom resources reused these names, they wouldn't be matched here.

    if (!shouldCopy) {
        return TRUE; // Skip this resource, continue enumeration
    }
    // ***** END FILTERING LOGIC *****


    // --- Proceed with copying logic only if shouldCopy is true ---
    std::wstring typeStr = IS_INTRESOURCE(lpType) ? L"#" + std::to_wstring(reinterpret_cast<UINT_PTR>(lpType)) : lpType;
    std::wstring nameStr = IS_INTRESOURCE(lpName) ? L"#" + std::to_wstring(reinterpret_cast<UINT_PTR>(lpName)) : lpName;

    // Optional: Log which specific resource is being attempted
    // std::wcout << L"Attempting to copy: Type=" << typeStr << L", Name=" << nameStr << L", Lang=" << wLang << std::endl;


    HRSRC hResInfo = FindResourceExW(hModule, lpType, lpName, wLang);
    if (hResInfo == nullptr) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: FindResourceExW failed for Type=" << typeStr << L", Name=" << nameStr << L", Lang=" << wLang
                   << L": " << GetLastErrorStdWstr(errorCode) << std::endl;
        context->resourcesFailed++;
        context->overallSuccess = false;
        return TRUE;
    }

    DWORD sRes = SizeofResource(hModule, hResInfo);
    if (sRes == 0 && GetLastError() != ERROR_SUCCESS) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: SizeofResource failed for Type=" << typeStr << L", Name=" << nameStr << L", Lang=" << wLang
                    << L": " << GetLastErrorStdWstr(errorCode) << std::endl;
        context->resourcesFailed++;
        context->overallSuccess = false;
        return TRUE;
    }

    HGLOBAL hResData = LoadResource(hModule, hResInfo);
    if (hResData == nullptr) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: LoadResource failed for Type=" << typeStr << L", Name=" << nameStr << L", Lang=" << wLang
                   << L": " << GetLastErrorStdWstr(errorCode) << std::endl;
        context->resourcesFailed++;
        context->overallSuccess = false;
        return TRUE;
    }

    LPVOID pResData = LockResource(hResData);
    if (pResData == nullptr && sRes > 0) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: LockResource failed for Type=" << typeStr << L", Name=" << nameStr << L", Lang=" << wLang
                   << L": " << GetLastErrorStdWstr(errorCode) << std::endl;
        context->resourcesFailed++;
        context->overallSuccess = false;
        return TRUE;
    }

    if (!UpdateResourceW(context->hTargetFile, lpType, lpName, wLang, pResData, sRes)) {
        DWORD errorCode = GetLastError();
        std::wcerr << L"Error: UpdateResourceW failed for Type=" << typeStr << L", Name=" << nameStr << L", Lang=" << wLang
                   << L": " << GetLastErrorStdWstr(errorCode) << std::endl;
        context->resourcesFailed++;
        context->overallSuccess = false;
    } else {
        context->resourcesCopied++;
    }

    return TRUE; // Continue enumeration
}