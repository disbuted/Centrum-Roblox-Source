#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdint>
#include <Psapi.h>
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <memory>
#include <winternl.h>


inline uintptr_t base_address;

typedef NTSTATUS(NTAPI* pNtQuerySystemInformation)(
    ULONG SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
    );

typedef NTSTATUS(NTAPI* pNtClose)(HANDLE Handle);
typedef NTSTATUS(NTAPI* pNtSetInformationProcess)(HANDLE ProcessHandle, ULONG ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToWrite, PULONG NumberOfBytesWritten);
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, ULONG NumberOfBytesToRead, PULONG NumberOfBytesRead);

typedef struct _PROCESS_BASIC_INFORMATIONZ {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    LONG BasePriority;
    HANDLE UniqueProcessId;
    HANDLE InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATIONZ, * PPROCESS_BASIC_INFORMATIONZ;

typedef enum _SYSTEM_INFORMATION_CLASSZ {
    SystemHandleInformation = 16
} SYSTEM_INFORMATION_CLASSZ;

typedef struct _SYSTEM_HANDLE_INFORMATION {
    ULONG ProcessId;
    UCHAR ObjectTypeNumber;
    UCHAR Flags;
    USHORT Handle;
    PVOID Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_INFORMATION, * PSYSTEM_HANDLE_INFORMATION;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
    ULONG_PTR NumberOfHandles;
    SYSTEM_HANDLE_INFORMATION Information[1];
} SYSTEM_HANDLE_INFORMATION_EX, * PSYSTEM_HANDLE_INFORMATION_EX;

namespace mem {
    inline std::atomic<HANDLE> roblox_h{ nullptr };
    inline std::atomic<INT32> process_id{ 0 };

    inline pNtWriteVirtualMemory NtWriteVirtualMemory = nullptr;
    inline pNtReadVirtualMemory NtReadVirtualMemory = nullptr;
    inline std::atomic<bool> use_nt_functions{ false };

    inline std::shared_mutex cache_mutex;
    inline std::unordered_map<uintptr_t, DWORD> page_protection_cache;

    inline std::shared_mutex regions_mutex;
    struct MemoryRegion {
        uintptr_t base;
        uintptr_t end;
        DWORD protection;
        bool writable;
    };
    inline std::vector<MemoryRegion> memory_regions;
    inline std::atomic<ULONGLONG> last_region_update{ 0 };

    inline std::mutex handle_mutex;
    struct HandleCandidate {
        DWORD sourcePID;
        HANDLE duplicatedHandle;
        DWORD accessRights;
        bool tested;
        bool functional;
    };
    inline std::vector<HandleCandidate> handle_candidates;

    inline void initialize_nt_functions() {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            NtWriteVirtualMemory = (pNtWriteVirtualMemory)GetProcAddress(hNtdll, "NtWriteVirtualMemory");
            NtReadVirtualMemory = (pNtReadVirtualMemory)GetProcAddress(hNtdll, "NtReadVirtualMemory");

            if (NtWriteVirtualMemory && NtReadVirtualMemory) {
                use_nt_functions.store(true);
                std::cout << "[PERF] NT functions loaded successfully" << std::endl;
            }
        }
    }

    inline void update_memory_regions() {
        static std::atomic_flag updating = ATOMIC_FLAG_INIT;
        if (updating.test_and_set(std::memory_order_acquire))
            return;
        struct Unlock { ~Unlock() { updating.clear(std::memory_order_release); } } unlock;

        HANDLE current_handle = roblox_h.load();
        if (!current_handle)
            return;
        const ULONGLONG kMinInterval = 1000;
        ULONGLONG now = GetTickCount64();
        ULONGLONG previous = last_region_update.load(std::memory_order_relaxed);
        if (now - previous < kMinInterval)
            return;

        std::vector<MemoryRegion> new_regions;
        new_regions.reserve(512);

        MEMORY_BASIC_INFORMATION mbi{};
        uintptr_t address = 0;
        size_t    region_count = 0;

        while (region_count < 10000) {
            SIZE_T got = VirtualQueryEx(current_handle, (LPCVOID)address, &mbi, sizeof(mbi));
            if (got != sizeof(mbi) || mbi.RegionSize == 0)
                break;

            if (mbi.State == MEM_COMMIT) {
                uintptr_t base = (uintptr_t)mbi.BaseAddress;
                uintptr_t end = base + mbi.RegionSize;
                DWORD prot = mbi.Protect;
                bool writable = (prot & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;

                if (base < end) {
                    new_regions.push_back({ base, end, prot, writable });
                    ++region_count;
                }
            }

            uintptr_t next = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            if (next <= address)
                break;
            address = next;
        }

        {
            std::unique_lock<std::shared_mutex> lk(regions_mutex);
            if (!roblox_h.load())
                return;
            memory_regions = std::move(new_regions);
            last_region_update.store(now, std::memory_order_relaxed);
        }

        std::cout << "[PERF] Updated " << memory_regions.size()
            << " memory regions safely" << std::endl;
    }


    inline bool is_address_writable_fast(uintptr_t address) {
        HANDLE current_handle = roblox_h.load();
        if (!current_handle || address == 0) return false;

        uintptr_t page_addr = address & ~0xFFF;

        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex);
            auto it = page_protection_cache.find(page_addr);
            if (it != page_protection_cache.end()) {
                return (it->second & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
            }
        }

        {
            std::shared_lock<std::shared_mutex> lock(regions_mutex);
            for (const auto& region : memory_regions) {
                if (address >= region.base && address < region.end && region.end > region.base) {
                    {
                        std::unique_lock<std::shared_mutex> cache_lock(cache_mutex);
                        if (page_protection_cache.size() < 10000) {
                            page_protection_cache[page_addr] = region.protection;
                        }
                    }
                    return region.writable;
                }
            }
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        SIZE_T result = VirtualQueryEx(current_handle, (LPCVOID)address, &mbi, sizeof(mbi));
        if (result == sizeof(mbi) && mbi.RegionSize > 0) {
            {
                std::unique_lock<std::shared_mutex> lock(cache_mutex);
                if (page_protection_cache.size() < 10000) {
                    page_protection_cache[page_addr] = mbi.Protect;
                }
            }
            return (mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY)) != 0;
        }

        return false;
    }

    inline bool change_protection_if_needed(uintptr_t address, size_t size, DWORD& old_protect) {
        HANDLE current_handle = roblox_h.load();
        if (!current_handle || address == 0 || size == 0 || size > 0x100000) return false;

        if (is_address_writable_fast(address)) {
            return false;
        }

        BOOL result = VirtualProtectEx(current_handle, (LPVOID)address, size, PAGE_EXECUTE_READWRITE, &old_protect);
        return result != FALSE;
    }

    inline bool enable_debug_privilege() {
        std::cout << "[PRIV] Attempting to enable SeDebugPrivilege..." << std::endl;

        HANDLE hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            std::cout << "[PRIV] OpenProcessToken failed: " << GetLastError() << std::endl;
            return false;
        }

        LUID luid;
        if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &luid)) {
            std::cout << "[PRIV] LookupPrivilegeValue failed: " << GetLastError() << std::endl;
            CloseHandle(hToken);
            return false;
        }

        TOKEN_PRIVILEGES tp;
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
        DWORD error = GetLastError();

        CloseHandle(hToken);

        if (!result || error == ERROR_NOT_ALL_ASSIGNED) {
            std::cout << "[PRIV] SeDebugPrivilege not available (error: " << error << ")" << std::endl;
            return false;
        }

        std::cout << "[PRIV] SeDebugPrivilege enabled successfully" << std::endl;
        return true;
    }

    inline bool test_handle_functionality(HANDLE handle, DWORD pid) {
        if (!handle || handle == INVALID_HANDLE_VALUE) return false;

        std::cout << "[TEST] Testing handle functionality for PID " << std::hex << pid << std::dec << "..." << std::endl;

        DWORD testPid = GetProcessId(handle);
        if (testPid != pid) {
            std::cout << "[TEST] GetProcessId: MISMATCH (got " << std::hex << testPid << ", expected " << pid << std::dec << ")" << std::endl;
            return false;
        }
        std::cout << "[TEST] GetProcessId: VERIFIED (" << std::hex << testPid << std::dec << ")" << std::endl;

        MEMORY_BASIC_INFORMATION mbi;
        bool vqSuccess = VirtualQueryEx(handle, (LPCVOID)0x10000, &mbi, sizeof(mbi));
        if (vqSuccess) {
            std::cout << "[TEST] VirtualQueryEx: SUCCESS (Base: 0x" << std::hex << mbi.BaseAddress << std::dec << ")" << std::endl;
        }
        else {
            std::cout << "[TEST] VirtualQueryEx: FAILED (" << std::hex << GetLastError() << std::dec << ")" << std::endl;
        }

        bool readSuccess = false;
        std::vector<LPCVOID> testAddresses = {
            (LPCVOID)0x10000,
            (LPCVOID)0x20000,
            (LPCVOID)0x30000,
            (LPCVOID)base_address,
            (LPCVOID)(base_address + 0x1000),
            (LPCVOID)mbi.BaseAddress
        };

        for (auto addr : testAddresses) {
            if (!addr) continue;

            char testBuffer[4];
            SIZE_T bytesRead = 0;

            if (ReadProcessMemory(handle, addr, testBuffer, sizeof(testBuffer), &bytesRead)) {
                if (bytesRead > 0) {
                    std::cout << "[TEST] ReadProcessMemory: SUCCESS at 0x" << std::hex << addr << " (" << std::dec << bytesRead << " bytes)" << std::endl;
                    readSuccess = true;
                    break;
                }
            }
        }

        if (!readSuccess) {
            DWORD readError = GetLastError();
            std::cout << "[TEST] ReadProcessMemory: FAILED on all addresses (error: " << std::hex << readError << std::dec;
            switch (readError) {
            case 5: std::cout << " ACCESS_DENIED"; break;
            case 6: std::cout << " INVALID_HANDLE"; break;
            case 87: std::cout << " INVALID_PARAMETER"; break;
            case 299: std::cout << " PARTIAL_COPY"; break;
            case 487: std::cout << " INVALID_ADDRESS"; break;
            default: break;
            }
            std::cout << ")" << std::endl;

            std::cout << "[TEST] Trying NtReadVirtualMemory..." << std::endl;
            HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
            if (hNtdll) {
                typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE, PVOID, PVOID, ULONG, PULONG);
                auto NtReadVirtualMemory = (pNtReadVirtualMemory)GetProcAddress(hNtdll, "NtReadVirtualMemory");

                if (NtReadVirtualMemory) {
                    for (auto addr : testAddresses) {
                        if (!addr) continue;

                        char testBuffer[4];
                        ULONG bytesRead = 0;

                        NTSTATUS status = NtReadVirtualMemory(handle, (PVOID)addr, testBuffer, sizeof(testBuffer), &bytesRead);
                        if (status == 0 && bytesRead > 0) {
                            std::cout << "[TEST] NtReadVirtualMemory: SUCCESS at 0x" << std::hex << addr << " (" << std::dec << bytesRead << " bytes)" << std::endl;
                            readSuccess = true;
                            break;
                        }
                    }

                    if (!readSuccess) {
                        std::cout << "[TEST] NtReadVirtualMemory: FAILED on all addresses" << std::endl;
                    }
                }
                else {
                    std::cout << "[TEST] NtReadVirtualMemory not available" << std::endl;
                }
            }
        }

        bool writeTest = false;
        for (auto addr : testAddresses) {
            if (!addr) continue;

            DWORD oldProtect;
            if (VirtualProtectEx(handle, (LPVOID)addr, 4, PAGE_READWRITE, &oldProtect)) {
                std::cout << "[TEST] VirtualProtectEx: SUCCESS at 0x" << std::hex << addr << std::dec << std::endl;
                VirtualProtectEx(handle, (LPVOID)addr, 4, oldProtect, &oldProtect);
                writeTest = true;
                break;
            }
        }

        if (!writeTest) {
            DWORD protError = GetLastError();
            std::cout << "[TEST] VirtualProtectEx: FAILED (" << std::hex << protError << std::dec;
            switch (protError) {
            case 5: std::cout << " ACCESS_DENIED"; break;
            case 487: std::cout << " INVALID_ADDRESS"; break;
            case 998: std::cout << " INVALID_ACCESS"; break;
            default: break;
            }
            std::cout << ")" << std::endl;
        }

        PROCESS_BASIC_INFORMATION pbi;
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (hNtdll) {
            typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE, ULONG, PVOID, ULONG, PULONG);
            auto NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(hNtdll, "NtQueryInformationProcess");

            if (NtQueryInformationProcess) {
                ULONG returnLength;
                NTSTATUS status = NtQueryInformationProcess(handle, 0, &pbi, sizeof(pbi), &returnLength);
                if (status == 0) {
                    std::cout << "[TEST] NtQueryInformationProcess: SUCCESS (PEB: 0x" << std::hex << pbi.PebBaseAddress << std::dec << ")" << std::endl;
                }
                else {
                    std::cout << "[TEST] NtQueryInformationProcess: FAILED (0x" << std::hex << status << std::dec << ")" << std::endl;
                }
            }
        }

        bool functional = readSuccess || writeTest || vqSuccess;

        if (functional) {
            std::cout << "[TEST] Handle is considered FUNCTIONAL" << std::endl;
        }
        else {
            std::cout << "[TEST] Handle is NOT functional - all tests failed" << std::endl;
        }

        return functional;
    }

    inline bool acquire_handle_through_enumeration() {
        std::lock_guard<std::mutex> lock(handle_mutex);

        std::cout << "[ENUM] Starting system handle enumeration..." << std::endl;
        std::cout << "[ENUM] Target process ID: " << process_id.load() << std::endl;
        std::cout << "[ENUM] Current process ID: " << GetCurrentProcessId() << std::endl;

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) {
            std::cout << "[ENUM] Failed to get ntdll.dll handle" << std::endl;
            return false;
        }

        auto NtQuery = (pNtQuerySystemInformation)GetProcAddress(hNtdll, "NtQuerySystemInformation");
        if (!NtQuery) {
            std::cout << "[ENUM] Failed to get NtQuerySystemInformation address" << std::endl;
            return false;
        }

        std::cout << "[ENUM] NtQuerySystemInformation address: " << (void*)NtQuery << std::endl;

        std::vector<char> buffer(0x100000);
        ULONG requiredSize = 0;
        NTSTATUS status;
        int attempts = 0;

        do {
            attempts++;
            std::cout << "[ENUM] Query attempt " << attempts << ", buffer size: " << buffer.size() << std::endl;

            status = NtQuery(SystemHandleInformation, buffer.data(), (ULONG)buffer.size(), &requiredSize);

            if (status == 0xC0000004) {
                std::cout << "[ENUM] Buffer too small, required: " << requiredSize << std::endl;
                if (requiredSize > 0 && requiredSize < 500 * 1024 * 1024) {
                    buffer.resize(requiredSize + 0x10000);
                }
                else {
                    std::cout << "[ENUM] Required size too large: " << requiredSize << std::endl;
                    return false;
                }
            }
        } while (status == 0xC0000004 && attempts < 5);

        if (status != 0) {
            std::cout << "[ENUM] NtQuerySystemInformation failed: 0x" << std::hex << status << std::endl;
            return false;
        }

        std::cout << "[ENUM] Successfully retrieved handle information" << std::endl;

        auto handleInfo = (SYSTEM_HANDLE_INFORMATION_EX*)buffer.data();
        std::cout << "[ENUM] Total system handles: " << handleInfo->NumberOfHandles << std::endl;

        if (handleInfo->NumberOfHandles == 0 || handleInfo->NumberOfHandles > 5000000) {
            std::cout << "[ENUM] Suspicious handle count: " << handleInfo->NumberOfHandles << std::endl;
            return false;
        }

        ULONG totalProcessed = 0;
        ULONG validHandles = 0;
        ULONG accessibleProcesses = 0;
        ULONG duplicateSuccess = 0;
        ULONG targetMatches = 0;
        DWORD currentPID = GetCurrentProcessId();
        INT32 target_pid = process_id.load();

        for (auto& candidate : handle_candidates) {
            if (candidate.duplicatedHandle) {
                CloseHandle(candidate.duplicatedHandle);
            }
        }
        handle_candidates.clear();

        std::cout << "[ENUM] Processing handles..." << std::endl;

        DWORD accessLevels[] = {
            PROCESS_ALL_ACCESS,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE,
            PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
        };

        for (DWORD requiredAccess : accessLevels) {
            std::cout << "[ENUM] Checking handles with access level: 0x" << std::hex << requiredAccess << std::dec << std::endl;

            for (ULONG i = 0; i < handleInfo->NumberOfHandles; i++) {
                auto& handle = handleInfo->Information[i];
                totalProcessed++;

                if (handle.ProcessId == 0 || handle.ProcessId == currentPID) continue;

                if (i % 25000 == 0 && i > 0) {
                    std::cout << "[PROGRESS] " << i << "/" << handleInfo->NumberOfHandles << " (" << targetMatches << " matches)" << std::endl;
                }

                validHandles++;

                if ((handle.GrantedAccess & requiredAccess) != requiredAccess) continue;

                HANDLE sourceProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, handle.ProcessId);
                if (!sourceProcess) continue;

                accessibleProcesses++;

                HANDLE duplicatedHandle;
                BOOL dupResult = DuplicateHandle(
                    sourceProcess,
                    (HANDLE)handle.Handle,
                    GetCurrentProcess(),
                    &duplicatedHandle,
                    0,
                    FALSE,
                    DUPLICATE_SAME_ACCESS
                );

                CloseHandle(sourceProcess);

                if (!dupResult) continue;

                duplicateSuccess++;

                DWORD targetPID = GetProcessId(duplicatedHandle);
                if (targetPID == (DWORD)target_pid) {
                    targetMatches++;
                    std::cout << "[MATCH] Found handle #" << targetMatches << " from PID " << handle.ProcessId << std::endl;
                    std::cout << "[MATCH]   Handle: 0x" << std::hex << handle.Handle << std::endl;
                    std::cout << "[MATCH]   Access: 0x" << handle.GrantedAccess << std::endl;
                    std::cout << "[MATCH]   Object Type: " << (int)handle.ObjectTypeNumber << std::endl;

                    HandleCandidate candidate;
                    candidate.sourcePID = handle.ProcessId;
                    candidate.duplicatedHandle = duplicatedHandle;
                    candidate.accessRights = handle.GrantedAccess;
                    candidate.tested = false;
                    candidate.functional = false;

                    handle_candidates.push_back(candidate);
                }
                else {
                    CloseHandle(duplicatedHandle);
                }
            }

            if (!handle_candidates.empty()) {
                std::cout << "[ENUM] Found " << handle_candidates.size() << " candidates with access 0x" << std::hex << requiredAccess << std::dec << std::endl;
                break;
            }
        }

        std::cout << "[ENUM] Enumeration complete:" << std::endl;
        std::cout << "[ENUM]   Total processed: " << totalProcessed << std::endl;
        std::cout << "[ENUM]   Valid handles: " << validHandles << std::endl;
        std::cout << "[ENUM]   Accessible processes: " << accessibleProcesses << std::endl;
        std::cout << "[ENUM]   Successful duplications: " << duplicateSuccess << std::endl;
        std::cout << "[ENUM]   Target matches: " << targetMatches << std::endl;
        std::cout << "[ENUM]   Candidates stored: " << handle_candidates.size() << std::endl;

        return !handle_candidates.empty();
    }

    inline bool select_best_handle() {
        std::lock_guard<std::mutex> lock(handle_mutex);

        if (handle_candidates.empty()) {
            std::cout << "[SELECT] No handle candidates available" << std::endl;
            return false;
        }

        std::cout << "[SELECT] Testing " << handle_candidates.size() << " candidates..." << std::endl;

        HandleCandidate* bestCandidate = nullptr;
        int bestScore = -1;
        INT32 target_pid = process_id.load();

        for (auto& candidate : handle_candidates) {
            std::cout << "[SELECT] Testing candidate from PID " << candidate.sourcePID << "..." << std::endl;

            candidate.tested = true;
            candidate.functional = test_handle_functionality(candidate.duplicatedHandle, target_pid);

            if (candidate.functional) {
                int score = __popcnt(candidate.accessRights);

                std::cout << "[SELECT] Functional candidate found, score: " << score << std::endl;

                if (score > bestScore) {
                    bestScore = score;
                    bestCandidate = &candidate;
                }
            }
        }

        if (bestCandidate) {
            std::cout << "[SELECT] Best candidate: PID " << bestCandidate->sourcePID << " (score: " << bestScore << ")" << std::endl;

            HANDLE old_handle = roblox_h.exchange(bestCandidate->duplicatedHandle);
            if (old_handle && old_handle != INVALID_HANDLE_VALUE) {
                CloseHandle(old_handle);
            }

            bestCandidate->duplicatedHandle = nullptr;
            for (auto& candidate : handle_candidates) {
                if (candidate.duplicatedHandle) {
                    CloseHandle(candidate.duplicatedHandle);
                }
            }
            handle_candidates.clear();

            return true;
        }
        else {
            std::cout << "[SELECT] No functional candidates found" << std::endl;
            for (auto& candidate : handle_candidates) {
                if (candidate.duplicatedHandle) {
                    CloseHandle(candidate.duplicatedHandle);
                }
            }
            handle_candidates.clear();

            return false;
        }
    }

    inline void hide_process_traces() {
        std::cout << "[HIDE] Attempting to hide process traces..." << std::endl;

        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) {
            std::cout << "[HIDE] Failed to get ntdll.dll handle" << std::endl;
            return;
        }
        auto NtSetInfo = (pNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");
        if (NtSetInfo) {
            ULONG flag = 1;
            NTSTATUS status = NtSetInfo(GetCurrentProcess(), 0x11, &flag, sizeof(flag));
            std::cout << "[HIDE] ProcessDebugFlags set, status: 0x" << std::hex << status << std::endl;
        }
        auto NtClose = (pNtClose)GetProcAddress(hNtdll, "NtClose");
        if (NtClose) {
            HANDLE testHandle = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, process_id.load());
            if (testHandle) {
                NtClose(testHandle);
                std::cout << "[HIDE] Closed test handle" << std::endl;
            }
        }
    }

    inline bool grabroblox_h() {
        std::cout << "[MAIN] Starting advanced handle acquisition..." << std::endl;

        bool hasDebugPriv = enable_debug_privilege();
        initialize_nt_functions();

        std::cout << "[MAIN] Attempting direct access..." << std::endl;
        DWORD accessLevels[] = {
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
            PROCESS_VM_READ | PROCESS_VM_WRITE,
            PROCESS_ALL_ACCESS
        };

        INT32 target_pid = process_id.load();
        for (DWORD access : accessLevels) {
            HANDLE directHandle = OpenProcess(access, FALSE, target_pid);
            if (directHandle && directHandle != INVALID_HANDLE_VALUE) {
                std::cout << "[MAIN] Direct access successful with 0x" << std::hex << access << std::endl;

                if (test_handle_functionality(directHandle, target_pid)) {
                    HANDLE old_handle = roblox_h.exchange(directHandle);
                    if (old_handle && old_handle != INVALID_HANDLE_VALUE) {
                        CloseHandle(old_handle);
                    }
                    update_memory_regions();
                    hide_process_traces();
                    std::cout << "[MAIN] Direct access method successful!" << std::endl;
                    return true;
                }
                else {
                    CloseHandle(directHandle);
                }
            }
        }

        std::cout << "[MAIN] Direct access failed, trying handle enumeration..." << std::endl;
        if (acquire_handle_through_enumeration()) {
            if (select_best_handle()) {
                update_memory_regions();
                hide_process_traces();
                std::cout << "[MAIN] Handle enumeration method successful!" << std::endl;
                return true;
            }
        }

        std::cout << "[MAIN] All methods failed" << std::endl;
        return false;
    }

    inline void initialize() {
        std::cout << "[INIT] Initializing process detection..." << std::endl;

        HWND robloxWindow = FindWindowA(nullptr, "Roblox");
        if (!robloxWindow) {
            std::cout << "[INIT] Roblox window not found" << std::endl;
            return;
        }

        DWORD windowPID = 0;
        GetWindowThreadProcessId(robloxWindow, &windowPID);
        std::cout << "[INIT] Roblox window PID: " << windowPID << std::endl;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, windowPID);
        if (snapshot == INVALID_HANDLE_VALUE) {
            std::cout << "[INIT] Failed to create module snapshot" << std::endl;
            return;
        }

        MODULEENTRY32 moduleEntry;
        moduleEntry.dwSize = sizeof(moduleEntry);

        bool foundRoblox = false;
        if (Module32First(snapshot, &moduleEntry)) {
            do {
                std::cout << "[INIT] Module: " << moduleEntry.szModule << std::endl;
                if (strcmp(moduleEntry.szModule, "RobloxPlayerBeta.exe") == 0) {
                    process_id.store(moduleEntry.th32ProcessID);
                    base_address = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
                    foundRoblox = true;
                    std::cout << "[INIT] Found RobloxPlayerBeta.exe" << std::endl;
                    std::cout << "[INIT]   PID: " << process_id.load() << std::endl;
                    std::cout << "[INIT]   Base: 0x" << std::hex << base_address << std::endl;
                    break;
                }
            } while (Module32Next(snapshot, &moduleEntry));
        }

        CloseHandle(snapshot);

        if (!foundRoblox) {
            std::cout << "[INIT] RobloxPlayerBeta.exe not found in process modules" << std::endl;
        }
    }

    inline void wait_for_safe_address(std::uintptr_t address) {
        HANDLE current_handle = roblox_h.load();
        if (!current_handle) return;

        auto startTime = GetTickCount64();
        while (GetTickCount64() - startTime < 10) {
            PSAPI_WORKING_SET_EX_INFORMATION wsInfo = {};
            wsInfo.VirtualAddress = (PVOID)address;

            if (K32QueryWorkingSetEx(current_handle, &wsInfo, sizeof(wsInfo))) {
                if (wsInfo.VirtualAttributes.Valid) {
                    break;
                }
            }
            Sleep(1);
        }
    }

    inline bool is_address_guarded(uintptr_t address) {
        HANDLE current_handle = roblox_h.load();
        if (!current_handle) return false;

        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQueryEx(current_handle, (LPCVOID)address, &mbi, sizeof(mbi)) != sizeof(mbi)) {
            return false;
        }

        return (mbi.Protect & PAGE_GUARD) != 0;
    }

    inline bool write_memory_batch(const std::vector<std::pair<uintptr_t, std::vector<uint8_t>>>& writes) {
        HANDLE current_handle = roblox_h.load();
        if (!current_handle || writes.empty()) return false;

        bool success = true;
        update_memory_regions();

        for (const auto& write_op : writes) {
            uintptr_t address = write_op.first;
            const auto& data = write_op.second;

            if (address == 0 || data.empty() || data.size() > 0x100000) {
                success = false;
                continue;
            }

            DWORD old_protect;
            bool protection_changed = change_protection_if_needed(address, data.size(), old_protect);

            SIZE_T bytes_written = 0;
            bool write_success = false;

            if (use_nt_functions.load() && NtWriteVirtualMemory) {
                ULONG nt_bytes_written = 0;
                NTSTATUS status = NtWriteVirtualMemory(current_handle, (PVOID)address, (PVOID)data.data(), (ULONG)data.size(), &nt_bytes_written);
                write_success = (status == 0 && nt_bytes_written == data.size());
            }
            else {
                write_success = WriteProcessMemory(current_handle, (PVOID)address, data.data(), data.size(), &bytes_written);
            }

            if (protection_changed) {
                DWORD temp;
                VirtualProtectEx(current_handle, (LPVOID)address, data.size(), old_protect, &temp);
            }

            if (!write_success) {
                success = false;
            }
        }

        return success;
    }
}

inline bool is_valid_address(uintptr_t address) {
    return address != 0 && address >= 0x10000 && address < 0x7FFFFFFFFFFF;
}

template <typename T>
T read(uint64_t address) {
    T buffer{};
    HANDLE current_handle = mem::roblox_h.load();
    if (!current_handle || address == 0) return buffer;

    SIZE_T bytes_read = 0;

    if (mem::use_nt_functions.load() && mem::NtReadVirtualMemory) {
        ULONG nt_bytes_read = 0;
        NTSTATUS status = mem::NtReadVirtualMemory(current_handle, (PVOID)address, &buffer, sizeof(T), &nt_bytes_read);
        if (status == 0 && nt_bytes_read == sizeof(T)) {
            return buffer;
        }
    }

    if (ReadProcessMemory(current_handle, (PVOID)address, &buffer, sizeof(T), &bytes_read)) {
        return buffer;
    }

    return T{};
}

template <typename T>
bool write(uint64_t address, const T& buffer) {
    HANDLE current_handle = mem::roblox_h.load();
    if (!current_handle || address == 0) return false;

    if (sizeof(T) > 0x1000) return false;

    mem::update_memory_regions();

    DWORD old_protect = 0;
    bool protection_changed = mem::change_protection_if_needed(address, sizeof(T), old_protect);

    bool success = false;

    if (mem::use_nt_functions.load() && mem::NtWriteVirtualMemory) {
        ULONG bytes_written = 0;
        NTSTATUS status = mem::NtWriteVirtualMemory(current_handle, (PVOID)address, (PVOID)&buffer, sizeof(T), &bytes_written);
        success = (status == 0 && bytes_written == sizeof(T));
    }
    else {
        SIZE_T bytes_written = 0;
        success = WriteProcessMemory(current_handle, (PVOID)address, &buffer, sizeof(T), &bytes_written) != FALSE;
    }

    if (protection_changed && old_protect != 0) {
        DWORD temp = 0;
        VirtualProtectEx(current_handle, (LPVOID)address, sizeof(T), old_protect, &temp);
    }

    return success;
}

template <typename T>
bool write_fast(uint64_t address, const T& buffer) {
    HANDLE current_handle = mem::roblox_h.load();
    if (!current_handle || address == 0) return false;

    if (sizeof(T) > 0x1000) return false;

    if (mem::is_address_writable_fast(address)) {
        if (mem::use_nt_functions.load() && mem::NtWriteVirtualMemory) {
            ULONG bytes_written = 0;
            NTSTATUS status = mem::NtWriteVirtualMemory(current_handle, (PVOID)address, (PVOID)&buffer, sizeof(T), &bytes_written);
            return (status == 0 && bytes_written == sizeof(T));
        }
        else {
            SIZE_T bytes_written = 0;
            return WriteProcessMemory(current_handle, (PVOID)address, &buffer, sizeof(T), &bytes_written) != FALSE;
        }
    }
    return write(address, buffer);
}

template <typename T>
bool write_array(uint64_t address, const T* buffer, size_t count) {
    HANDLE current_handle = mem::roblox_h.load();
    if (!current_handle || !buffer || count == 0 || address == 0) return false;

    size_t total_size = sizeof(T) * count;

    if (total_size > 0x100000) return false;

    mem::update_memory_regions();

    DWORD old_protect = 0;
    bool protection_changed = mem::change_protection_if_needed(address, total_size, old_protect);

    bool success = false;

    if (mem::use_nt_functions.load() && mem::NtWriteVirtualMemory) {
        ULONG bytes_written = 0;
        NTSTATUS status = mem::NtWriteVirtualMemory(current_handle, (PVOID)address, (PVOID)buffer, (ULONG)total_size, &bytes_written);
        success = (status == 0 && bytes_written == total_size);
    }
    else {
        SIZE_T bytes_written = 0;
        success = WriteProcessMemory(current_handle, (PVOID)address, buffer, total_size, &bytes_written) != FALSE;
    }

    if (protection_changed && old_protect != 0) {
        DWORD temp = 0;
        VirtualProtectEx(current_handle, (LPVOID)address, total_size, old_protect, &temp);
    }

    return success;
}