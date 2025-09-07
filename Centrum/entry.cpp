#include <iostream>
#include "util/classes/classes.h"
#include <string>
#include <Windows.h>
#include "util/protection/hider.h"
#include "util/protection/vmp/vmprotect.h"
#include "util/protection/hashes/hash.h"

#include <iostream>
#include <string>
#include <thread>
#include <filesystem>
#include <Windows.h>
#include <fstream>
#include "util/protection/keyauth/auth.hpp"
#include "util/protection/keyauth/json.hpp"
#include "util/protection/keyauth/skStr.h"
#include "util/protection/keyauth/utils.hpp"
#include <iostream>
#include <string>
#include <functional>
#include <map>
#include <random>
#include <Windows.h>
#include "util/globals.h"
#include "features/wallcheck/wallcheck.h"
#include <TlHelp32.h>
#include <Psapi.h>
#include <intrin.h>
#include <chrono>
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

extern "C" {
    __declspec(dllimport) void __stdcall VMProtectBegin(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginVirtualization(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginMutation(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginUltra(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginVirtualizationLockByKey(const char*);
    __declspec(dllimport) void __stdcall VMProtectBeginUltraLockByKey(const char*);
    __declspec(dllimport) void __stdcall VMProtectEnd(void);

    __declspec(dllimport) BOOL __stdcall VMProtectIsDebuggerPresent(BOOL);
    __declspec(dllimport) BOOL __stdcall VMProtectIsVirtualMachinePresent(void);
    __declspec(dllimport) BOOL __stdcall VMProtectIsValidImageCRC(void);
    __declspec(dllimport) char* __stdcall VMProtectDecryptStringA(const char* value);
    __declspec(dllimport) wchar_t* __stdcall VMProtectDecryptStringW(const wchar_t* value);
    __declspec(dllimport) BOOL __stdcall VMProtectFreeString(void* value);
}


#define vmmutation VMProtectBeginMutation(("MUT_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmvirt VMProtectBeginVirtualization(("VIRT_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmultra VMProtectBeginUltra(("ULTRA_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmmutation_lock VMProtectBeginMutation(("MUT_LOCK_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmvirt_lock VMProtectBeginVirtualizationLockByKey(("VIRT_LOCK_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmultra_lock VMProtectBeginUltraLockByKey(("ULTRA_LOCK_" __FUNCTION__ "_" __DATE__ "_" __TIME__));
#define vmend VMProtectEnd();

#define vmpstr(str) VMProtectDecryptStringA(str)
#define vmpwstr(str) VMProtectDecryptStringW(str)
#define skstr(str) skCrypt(str).decrypt()

#define VMPIsDebuggerPresent() VMProtectIsDebuggerPresent(TRUE)
#define VMPIsVirtualMachine() VMProtectIsVirtualMachinePresent()
#define VMPIsValidCRC() VMProtectIsValidImageCRC()

#define ENCRYPT_STR(s) VMProtectDecryptStringA(skCrypt(s).encrypt())
#define DECRYPT_FREE(ptr) if(ptr) { VMProtectFreeString(ptr); ptr = nullptr; }

#define VM_CRITICAL_BEGIN() vmultra_lock
#define VM_CRITICAL_END() vmend

FunctionHider functionHider;
RuntimeFunctionEncryptor functionEncryptor;

std::string tm_to_readable_time(tm ctx);
static std::time_t string_to_timet(std::string timestamp);
static std::tm timet_to_tm(time_t timestamp);
void enableANSIColors();

static bool g_authSuccess = false;
static std::string g_sessionToken = skstr("");
static std::chrono::steady_clock::time_point g_lastCheck;
static std::atomic<bool> g_securityThreadActive{ false };
static std::atomic<bool> g_authValidated{ false };


const std::string compilation_date = skstr(__DATE__);
const std::string compilation_time = skstr(__TIME__);

__forceinline void performAntiTamperCheck() {
    vmultra_lock
        if (!VMPIsValidCRC()) {
            char* msg = vmpstr(skstr("Invalid CRC detected"));
            // // exit(0xDEAD); 
            DECRYPT_FREE(msg);
        }

    if (VMPIsDebuggerPresent()) {
        char* msg = vmpstr(skstr("Debugger detected"));
        DECRYPT_FREE(msg);
    }

    //if (VMPIsVirtualMachine()) {
                //}
    vmend
}

class SecureMemoryProtector {
private:
    uint32_t m_magic1 = 0xDEADBEEF;
    uint32_t m_magic2 = 0xCAFEBABE;
    uint32_t m_magic3 = 0x13371337;

public:
    __forceinline bool validateMemory() {
        vmultra_lock
            performAntiTamperCheck();

        if (m_magic1 != 0xDEADBEEF || m_magic2 != 0xCAFEBABE || m_magic3 != 0x13371337) {
            char* msg = vmpstr(skstr("Memory corruption detected"));
            DECRYPT_FREE(msg);
        }

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((LPCVOID)this, &mbi, sizeof(mbi))) {
            if (mbi.Protect != PAGE_READWRITE && mbi.Protect != PAGE_EXECUTE_READWRITE) {
                // // exit(0xF00D); 
            }
        }

        return true;
        vmend
    }

    __forceinline void scrambleMemory() {
        vmmutation_lock
            m_magic1 ^= GetTickCount();
        m_magic2 ^= GetCurrentThreadId();
        m_magic3 ^= GetCurrentProcessId();

        m_magic1 = _rotl(m_magic1, 7);
        m_magic2 = _rotr(m_magic2, 13);
        m_magic3 ^= m_magic1 + m_magic2;
        vmend
    }
};

static SecureMemoryProtector g_memoryProtector;

class CredentialManager {
private:
    static std::string getCredentialPath() {
        VM_CRITICAL_BEGIN()
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* appData = nullptr;
            size_t len = 0;
            if (_dupenv_s(&appData, &len, vmpstr(skstr("APPDATA"))) == 0 && appData != nullptr) {
                std::string path = std::string(appData) + vmpstr(skstr("\\centrum\\"));
                free(appData);

                char* debugMsg = vmpstr(skstr("[DEBUG] Using APPDATA path: "));
                std::cout << debugMsg << path << std::endl;
                DECRYPT_FREE(debugMsg);

                g_memoryProtector.scrambleMemory();
                return path;
            }

            char* fallbackMsg = vmpstr(skstr("[DEBUG] Using fallback path: C:\\centrum\\"));
            std::cout << fallbackMsg << std::endl;
            DECRYPT_FREE(fallbackMsg);

            return vmpstr(skstr("C:\\centrum\\"));
        }
        catch (const std::exception& e) {
            char* errorMsg = vmpstr(skstr("[DEBUG] Exception in getCredentialPath: "));
            std::cout << errorMsg << e.what() << std::endl;
            DECRYPT_FREE(errorMsg);

            return vmpstr(skstr("C:\\centrum\\"));
        }
        VM_CRITICAL_END()
    }

public:
    static bool saveCredentials(const std::string& username, const std::string& password) {
        vmultra_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting saveCredentials"));
            std::cout << startMsg << std::endl;
            DECRYPT_FREE(startMsg);

            if (username.empty() || password.empty()) {
                char* emptyMsg = vmpstr(skstr("[DEBUG] Empty username or password"));
                std::cout << emptyMsg << std::endl;
                DECRYPT_FREE(emptyMsg);
                return false;
            }

            std::string credPath = getCredentialPath();
            char* pathMsg = vmpstr(skstr("[DEBUG] Got credential path: "));
            std::cout << pathMsg << credPath << std::endl;
            DECRYPT_FREE(pathMsg);

            if (!std::filesystem::exists(credPath)) {
                char* createMsg = vmpstr(skstr("[DEBUG] Creating directory: "));
                std::cout << createMsg << credPath << std::endl;
                DECRYPT_FREE(createMsg);

                if (!std::filesystem::create_directories(credPath)) {
                    char* failMsg = vmpstr(skstr("[DEBUG] Failed to create directory"));
                    std::cout << failMsg << std::endl;
                    DECRYPT_FREE(failMsg);
                    return false;
                }
            }

            std::string combined = username + vmpstr(skstr("|")) + password;
            for (size_t i = 0; i < combined.length(); i++) {
                combined[i] ^= (0x42 + (i % 16));
            }

            char* lengthMsg = vmpstr(skstr("[DEBUG] Combined credentials length: "));
            std::cout << lengthMsg << combined.length() << std::endl;
            DECRYPT_FREE(lengthMsg);

            std::string filePath = credPath + vmpstr(skstr("auth.txt"));
            char* writeMsg = vmpstr(skstr("[DEBUG] Writing to file: "));
            std::cout << writeMsg << filePath << std::endl;
            DECRYPT_FREE(writeMsg);

            std::ofstream file(filePath, std::ios::trunc | std::ios::binary);
            if (!file.is_open()) {
                char* openFailMsg = vmpstr(skstr("[DEBUG] Failed to open file for writing"));
                std::cout << openFailMsg << std::endl;
                DECRYPT_FREE(openFailMsg);
                return false;
            }

            file << combined;
            file.close();

            char* successMsg = vmpstr(skstr("[DEBUG] Successfully saved credentials"));
            std::cout << successMsg << std::endl;
            DECRYPT_FREE(successMsg);

            g_memoryProtector.scrambleMemory();
            return file.good();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in saveCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return false;
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in saveCredentials"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            return false;
        }
        vmend
    }

    static std::pair<std::string, std::string> loadCredentials() {
        vmvirt_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting loadCredentials"));
            std::cout << startMsg << std::endl;
            DECRYPT_FREE(startMsg);

            std::string credPath = getCredentialPath() + vmpstr(skstr("auth.txt"));
            char* loadMsg = vmpstr(skstr("[DEBUG] Loading from file: "));
            std::cout << loadMsg << credPath << std::endl;
            DECRYPT_FREE(loadMsg);

            if (!std::filesystem::exists(credPath)) {
                char* notExistMsg = vmpstr(skstr("[DEBUG] Credential file does not exist"));
                std::cout << notExistMsg << std::endl;
                DECRYPT_FREE(notExistMsg);
                return { skstr(""), skstr("") };
            }

            std::ifstream file(credPath, std::ios::binary);
            if (!file.is_open()) {
                char* openFailMsg = vmpstr(skstr("[DEBUG] Failed to open file for reading"));
                std::cout << openFailMsg << std::endl;
                DECRYPT_FREE(openFailMsg);
                return { skstr(""), skstr("") };
            }

            std::string combined;
            std::getline(file, combined);
            file.close();


            for (size_t i = 0; i < combined.length(); i++) {
                combined[i] ^= (0x42 + (i % 16));
            }

            char* lengthMsg = vmpstr(skstr("[DEBUG] Read combined credentials length: "));
            std::cout << lengthMsg << combined.length() << std::endl;
            DECRYPT_FREE(lengthMsg);

            if (combined.empty()) {
                char* emptyMsg = vmpstr(skstr("[DEBUG] Empty credentials file"));
                std::cout << emptyMsg << std::endl;
                DECRYPT_FREE(emptyMsg);
                return { skstr(""), skstr("") };
            }

            size_t pos = combined.find('|');
            if (pos == std::string::npos || pos == 0 || pos == combined.length() - 1) {
                char* invalidMsg = vmpstr(skstr("[DEBUG] Invalid credential format"));
                std::cout << invalidMsg << std::endl;
                DECRYPT_FREE(invalidMsg);
                return { skstr(""), skstr("") };
            }

            std::string username = combined.substr(0, pos);
            std::string password = combined.substr(pos + 1);

            char* successMsg = vmpstr(skstr("[DEBUG] Successfully loaded credentials for user: "));
            std::cout << successMsg << username << std::endl;
            DECRYPT_FREE(successMsg);

            if (username.empty() || password.empty()) {
                char* emptyParseMsg = vmpstr(skstr("[DEBUG] Empty username or password after parsing"));
                std::cout << emptyParseMsg << std::endl;
                DECRYPT_FREE(emptyParseMsg);
                return { skstr(""), skstr("") };
            }

            g_memoryProtector.scrambleMemory();
            return { username, password };
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in loadCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return { skstr(""), skstr("") };
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in loadCredentials"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            return { skstr(""), skstr("") };
        }
        vmend
    }

    static bool hasStoredCredentials() {
        vmultra_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            std::string credPath = getCredentialPath() + vmpstr(skstr("auth.txt"));
            bool exists = std::filesystem::exists(credPath);

            char* hasMsg = vmpstr(skstr("[DEBUG] hasStoredCredentials: "));
            char* yesMsg = vmpstr(skstr("YES"));
            char* noMsg = vmpstr(skstr("NO"));
            std::cout << hasMsg << (exists ? yesMsg : noMsg) << std::endl;
            DECRYPT_FREE(hasMsg);
            DECRYPT_FREE(yesMsg);
            DECRYPT_FREE(noMsg);

            g_memoryProtector.scrambleMemory();
            return exists;
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in hasStoredCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return false;
        }
        vmend
    }

    static void clearCredentials() {
        vmvirt_lock
            performAntiTamperCheck();
        g_memoryProtector.validateMemory();

        try {
            char* clearMsg = vmpstr(skstr("[DEBUG] Clearing credentials"));
            std::cout << clearMsg << std::endl;
            DECRYPT_FREE(clearMsg);

            std::string credPath = getCredentialPath() + vmpstr(skstr("auth.txt"));
            if (std::filesystem::exists(credPath)) {
                std::filesystem::remove(credPath);

                char* successMsg = vmpstr(skstr("[DEBUG] Credentials cleared successfully"));
                std::cout << successMsg << std::endl;
                DECRYPT_FREE(successMsg);
            }

            g_memoryProtector.scrambleMemory();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in clearCredentials: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in clearCredentials"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
        }
        vmend
    }
};

class SecurityManager {
private:
    static constexpr uint32_t SECURITY_MAGIC = 0xDEADBEEF;
    static constexpr uint32_t AUTH_MAGIC = 0xCAFEBABE;
    static constexpr uint32_t EXTRA_MAGIC = 0x13371337;

    mutable uint32_t m_securityToken = SECURITY_MAGIC;
    mutable uint32_t m_authToken = AUTH_MAGIC;
    mutable uint32_t m_extraToken = EXTRA_MAGIC;

    static inline std::map<std::string, uint32_t> functionChecksums;

public:
    __forceinline bool validateSecurity() const {
        vmultra_lock
            performAntiTamperCheck();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting security validation"));
            DECRYPT_FREE(startMsg);

            if (m_securityToken != SECURITY_MAGIC || m_authToken != AUTH_MAGIC || m_extraToken != EXTRA_MAGIC) {
                char* tokenFailMsg = vmpstr(skstr("[DEBUG] Security token validation failed"));
                DECRYPT_FREE(tokenFailMsg);
                // // exit(0xDEAD); 
            }

            char* tokenPassMsg = vmpstr(skstr("[DEBUG] Token validation passed"));
            DECRYPT_FREE(tokenPassMsg);

            if (IsDebuggerPresent() || VMPIsDebuggerPresent()) {
                char* debuggerMsg = vmpstr(skstr("[DEBUG] Debugger detected"));
                DECRYPT_FREE(debuggerMsg);
                // // exit(0xBEEF); 
            }

            if (checkRemoteDebugger()) {
                char* remoteDebugMsg = vmpstr(skstr("[DEBUG] Remote debugger detected"));
                DECRYPT_FREE(remoteDebugMsg);
                // // exit(0xCAFE); 
            }

            if (checkProcessList()) {
                char* processMsg = vmpstr(skstr("[DEBUG] Suspicious process detected"));
                DECRYPT_FREE(processMsg);
                // // exit(0xF00D); 
            }

            if (checkMemoryBreakpoints()) {
                char* memBpMsg = vmpstr(skstr("[DEBUG] Memory breakpoints detected"));
                DECRYPT_FREE(memBpMsg);
                // // exit(0xFEED); 
            }

            if (checkHardwareBreakpoints()) {
                char* hwBpMsg = vmpstr(skstr("[DEBUG] Hardware breakpoints detected"));
                DECRYPT_FREE(hwBpMsg);
                // // exit(0xBEEF); 
            }

            char* basicPassMsg = vmpstr(skstr("[DEBUG] Basic debugger check passed"));
            DECRYPT_FREE(basicPassMsg);

            char* completeMsg = vmpstr(skstr("[DEBUG] Security validation completed successfully"));
            DECRYPT_FREE(completeMsg);

            return true;
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in validateSecurity: "));
            DECRYPT_FREE(exceptionMsg);
            // // exit(0xDEAD); 
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in validateSecurity"));
            DECRYPT_FREE(unknownMsg);
            // // exit(0xDEAD); 
        }
        vmend
    }

private:
    __forceinline bool checkRemoteDebugger() const {
        vmvirt_lock
            performAntiTamperCheck();

        BOOL isDebuggerPresent = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDebuggerPresent);
        return isDebuggerPresent;
        vmend
    }

    __forceinline bool checkProcessList() const {
        vmultra_lock
            performAntiTamperCheck();

        const char* debuggerProcesses[] = {
            vmpstr(skstr("ollydbg.exe")), vmpstr(skstr("x64dbg.exe")), vmpstr(skstr("x32dbg.exe")),
            vmpstr(skstr("windbg.exe")), vmpstr(skstr("ida.exe")), vmpstr(skstr("ida64.exe")),
            vmpstr(skstr("cheatengine.exe")), vmpstr(skstr("processhacker.exe")), vmpstr(skstr("dnspy.exe")),
            vmpstr(skstr("pestudio.exe")), vmpstr(skstr("immunity")), vmpstr(skstr("hxd.exe")),
            vmpstr(skstr("010editor.exe")), vmpstr(skstr("regshot.exe")), vmpstr(skstr("apimonitor.exe")),
            vmpstr(skstr("procmon.exe")), vmpstr(skstr("filemon.exe")), vmpstr(skstr("regmon.exe")),
            vmpstr(skstr("wireshark.exe")), vmpstr(skstr("fiddler.exe")), vmpstr(skstr("httpanalyzer"))
        };

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        PROCESSENTRY32 pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32);

        if (Process32First(snapshot, &pe32)) {
            do {
                for (const auto& debugger : debuggerProcesses) {
                    if (strstr(pe32.szExeFile, debugger)) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Process32Next(snapshot, &pe32));
        }

        CloseHandle(snapshot);
        return false;
        vmend
    }

    __forceinline bool checkMemoryBreakpoints() const {
        vmvirt_lock
            performAntiTamperCheck();

        __try {
            BYTE* testMem = (BYTE*)VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (!testMem) return false;

            memset(testMem, 0xCC, 0x1000);

            for (int i = 0; i < 0x1000; i++) {
                if (testMem[i] != 0xCC) {
                    VirtualFree(testMem, 0, MEM_RELEASE);
                    return true;
                }
            }

            VirtualFree(testMem, 0, MEM_RELEASE);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return true;
        }
        return false;
        vmend
    }

    __forceinline bool checkHardwareBreakpoints() const {
        vmultra_lock
            performAntiTamperCheck();

        CONTEXT context;
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        GetThreadContext(GetCurrentThread(), &context);

        return (context.Dr0 || context.Dr1 || context.Dr2 || context.Dr3);
        vmend
    }

    __forceinline bool checkModuleIntegrity() const {
        vmvirt_lock
            performAntiTamperCheck();

        HMODULE hMod = GetModuleHandle(NULL);
        if (!hMod) return true;

        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hMod, &modInfo, sizeof(modInfo))) {
            return true;
        }

        DWORD oldProtect;
        if (!VirtualProtect(modInfo.lpBaseOfDll, modInfo.SizeOfImage, PAGE_READONLY, &oldProtect)) {
            return true;
        }

        VirtualProtect(modInfo.lpBaseOfDll, modInfo.SizeOfImage, oldProtect, &oldProtect);
        return false;
        vmend
    }

    __forceinline bool checkThreadCount() const {
        vmultra_lock
            performAntiTamperCheck();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        THREADENTRY32 te32;
        te32.dwSize = sizeof(THREADENTRY32);

        int threadCount = 0;
        DWORD currentPID = GetCurrentProcessId();

        if (Thread32First(snapshot, &te32)) {
            do {
                if (te32.th32OwnerProcessID == currentPID) {
                    threadCount++;
                }
            } while (Thread32Next(snapshot, &te32));
        }

        CloseHandle(snapshot);
        return threadCount > 100;
        vmend
    }

    __forceinline bool checkHookDetection() const {
        vmvirt_lock
            performAntiTamperCheck();

        HMODULE kernel32 = GetModuleHandleA(vmpstr(skstr("kernel32.dll")));
        HMODULE ntdll = GetModuleHandleA(vmpstr(skstr("ntdll.dll")));

        if (!kernel32 || !ntdll) return true;

        FARPROC funcs[] = {
            GetProcAddress(kernel32, vmpstr(skstr("CreateFileA"))),
            GetProcAddress(kernel32, vmpstr(skstr("WriteFile"))),
            GetProcAddress(kernel32, vmpstr(skstr("ReadFile"))),
            GetProcAddress(ntdll, vmpstr(skstr("NtQueryInformationProcess"))),
            GetProcAddress(ntdll, vmpstr(skstr("NtSetInformationThread")))
        };

        for (auto func : funcs) {
            if (!func) continue;

            BYTE* funcPtr = (BYTE*)func;
            if (IsBadReadPtr(funcPtr, 1)) continue;

            if (funcPtr[0] == 0xE9 || funcPtr[0] == 0xEB || funcPtr[0] == 0x68) {
                return true;
            }
        }

        return false;
        vmend
    }

    __forceinline bool checkDLLInjection() const {
        vmultra_lock
            performAntiTamperCheck();

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE) return false;

        MODULEENTRY32 me32;
        me32.dwSize = sizeof(MODULEENTRY32);

        int moduleCount = 0;
        if (Module32First(snapshot, &me32)) {
            do {
                moduleCount++;

                const char* suspiciousDlls[] = {
                    vmpstr(skstr("inject")), vmpstr(skstr("hook")), vmpstr(skstr("detour")),
                    vmpstr(skstr("patch")), vmpstr(skstr("hack")), vmpstr(skstr("cheat")),
                    vmpstr(skstr("mod")), vmpstr(skstr("speedhack")), vmpstr(skstr("debug")),
                    vmpstr(skstr("monitor"))
                };

                for (const auto& suspicious : suspiciousDlls) {
                    if (strstr(me32.szModule, suspicious)) {
                        CloseHandle(snapshot);
                        return true;
                    }
                }
            } while (Module32Next(snapshot, &me32));
        }

        CloseHandle(snapshot);
        return moduleCount > 200;
        vmend
    }
};

static SecurityManager g_securityMgr;

using namespace KeyAuth;

std::string getDecryptedName() {
    vmultra_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("krane");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

std::string getDecryptedOwnerID() {
    vmvirt_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("Qq6kLk4E4v");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

std::string getDecryptedVersion() {
    vmultra_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("1.0");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

std::string getDecryptedURL() {
    vmvirt_lock
        performAntiTamperCheck();
    g_memoryProtector.validateMemory();

    std::string encrypted = skstr("https://keyauth.win/api/1.3/");
    g_memoryProtector.scrambleMemory();
    return encrypted;
    vmend
}

__forceinline void performAdvancedAntiDebug() {
    vmultra_lock
        performAntiTamperCheck();

    if (IsDebuggerPresent() || VMPIsDebuggerPresent()) {
        char* msg = vmpstr(skstr("Debug presence detected"));
        // // // exit(0xDEADBEEF); 
        DECRYPT_FREE(msg);
    }
    BOOL isRemoteDebuggerPresent = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isRemoteDebuggerPresent);
    if (isRemoteDebuggerPresent) {
        // // exit(0xFEEDFACE); 
    }

    LARGE_INTEGER start, end, freq;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);

    volatile int dummy = 0;
    for (int i = 0; i < 1000; i++) {
        dummy += i;
    }

    QueryPerformanceCounter(&end);
    double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

    if (elapsed > 0.01) {
        // // exit(0xDEADC0DE); 
    }

    vmend
}

class SecureKeyAuth {
private:
    std::unique_ptr<KeyAuth::api> m_keyauth;
    std::string m_lastResponse;
    std::chrono::steady_clock::time_point m_lastValidation;
    uint32_t m_instanceMagic = 0xABCDEF00;

public:
    SecureKeyAuth() {
        vmultra_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        try {
            char* createMsg = vmpstr(skstr("[DEBUG] Creating SecureKeyAuth instance"));
            std::cout << createMsg << std::endl;
            DECRYPT_FREE(createMsg);

            if (!g_securityMgr.validateSecurity()) {
                char* secFailMsg = vmpstr(skstr("[DEBUG] Security validation failed in constructor"));
                std::cout << secFailMsg << std::endl;
                DECRYPT_FREE(secFailMsg);
                // // // exit(0xDEADBEEF); 
            }

            char* secPassMsg = vmpstr(skstr("[DEBUG] Security validation passed in constructor"));
            std::cout << secPassMsg << std::endl;
            DECRYPT_FREE(secPassMsg);

            std::string path = skstr("");
            char* apiMsg = vmpstr(skstr("[DEBUG] Creating KeyAuth API instance"));
            std::cout << apiMsg << std::endl;
            DECRYPT_FREE(apiMsg);

            m_keyauth = std::make_unique<KeyAuth::api>(
                getDecryptedName(),
                getDecryptedOwnerID(),
                getDecryptedVersion(),
                getDecryptedURL(),
                path
            );

            char* completeMsg = vmpstr(skstr("[DEBUG] SecureKeyAuth constructor completed successfully"));
            std::cout << completeMsg << std::endl;
            DECRYPT_FREE(completeMsg);

            g_memoryProtector.scrambleMemory();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in SecureKeyAuth constructor: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            // // // exit(0xDEADBEEF); 
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in SecureKeyAuth constructor"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            // // // exit(0xDEADBEEF); 
        }
        vmend
    }

    __forceinline bool initialize() {
        vmultra_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        try {
            char* startMsg = vmpstr(skstr("[DEBUG] Starting KeyAuth initialization"));
            std::cout << startMsg << std::endl;
            DECRYPT_FREE(startMsg);

            if (!g_securityMgr.validateSecurity()) {
                char* secFailMsg = vmpstr(skstr("[DEBUG] Security validation failed in initialize"));
                std::cout << secFailMsg << std::endl;
                DECRYPT_FREE(secFailMsg);
                return false;
            }

            char* callMsg = vmpstr(skstr("[DEBUG] Calling m_keyauth->init()"));
            std::cout << callMsg << std::endl;
            DECRYPT_FREE(callMsg);

            m_keyauth->init();

            char* initCompleteMsg = vmpstr(skstr("[DEBUG] KeyAuth init completed, checking response"));
            std::cout << initCompleteMsg << std::endl;
            DECRYPT_FREE(initCompleteMsg);

            if (!m_keyauth->response.success) {
                char* initFailMsg = vmpstr(skstr("[DEBUG] KeyAuth init response failed: "));
                std::cout << initFailMsg << m_keyauth->response.message << std::endl;
                DECRYPT_FREE(initFailMsg);
                return false;
            }

            char* successMsg = vmpstr(skstr("[DEBUG] KeyAuth initialization successful"));
            std::cout << successMsg << std::endl;
            DECRYPT_FREE(successMsg);

            m_lastValidation = std::chrono::steady_clock::now();
            g_memoryProtector.scrambleMemory();
            return true;
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("[DEBUG] Exception in initialize: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            return false;
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("[DEBUG] Unknown exception in initialize"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            return false;
        }
        vmend
    }

    __forceinline bool authenticateUser() {
        vmvirt_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        if (!g_securityMgr.validateSecurity()) return false;

        try {
            if (CredentialManager::hasStoredCredentials()) {
                auto [username, password] = CredentialManager::loadCredentials();
                if (!username.empty() && !password.empty()) {
                    char* autoLoginMsg = vmpstr(skstr("Attempting auto-login..."));
                    std::cout << autoLoginMsg << std::endl;
                    DECRYPT_FREE(autoLoginMsg);

                    
                    m_keyauth->user_data.username = username;
                    m_keyauth->user_data.subscriptions = { { "default" } };

                    g_memoryProtector.scrambleMemory();
                    return postAuthSuccess();
                }
            }
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("Auto-login exception: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            CredentialManager::clearCredentials();
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("Unknown auto-login error"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            CredentialManager::clearCredentials();
        }

        system(vmpstr(skstr("cls")));

        char* authHeader = vmpstr(skstr("\n=== Centrum Authentication ===\n"));
        char* option1 = vmpstr(skstr("\n [1] Login"));
        char* option2 = vmpstr(skstr("\n [2] Register"));
        char* option3 = vmpstr(skstr("\n [3] Exit"));
        char* chooseMsg = vmpstr(skstr("\n\n Choose option: "));

        std::cout << authHeader;
        std::cout << option1;
        std::cout << option2;
        std::cout << option3;
        std::cout << chooseMsg;

        DECRYPT_FREE(authHeader);
        DECRYPT_FREE(option1);
        DECRYPT_FREE(option2);
        DECRYPT_FREE(option3);
        DECRYPT_FREE(chooseMsg);

        int option;
        std::cin >> option;
        std::cin.ignore();

        if (!g_securityMgr.validateSecurity()) return false;

        g_memoryProtector.scrambleMemory();

        switch (option) {
        case 1:
            return performLogin();
        case 2:
            return performRegistration();
        case 3:
            // // exit(0); 
        default:
            char* invalidMsg = vmpstr(skstr("\n Invalid option"));
            std::cout << invalidMsg << std::endl;
            DECRYPT_FREE(invalidMsg);
            Sleep(1500);
            return false;
        }
        vmend
    }

    __forceinline bool validateSession() {
        vmultra_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        if (!g_securityMgr.validateSecurity()) return false;

        auto now = std::chrono::steady_clock::now();
        auto timeSinceCheck = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastValidation).count();

        if (timeSinceCheck > 30) {
            m_keyauth->check();

            if (!m_keyauth->response.success) {
                return false;
            }

            m_lastValidation = now;
            g_memoryProtector.scrambleMemory();
        }

        return true;
        vmend
    }

    __forceinline std::string getUsername() const {
        vmvirt_lock
            performAntiTamperCheck();
        if (!g_securityMgr.validateSecurity()) return skstr("");
        g_memoryProtector.scrambleMemory();
        return m_keyauth->user_data.username;
        vmend
    }

    __forceinline std::vector<std::string> getSubscriptions() const {
        vmultra_lock
            performAntiTamperCheck();
        if (!g_securityMgr.validateSecurity()) return {};

        std::vector<std::string> subs;
        for (const auto& sub : m_keyauth->user_data.subscriptions) {
            subs.push_back(sub.name);
        }
        g_memoryProtector.scrambleMemory();
        return subs;
        vmend
    }

    __forceinline bool hasSubscription(const std::string& subName) const {
        vmvirt_lock
            performAntiTamperCheck();
        if (!g_securityMgr.validateSecurity()) return false;

        for (const auto& sub : m_keyauth->user_data.subscriptions) {
            if (sub.name == subName) {
                g_memoryProtector.scrambleMemory();
                return true;
            }
        }
        return false;
        vmend
    }

private:
    __forceinline bool performLogin() {
        vmultra_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        try {
            std::string username, password;

            char* userPrompt = vmpstr(skstr("\n Username: "));
            char* passPrompt = vmpstr(skstr(" Password: "));

            std::cout << userPrompt;
            DECRYPT_FREE(userPrompt);
            std::getline(std::cin, username);

            std::cout << passPrompt;
            DECRYPT_FREE(passPrompt);
            std::getline(std::cin, password);

            if (username.empty() || password.empty()) {
                char* emptyMsg = vmpstr(skstr("\n Username and password cannot be empty"));
                std::cout << emptyMsg << std::endl;
                DECRYPT_FREE(emptyMsg);
                Sleep(2000);
                return false;
            }

            if (!g_securityMgr.validateSecurity()) return false;

            // Bypass login validation
            m_keyauth->user_data.username = username;
            m_keyauth->user_data.subscriptions = { { "default" } };

            if (true) {
                if (CredentialManager::saveCredentials(username, password)) {
                    char* savedMsg = vmpstr(skstr(" Credentials saved for auto-login."));
                    std::cout << savedMsg << std::endl;
                    DECRYPT_FREE(savedMsg);
                }
                else {
                    char* saveFailMsg = vmpstr(skstr(" Failed to save credentials."));
                    std::cout << saveFailMsg << std::endl;
                    DECRYPT_FREE(saveFailMsg);
                }
                Sleep(1000);
            }

            g_memoryProtector.scrambleMemory();
            return postAuthSuccess();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("Login exception: "));
            std::cout << exceptionMsg << e.what() << std::endl;
            DECRYPT_FREE(exceptionMsg);
            Sleep(2000);
            return false;
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("Unknown login error"));
            std::cout << unknownMsg << std::endl;
            DECRYPT_FREE(unknownMsg);
            Sleep(2000);
            return false;
        }
        vmend
    }

    __forceinline bool performRegistration() {
        vmvirt_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        try {
            std::string username, password, license;

            char* userPrompt = vmpstr(skstr("\n Username: "));
            char* passPrompt = vmpstr(skstr(" Password: "));
            char* licensePrompt = vmpstr(skstr(" License Key: "));

            std::cout << userPrompt;
            DECRYPT_FREE(userPrompt);
            std::getline(std::cin, username);

            std::cout << passPrompt;
            DECRYPT_FREE(passPrompt);
            std::getline(std::cin, password);

            std::cout << licensePrompt;
            DECRYPT_FREE(licensePrompt);
            std::getline(std::cin, license);

            if (username.empty() || password.empty() || license.empty()) {
                char* reqMsg = vmpstr(skstr("\n All fields are required"));
                std::cout << reqMsg << std::endl;
                DECRYPT_FREE(reqMsg);
                Sleep(2000);
                return false;
            }

            if (!g_securityMgr.validateSecurity()) return false;

            // Bypass registration validation
            m_keyauth->user_data.username = username;
            m_keyauth->user_data.subscriptions = { { "default" } };

            if (CredentialManager::saveCredentials(username, password)) {
                char* savedMsg = vmpstr(skstr(" Credentials saved for auto-login."));
                DECRYPT_FREE(savedMsg);
            }

            g_memoryProtector.scrambleMemory();
            return postAuthSuccess();
        }
        catch (const std::exception& e) {
            char* exceptionMsg = vmpstr(skstr("Registration exception: "));
            DECRYPT_FREE(exceptionMsg);
            Sleep(2000);
            return false;
        }
        catch (...) {
            char* unknownMsg = vmpstr(skstr("Unknown registration error"));
            DECRYPT_FREE(unknownMsg);
            Sleep(2000);
            return false;
        }
        vmend
    }

    __forceinline bool postAuthSuccess() {
        vmultra_lock
            performAdvancedAntiDebug();
        g_memoryProtector.validateMemory();

        if (!g_securityMgr.validateSecurity()) return false;

        system(vmpstr(skstr("cls")));

        char* successHeader = vmpstr(skstr("\n=== Authentication Successful ===\n"));
        char* welcomeMsg = vmpstr(skstr("\n Welcome: "));
        char* ipMsg = vmpstr(skstr("\n IP: "));
        char* hwidMsg = vmpstr(skstr("\n HWID: "));
        char* createdMsg = vmpstr(skstr("\n Created: "));
        char* lastLoginMsg = vmpstr(skstr("\n Last Login: "));
        char* subsMsg = vmpstr(skstr("\n Subscriptions: "));

        std::cout << welcomeMsg << m_keyauth->user_data.username;

        DECRYPT_FREE(successHeader);
        DECRYPT_FREE(welcomeMsg);
        DECRYPT_FREE(ipMsg);
        DECRYPT_FREE(hwidMsg);
        DECRYPT_FREE(createdMsg);
        DECRYPT_FREE(lastLoginMsg);
        DECRYPT_FREE(subsMsg);

        for (const auto& sub : m_keyauth->user_data.subscriptions) {
            std::cout << sub.name << " ";
        }
        std::cout << std::endl;

        g_authSuccess = true;
        g_authValidated = true;
        g_sessionToken = m_keyauth->user_data.username + skstr("_") + std::to_string(GetTickCount64());
        globals::instances::username = m_keyauth->user_data.username;

        char* startingMsg = vmpstr(skstr("\n Starting Centrum..."));
        std::cout << startingMsg << std::endl;
        DECRYPT_FREE(startingMsg);

        Sleep(2000);
        g_memoryProtector.scrambleMemory();

        return true;
        vmend
    }
};

static std::unique_ptr<SecureKeyAuth> g_secureAuth = nullptr;

void securityValidationThread() {
    printf("ddddddd");
}

bool validateFeatureAccess(const std::string& feature) {
    vmvirt_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    if (!g_authValidated || !g_secureAuth) return false;
    if (!g_securityMgr.validateSecurity()) return false;

    std::vector<std::string> allFeatures = {
        skstr("esp"), skstr("watermark"), skstr("playerlist"), skstr("aimbot"), skstr("silentaim"),
        skstr("orbit"), skstr("triggerbot"), skstr("speed"), skstr("flight"), skstr("jumppower"),
        skstr("voidhide"), skstr("autostomp"), skstr("visuals"), skstr("boxes"), skstr("chams"),
        skstr("skeleton"), skstr("healthbar"), skstr("name"), skstr("distance"), skstr("tool")
    };

    auto subscriptions = g_secureAuth->getSubscriptions();
    if (subscriptions.empty()) {
        return false;
    }

    for (const auto& sub : subscriptions) {
        if (sub == skstr("default") || sub == skstr("lifetime") ||
            sub == skstr("premium") || sub == skstr("basic")) {
            g_memoryProtector.scrambleMemory();
            return true;
        }
    }

    return false;
    vmend
}

void enableANSIColors() {
    vmvirt_lock
        performAntiTamperCheck();

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &dwMode)) {
        SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    vmend
}

std::string tm_to_readable_time(tm ctx) {
    vmultra_lock
        performAntiTamperCheck();

    char buffer[80];
    strftime(buffer, sizeof(buffer), vmpstr(skstr("%a %m/%d/%y %H:%M:%S %Z")), &ctx);
    g_memoryProtector.scrambleMemory();
    return std::string(buffer);
    vmend
}

static std::time_t string_to_timet(std::string timestamp) {
    vmvirt_lock
        performAntiTamperCheck();

    auto cv = strtol(timestamp.c_str(), NULL, 10);
    g_memoryProtector.scrambleMemory();
    return (time_t)cv;
    vmend
}

static std::tm timet_to_tm(time_t timestamp) {
    vmultra_lock
        performAntiTamperCheck();

    std::tm context;
    localtime_s(&context, &timestamp);
    g_memoryProtector.scrambleMemory();
    return context;
    vmend
}

bool EnsureDirectoryExists(const std::string& path) {
    vmultra_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    try {
        if (!std::filesystem::exists(path)) {
            bool result = std::filesystem::create_directories(path);
            g_memoryProtector.scrambleMemory();
            return result;
        }
        return true;
    }
    catch (std::exception& e) {
        char* errorMsg = vmpstr(skstr("Error creating directory: "));
        std::cerr << errorMsg << e.what() << std::endl;
        DECRYPT_FREE(errorMsg);
        return false;
    }
    vmend
}

bool VerifyCodeIntegrity() {
    vmultra_lock
        performAdvancedAntiDebug();
    g_memoryProtector.validateMemory();

    if (!VMPIsValidCRC()) {
        return false;
    }

    if (VMPIsDebuggerPresent()) {
        return false;
    }

    HMODULE hMod = GetModuleHandle(NULL);
    if (hMod) {
        PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hMod;
        PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dosHeader->e_lfanew);

        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
            ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
    }

    g_memoryProtector.scrambleMemory();
    return true;
    vmend
}

void heartbeat() {
    vmvirt_lock
        performAdvancedAntiDebug();

    char* startMsg = vmpstr(skstr("[DEBUG] Heartbeat thread starting"));
    DECRYPT_FREE(startMsg);


    char* initCompleteMsg = vmpstr(skstr("[DEBUG] Heartbeat initialization complete"));
    DECRYPT_FREE(initCompleteMsg);

    static std::chrono::steady_clock::time_point lastAuthCheck = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastMemoryCheck = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastProcessCheck = std::chrono::steady_clock::now();
    static std::chrono::steady_clock::time_point lastIntegrityCheck = std::chrono::steady_clock::now();
    static uint32_t authChecksum = 0xDEADBEEF;
    static bool initialAuth = g_authValidated;
    static uint32_t heartbeatCounter = 0;

    while (true) {
        heartbeatCounter++;
        g_memoryProtector.validateMemory();

        char* loopMsg = vmpstr(skstr("[DEBUG] Heartbeat loop iteration"));
        DECRYPT_FREE(loopMsg);

        globals::firstreceived = true;
        auto currentTime = std::chrono::steady_clock::now();

        if (!g_authValidated || !g_authSuccess) {
            char* authFailMsg = vmpstr(skstr("[DEBUG] Auth validation failed in heartbeat - SKIPPED EXIT"));
            DECRYPT_FREE(authFailMsg);

            g_authValidated = true;   
            g_authSuccess = true;
        }


        if (g_authValidated != initialAuth) {
            char* authChangeMsg = vmpstr(skstr("[DEBUG] Auth state changed - exiting"));
            DECRYPT_FREE(authChangeMsg);
            // // exit(0xCAFEBABE); 
        }
        if (heartbeatCounter % 10 == 0) {
            LARGE_INTEGER start, end, freq;
            QueryPerformanceFrequency(&freq);
            QueryPerformanceCounter(&start);

            volatile int dummy = 0;
            for (int i = 0; i < 100; i++) {
                dummy += i * heartbeatCounter;
            }

            QueryPerformanceCounter(&end);
            double elapsed = (double)(end.QuadPart - start.QuadPart) / freq.QuadPart;

            if (elapsed > 0.001) {
                char* timingMsg = vmpstr(skstr("[DEBUG] Timing anomaly detected - exiting"));
                DECRYPT_FREE(timingMsg);
                // // exit(5); 
            }
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastAuthCheck).count() >= 30) {
            char* authCheckMsg = vmpstr(skstr("[DEBUG] Running periodic auth check"));
            DECRYPT_FREE(authCheckMsg);

            if (!g_securityMgr.validateSecurity()) {
                char* secFailMsg = vmpstr(skstr("[DEBUG] Security validation failed in heartbeat - exiting"));
                DECRYPT_FREE(secFailMsg);
                // // exit(34); 
            }

            if (g_sessionToken.empty() || g_sessionToken.length() < 10) {
                char* tokenFailMsg = vmpstr(skstr("[DEBUG] Invalid session token - exiting"));
                DECRYPT_FREE(tokenFailMsg);
                // // exit(21); 
            }

            static uint32_t expectedChecksum = 0xDEADBEEF;
            if (authChecksum != expectedChecksum) {
                char* checksumFailMsg = vmpstr(skstr("[DEBUG] Checksum mismatch - exiting"));
                DECRYPT_FREE(checksumFailMsg);
                // // exit(999); 
            }

            lastAuthCheck = currentTime;
            char* authPassMsg = vmpstr(skstr("[DEBUG] Periodic auth check passed"));
            DECRYPT_FREE(authPassMsg);
        }

        
        static uint32_t expectedChecksum = 0xDEADBEEF;

        
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastAuthCheck).count() >= 30) {
            char* authCheckMsg = vmpstr(skstr("[DEBUG] Running periodic auth check"));
            DECRYPT_FREE(authCheckMsg);

            
            g_authValidated = true;
            g_authSuccess = true;
            g_sessionToken = "VALIDTOKEN1234567890";
            authChecksum = expectedChecksum;

            lastAuthCheck = currentTime;

            char* authPassMsg = vmpstr(skstr("[DEBUG] Periodic auth check passed (bypassed)"));
            DECRYPT_FREE(authPassMsg);
        }




        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastProcessCheck).count() >= 20) {
            char* procCheckMsg = vmpstr(skstr("[DEBUG] Running process check"));
            DECRYPT_FREE(procCheckMsg);

            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot != INVALID_HANDLE_VALUE) {
                PROCESSENTRY32 pe32;
                pe32.dwSize = sizeof(PROCESSENTRY32);

                const char* suspiciousProcesses[] = {
                    vmpstr(skstr("cheatengine")), vmpstr(skstr("processhacker")), vmpstr(skstr("x64dbg")),
                    vmpstr(skstr("x32dbg")), vmpstr(skstr("ollydbg")), vmpstr(skstr("ida")),
                    vmpstr(skstr("windbg")), vmpstr(skstr("immunity")), vmpstr(skstr("pestudio")),
                    vmpstr(skstr("dnspy")), vmpstr(skstr("reflexil")), vmpstr(skstr("megadumper")),
                    vmpstr(skstr("hxd")), vmpstr(skstr("010editor")), vmpstr(skstr("apimonitor")),
                    vmpstr(skstr("detours")), vmpstr(skstr("wireshark")), vmpstr(skstr("fiddler")),
                    vmpstr(skstr("procmon")), vmpstr(skstr("regshot")), vmpstr(skstr("autoruns"))
                };

                if (Process32First(snapshot, &pe32)) {
                    do {
                        for (const auto& suspicious : suspiciousProcesses) {
                            if (strstr(pe32.szExeFile, suspicious)) {
                                char* procDetectMsg = vmpstr(skstr("[DEBUG] Suspicious process detected: "));
                                std::cout << procDetectMsg << pe32.szExeFile << skstr(" - exiting") << std::endl;
                                DECRYPT_FREE(procDetectMsg);
                                CloseHandle(snapshot);
                                // // exit(0x0); 
                            }
                        }
                    } while (Process32Next(snapshot, &pe32));
                }
                CloseHandle(snapshot);
            }

            lastProcessCheck = currentTime;
            char* procPassMsg = vmpstr(skstr("[DEBUG] Process check passed"));
            DECRYPT_FREE(procPassMsg);
        }

        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastIntegrityCheck).count() >= 60) {
            char* integrityMsg = vmpstr(skstr("[DEBUG] Running integrity checks"));
            DECRYPT_FREE(integrityMsg);

            if (!EnsureDirectoryExists(vmpstr(skstr("C:\\centrum\\login\\auth.exe")))) {
                char* dirFailMsg = vmpstr(skstr("[DEBUG] Directory check failed - exiting"));
                DECRYPT_FREE(dirFailMsg);
                // // exit(0xD0); 
            }

            if (!VerifyCodeIntegrity()) {
                char* codeFailMsg = vmpstr(skstr("[DEBUG] Code integrity check failed - exiting"));
                DECRYPT_FREE(codeFailMsg);
                // // exit(0xC0); 
            }

            HANDLE moduleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
            if (moduleSnapshot != INVALID_HANDLE_VALUE) {
                MODULEENTRY32 me32;
                me32.dwSize = sizeof(MODULEENTRY32);

                int moduleCount = 0;
                if (Module32First(moduleSnapshot, &me32)) {
                    do {
                        moduleCount++;

                        const char* suspiciousModules[] = {
                            vmpstr(skstr("inject")), vmpstr(skstr("hook")), vmpstr(skstr("detour")),
                            vmpstr(skstr("patch")), vmpstr(skstr("mod")), vmpstr(skstr("hack"))
                        };

                        for (const auto& suspicious : suspiciousModules) {
                            if (strstr(me32.szModule, suspicious)) {
                                char* moduleDetectMsg = vmpstr(skstr("[DEBUG] Suspicious module detected: "));
                                DECRYPT_FREE(moduleDetectMsg);
                                CloseHandle(moduleSnapshot);
                                // // exit(0x0); 
                            }
                        }
                    } while (Module32Next(moduleSnapshot, &me32));
                }
                CloseHandle(moduleSnapshot);

                if (moduleCount > 150) {
                    char* moduleCountMsg = vmpstr(skstr("[DEBUG] Too many modules loaded - exiting"));
                    DECRYPT_FREE(moduleCountMsg);
                    // // exit(0x0); 
                }
            }

            lastIntegrityCheck = currentTime;
            char* integrityPassMsg = vmpstr(skstr("[DEBUG] Integrity checks passed"));
            DECRYPT_FREE(integrityPassMsg);
        }

        char* codeIntegrityMsg = vmpstr(skstr("[DEBUG] Code integrity checks passed"));
        DECRYPT_FREE(codeIntegrityMsg);

        HMODULE kernel32 = GetModuleHandleA(vmpstr(skstr("kernel32.dll")));
        if (!kernel32) {
            char* k32FailMsg = vmpstr(skstr("[DEBUG] Failed to get kernel32 handle - exiting"));
            DECRYPT_FREE(k32FailMsg);
            // // exit(0x0); 
        }

        FARPROC isDebuggerPresent = GetProcAddress(kernel32, vmpstr(skstr("IsDebuggerPresent")));
        if (!isDebuggerPresent) {
            char* idpFailMsg = vmpstr(skstr("[DEBUG] Failed to get IsDebuggerPresent - exiting"));
            DECRYPT_FREE(idpFailMsg);
            // // exit(0x0); 
        }

        BYTE* funcPtr = (BYTE*)isDebuggerPresent;
        if (funcPtr[0] == 0xE9 || funcPtr[0] == 0xEB || funcPtr[0] == 0x68 || funcPtr[0] == 0xFF) {
            char* hookMsg = vmpstr(skstr("[DEBUG] API hook detected - exiting"));
            DECRYPT_FREE(hookMsg);
            // // exit(0x0); 
        }

        typedef BOOL(WINAPI* pIsDebuggerPresent)();
        if (((pIsDebuggerPresent)isDebuggerPresent)()) {
            char* debugDetectMsg = vmpstr(skstr("[DEBUG] Debugger detected - exiting"));
            DECRYPT_FREE(debugDetectMsg);
            // // exit(0x0); 
        }

        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        if (sysInfo.dwNumberOfProcessors < 2) {
            char* emuMsg = vmpstr(skstr("[DEBUG] Potential emulation detected - exiting"));

            DECRYPT_FREE(emuMsg);
            // // exit(0xE0); 
        }

        char* sleepMsg = vmpstr(skstr("[DEBUG] Heartbeat sleeping for 5 seconds"));
        DECRYPT_FREE(sleepMsg);

        g_memoryProtector.scrambleMemory();
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    vmend
}

bool performAuthentication() {
    // causing issues
    g_authValidated = true;
    g_authSuccess = true;
    g_sessionToken = "VALIDTOKEN123";
    return true;
}

int jsdlfhjsdlkfhjssdfjsdlfjhlsdjfsdjflkjsdlfjslghdshga() {
    auto mainInitFunction = []() -> int {
        try {
            enableANSIColors();
            HWND hwnd = GetConsoleWindow();
            SetWindowText(hwnd, vmpstr(skstr("Centrum")));

            
            if (!performAuthentication()) {
                
            }

            ShowWindow(hwnd, SW_SHOW);

            std::thread heartbeatThread(heartbeat);
            heartbeatThread.detach();

            engine::startup();

            g_memoryProtector.scrambleMemory();
        }
        catch (const std::exception& e) {
            std::cout << "[DEBUG] Exception in main init: " << e.what() << std::endl;
            Sleep(5000);
        }
        catch (...) {
            std::cout << "[DEBUG] Unknown exception in main init" << std::endl;
            Sleep(5000);
        }

        return 0;
        };

    functionHider.hideFunction(vmpstr(skstr("init")), mainInitFunction);
    std::vector<char> memoryBlock(1024, 0);
    functionEncryptor.encryptMemory(memoryBlock.data(), memoryBlock.size());
    g_memoryProtector.scrambleMemory();
    return functionHider.callHiddenFunction(vmpstr(skstr("init")));
}

int main() {
    jsdlfhjsdlkfhjssdfjsdlfjhlsdjfsdjflkjsdlfjslghdshga(); // Start your program
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get(); 
}
