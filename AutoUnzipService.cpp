#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <winsvc.h>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <regex>
#include <chrono>
#include <map>
#include <algorithm>
#include "resource.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")

// Enhanced constants
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_SHOW 1002
#define ID_TRAY_PAUSE 1003
#define ID_TRAY_SETTINGS 1004
#define MAX_PASSWORD_ATTEMPTS 3

// Enhanced AutoUnzipService class with better error handling and 2FA support
class AutoUnzipService {
private:
    HWND hWnd;
    NOTIFYICONDATA nid;
    HANDLE hDirectoryWatcher;
    std::atomic<bool> isRunning{true};
    std::atomic<bool> isPaused{false};
    std::mutex processingMutex;
    std::string peazipPath;
    std::string downloadsPath;
    std::map<std::string, int> passwordAttempts;
    
    // Enhanced supported extensions with better categorization
    std::vector<std::string> supportedExtensions = {
        // Common archives
        ".7z", ".zip", ".rar", ".tar", ".gz", ".bz2", ".xz", 
        // Disk images
        ".iso", ".img", ".dmg", ".vhd", ".vmdk",
        // Legacy formats
        ".cab", ".arj", ".lzh", ".ace", ".uue", ".z", 
        // Compressed tars
        ".taz", ".tbz", ".tbz2", ".txz", ".tlz",
        // Application packages
        ".war", ".jar", ".ear", ".sar", ".apk", ".ipa",
        // Split archives
        ".001", ".002", ".003", ".part1", ".part2",
        // Other formats
        ".lzma", ".zipx", ".par", ".par2", ".deb", ".rpm",
        // Backup formats
        ".bak", ".backup", ".arc"
    };
    
    std::vector<std::string> conventionalExtensions = {
        ".zip", ".rar", ".7z", ".tar", ".gz", ".bz2"
    };

public:
    AutoUnzipService() {
        CoInitialize(NULL);
        InitializePaths();
        CreateTrayIcon();
        StartDirectoryWatcher();
        LogEvent("Auto Unzip Service started successfully");
    }
    
    ~AutoUnzipService() {
        Cleanup();
        CoUninitialize();
    }
    
    void InitializePaths() {
        // Enhanced PeaZip detection with multiple registry locations
        std::vector<std::string> registryPaths = {
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PeaZip",
            "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PeaZip",
            "SOFTWARE\\PeaZip"
        };
        
        for (const auto& regPath : registryPaths) {
            HKEY hKey;
            char buffer[MAX_PATH];
            DWORD bufferSize = sizeof(buffer);
            
            if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                if (RegQueryValueExA(hKey, "InstallLocation", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                    peazipPath = std::string(buffer) + "\\peazip.exe";
                    RegCloseKey(hKey);
                    break;
                }
                RegCloseKey(hKey);
            }
        }
        
        // Enhanced fallback paths including portable versions
        if (peazipPath.empty() || !std::filesystem::exists(peazipPath)) {
            std::vector<std::string> possiblePaths = {
                "C:\\Program Files\\PeaZip\\peazip.exe",
                "C:\\Program Files (x86)\\PeaZip\\peazip.exe",
                "C:\\PeaZip\\peazip.exe",
                "C:\\Tools\\PeaZip\\peazip.exe"
            };
            
            // Get current directory
            char currentDir[MAX_PATH];
            GetModuleFileNameA(NULL, currentDir, MAX_PATH);
            std::string currentDirStr(currentDir);
            currentDirStr = currentDirStr.substr(0, currentDirStr.find_last_of("\\/"));
            possiblePaths.push_back(currentDirStr + "\\PeaZip\\peazip.exe");
            
            for (const auto& path : possiblePaths) {
                if (std::filesystem::exists(path)) {
                    peazipPath = path;
                    break;
                }
            }
        }
        
        // Get Downloads folder with fallback
        char downloadsBuffer[MAX_PATH];
        if (SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, downloadsBuffer) == S_OK) {
            downloadsPath = std::string(downloadsBuffer) + "\\Downloads";
        } else {
            // Fallback to current user profile
            char* userProfile = getenv("USERPROFILE");
            if (userProfile) {
                downloadsPath = std::string(userProfile) + "\\Downloads";
            }
        }
        
        // Verify paths exist
        if (!std::filesystem::exists(downloadsPath)) {
            std::filesystem::create_directories(downloadsPath);
        }
    }
    
    void CreateTrayIcon() {
        // Load icon with fallback
        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_TRAY_ICON));
        if (!hIcon) {
            hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }
        
        ZeroMemory(&nid, sizeof(NOTIFYICONDATA));
        nid.cbSize = sizeof(NOTIFYICONDATA);
        nid.hWnd = hWnd;
        nid.uID = 1;
        nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid.uCallbackMessage = WM_TRAYICON;
        nid.hIcon = hIcon;
        lstrcpyA(nid.szTip, "Auto Unzip Service - Monitoring Downloads");
        
        Shell_NotifyIcon(NIM_ADD, &nid);
    }
    
    void StartDirectoryWatcher() {
        std::thread([this]() {
            HANDLE hDir = CreateFileA(
                downloadsPath.c_str(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                NULL
            );
            
            if (hDir == INVALID_HANDLE_VALUE) {
                LogEvent("Failed to open Downloads directory for monitoring");
                return;
            }
            
            char buffer[4096]; // Increased buffer size
            DWORD bytesReturned;
            OVERLAPPED overlapped = {0};
            overlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            
            LogEvent("Started monitoring: " + downloadsPath);
            
            while (isRunning) {
                if (ReadDirectoryChangesW(
                    hDir,
                    buffer,
                    sizeof(buffer),
                    FALSE,
                    FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_SIZE,
                    &bytesReturned,
                    &overlapped,
                    NULL
                )) {
                    DWORD waitResult = WaitForSingleObject(overlapped.hEvent, 1000);
                    
                    if (waitResult == WAIT_OBJECT_0 && !isPaused && isRunning) {
                        ProcessDirectoryChanges(buffer, bytesReturned);
                    }
                } else {
                    // Handle error and attempt to restart monitoring
                    Sleep(5000);
                    LogEvent("Directory monitoring error, attempting restart...");
                }
            }
            
            CloseHandle(overlapped.hEvent);
            CloseHandle(hDir);
        }).detach();
    }
    
    void ProcessDirectoryChanges(char* buffer, DWORD bytesReturned) {
        std::lock_guard<std::mutex> lock(processingMutex);
        
        FILE_NOTIFY_INFORMATION* pNotify = (FILE_NOTIFY_INFORMATION*)buffer;
        
        do {
            if (pNotify->Action == FILE_ACTION_ADDED || pNotify->Action == FILE_ACTION_RENAMED_NEW_NAME) {
                std::wstring filename(pNotify->FileName, pNotify->FileNameLength / sizeof(WCHAR));
                std::string filenameStr = WStringToString(filename);
                
                std::string fullPath = downloadsPath + "\\" + filenameStr;
                
                // Wait for file to be completely written and stable
                if (WaitForFileStable(fullPath)) {
                    if (IsArchiveFile(filenameStr)) {
                        LogEvent("Detected archive: " + filenameStr);
                        ProcessArchiveFile(fullPath, filenameStr);
                    }
                }
            }
            
            if (pNotify->NextEntryOffset == 0) break;
            pNotify = (FILE_NOTIFY_INFORMATION*)((char*)pNotify + pNotify->NextEntryOffset);
            
        } while (true);
    }
    
    std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
        std::string result(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, NULL, NULL);
        return result;
    }
    
    bool WaitForFileStable(const std::string& filePath) {
        // Wait for file to be stable (not being written to)
        for (int i = 0; i < 10; i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, 
                                      NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                CloseHandle(hFile);
                return true;
            }
        }
        return false;
    }
    
    bool IsArchiveFile(const std::string& filename) {
        std::string lowerFilename = filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
        
        for (const auto& ext : supportedExtensions) {
            if (lowerFilename.ends_with(ext)) {
                return true;
            }
        }
        
        // Check for numbered extensions (.001, .002, etc.)
        std::regex numberedPattern(R"(\.(\d{3})$)");
        return std::regex_search(lowerFilename, numberedPattern);
    }
    
    bool IsConventionalArchive(const std::string& filename) {
        std::string lowerFilename = filename;
        std::transform(lowerFilename.begin(), lowerFilename.end(), lowerFilename.begin(), ::tolower);
        
        for (const auto& ext : conventionalExtensions) {
            if (lowerFilename.ends_with(ext)) {
                return true;
            }
        }
        return false;
    }
    
    void ProcessArchiveFile(const std::string& filePath, const std::string& filename) {
        // For non-conventional archives, prompt user
        if (!IsConventionalArchive(filename)) {
            std::string message = "Do you want to extract the archive: " + filename + "?\n\n"
                                "File path: " + filePath + "\n"
                                "This is a non-standard archive format.";
            
            int result = MessageBoxA(NULL, message.c_str(),
                                   "Auto Unzip - Confirmation Required",
                                   MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
            
            if (result != IDYES) {
                LogEvent("User declined to extract: " + filename);
                return;
            }
        }
        
        // Reset password attempts for new file
        passwordAttempts[filename] = 0;
        
        // Try to extract without password first
        if (!ExtractArchive(filePath, "", "")) {
            // If failed, prompt for password
            PasswordDialogData data = PromptForPassword(filename);
            if (!data.cancelled && !data.password.empty()) {
                ExtractArchive(filePath, data.password, data.twoFactorCode);
            }
        }
    }
    
    struct PasswordDialogData {
        std::string filename;
        std::string password;
        std::string twoFactorCode;
        bool cancelled = false;
    };
    
    PasswordDialogData PromptForPassword(const std::string& filename) {
        PasswordDialogData data;
        data.filename = filename;
        
        // Check if we've exceeded maximum attempts
        if (passwordAttempts[filename] >= MAX_PASSWORD_ATTEMPTS) {
            MessageBoxA(NULL, 
                       ("Maximum password attempts exceeded for: " + filename).c_str(),
                       "Auto Unzip - Error", 
                       MB_OK | MB_ICONERROR | MB_TOPMOST);
            data.cancelled = true;
            return data;
        }
        
        passwordAttempts[filename]++;
        
        INT_PTR result = DialogBoxParam(
            GetModuleHandle(NULL),
            MAKEINTRESOURCE(IDD_PASSWORD_DIALOG),
            NULL,
            PasswordDialogProc,
            (LPARAM)&data
        );
        
        return data;
    }
    
    bool ExtractArchive(const std::string& archivePath, const std::string& password, const std::string& twoFactorCode) {
        if (peazipPath.empty()) {
            ShowTrayNotification("Auto Unzip - Error", "PeaZip not found. Please install PeaZip.");
            LogEvent("PeaZip not found at expected locations");
            return false;
        }
        
        // Create output directory
        std::string outputDir = archivePath.substr(0, archivePath.find_last_of('.'));
        
        // Build PeaZip command with enhanced options
        std::string command = "\"" + peazipPath + "\" -ext2folder -o+ ";
        
        if (!password.empty()) {
            command += "-pwd \"" + password + "\" ";
        }
        
        // Add 2FA support if provided
        if (!twoFactorCode.empty()) {
            command += "-2fa \"" + twoFactorCode + "\" ";
        }
        
        command += "\"" + archivePath + "\"";
        
        LogEvent("Executing: " + command);
        
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        
        BOOL success = CreateProcessA(
            NULL,
            const_cast<char*>(command.c_str()),
            NULL, NULL, FALSE,
            CREATE_NO_WINDOW,
            NULL, NULL,
            &si, &pi
        );
        
        if (success) {
            // Wait with timeout
            DWORD waitResult = WaitForSingleObject(pi.hProcess, 300000); // 5 minutes timeout
            
            DWORD exitCode = 1;
            if (waitResult == WAIT_OBJECT_0) {
                GetExitCodeProcess(pi.hProcess, &exitCode);
            } else if (waitResult == WAIT_TIMEOUT) {
                TerminateProcess(pi.hProcess, 1);
                LogEvent("Extraction timed out for: " + archivePath);
            }
            
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            
            if (exitCode == 0) {
                std::string filename = std::filesystem::path(archivePath).filename().string();
                ShowTrayNotification("Auto Unzip - Success", ("Extracted: " + filename).c_str());
                LogEvent("Successfully extracted: " + filename);
                
                // Reset password attempts on success
                passwordAttempts.erase(filename);
                return true;
            } else {
                LogEvent("Extraction failed with exit code: " + std::to_string(exitCode));
            }
        } else {
            LogEvent("Failed to start PeaZip process");
        }
        
        return false;
    }
    
    void ShowTrayNotification(const char* title, const char* message) {
        nid.uFlags = NIF_INFO;
        lstrcpyA(nid.szInfoTitle, title);
        lstrcpyA(nid.szInfo, message);
        nid.dwInfoFlags = NIIF_INFO;
        nid.uTimeout = 5000;
        Shell_NotifyIcon(NIM_MODIFY, &nid);
    }
    
    void LogEvent(const std::string& message) {
        // Get current directory for log file
        char currentDir[MAX_PATH];
        GetModuleFileNameA(NULL, currentDir, MAX_PATH);
        std::string currentDirStr(currentDir);
        currentDirStr = currentDirStr.substr(0, currentDirStr.find_last_of("\\/"));
        
        std::string logPath = currentDirStr + "\\AutoUnzipService.log";
        std::ofstream logFile(logPath, std::ios::app);
        if (logFile.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            char timeStr[100];
            std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
            
            logFile << "[" << timeStr << "] " << message << std::endl;
            logFile.close();
        }
    }
    
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        AutoUnzipService* service = (AutoUnzipService*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        
        switch (uMsg) {
            case WM_CREATE:
                SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT*)lParam)->lpCreateParams);
                break;
                
            case WM_TRAYICON:
                if (lParam == WM_RBUTTONUP) {
                    POINT pt;
                    GetCursorPos(&pt);
                    
                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuA(hMenu, MF_STRING, ID_TRAY_SHOW, "Show Status");
                    AppendMenuA(hMenu, MF_STRING, ID_TRAY_PAUSE, 
                              service->isPaused ? "Resume Monitoring" : "Pause Monitoring");
                    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuA(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit Service");
                    
                    SetForegroundWindow(hwnd);
                    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                    DestroyMenu(hMenu);
                }
                break;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case ID_TRAY_EXIT:
                        service->LogEvent("Service shutdown requested by user");
                        service->isRunning = false;
                        PostQuitMessage(0);
                        break;
                        
                    case ID_TRAY_PAUSE:
                        service->isPaused = !service->isPaused;
                        service->ShowTrayNotification("Auto Unzip Service", 
                            service->isPaused ? "Monitoring Paused" : "Monitoring Resumed");
                        service->LogEvent(service->isPaused ? "Service paused" : "Service resumed");
                        break;
                        
                    case ID_TRAY_SHOW: {
                        std::string status = "Auto Unzip Service Status\n\n";
                        status += "Status: " + std::string(service->isPaused ? "Paused" : "Running") + "\n";
                        status += "Monitoring: " + service->downloadsPath + "\n";
                        status += "PeaZip Path: " + (service->peazipPath.empty() ? "Not Found" : service->peazipPath) + "\n";
                        
                        // Get log path
                        char currentDir[MAX_PATH];
                        GetModuleFileNameA(NULL, currentDir, MAX_PATH);
                        std::string currentDirStr(currentDir);
                        currentDirStr = currentDirStr.substr(0, currentDirStr.find_last_of("\\/"));
                        status += "Log File: " + currentDirStr + "\\AutoUnzipService.log";
                        
                        MessageBoxA(hwnd, status.c_str(), "Service Status", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
                        break;
                    }
                }
                break;
                
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
                
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        return 0;
    }
    
    static INT_PTR CALLBACK PasswordDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
        static PasswordDialogData* data = nullptr;
        
        switch (message) {
            case WM_INITDIALOG:
                data = (PasswordDialogData*)lParam;
                SetWindowTextA(GetDlgItem(hDlg, IDC_FILENAME_STATIC), 
                             ("Archive: " + data->filename).c_str());
                SetFocus(GetDlgItem(hDlg, IDC_PASSWORD_EDIT));
                
                // Set dialog to be topmost
                SetWindowPos(hDlg, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
                return FALSE;
                
            case WM_COMMAND:
                switch (LOWORD(wParam)) {
                    case IDC_PASSWORD_OK: {
                        char password[256];
                        char twoFactor[64];
                        
                        GetWindowTextA(GetDlgItem(hDlg, IDC_PASSWORD_EDIT), password, sizeof(password));
                        GetWindowTextA(GetDlgItem(hDlg, IDC_2FA_EDIT), twoFactor, sizeof(twoFactor));
                        
                        data->password = password;
                        data->twoFactorCode = twoFactor;
                        data->cancelled = false;
                        EndDialog(hDlg, LOWORD(wParam));
                        return TRUE;
                    }
                    
                    case IDC_PASSWORD_SKIP:
                    case IDCANCEL:
                        data->cancelled = true;
                        EndDialog(hDlg, LOWORD(wParam));
                        return TRUE;
                }
                break;
        }
        return FALSE;
    }
    
    void Run() {
        // Register window class
        WNDCLASSA wc = {};
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = "AutoUnzipService";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassA(&wc);
        
        // Create hidden window
        hWnd = CreateWindowA(
            "AutoUnzipService", "Auto Unzip Service",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
            NULL, NULL, GetModuleHandle(NULL), this
        );
        
        if (!hWnd) {
            LogEvent("Failed to create main window");
            return;
        }
        
        // Message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            if (!IsDialogMessage(hWnd, &msg)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    
    void Cleanup() {
        isRunning = false;
        Shell_NotifyIcon(NIM_DELETE, &nid);
        LogEvent("Auto Unzip Service stopped");
    }
};

// Enhanced Service Management
class ServiceManager {
public:
    static bool InstallService() {
        SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
        if (!hSCManager) {
            MessageBoxA(NULL, "Failed to open Service Control Manager", "Error", MB_OK | MB_ICONERROR);
            return false;
        }
        
        char szPath[MAX_PATH];
        GetModuleFileNameA(NULL, szPath, MAX_PATH);
        
        SC_HANDLE hService = CreateServiceA(
            hSCManager,
            "AutoUnzipService",
            "Auto Unzip Service",
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL,
            szPath,
            NULL, NULL, NULL, NULL, NULL
        );
        
        bool success = (hService != NULL);
        
        if (success) {
            // Set service description
            SERVICE_DESCRIPTIONA desc;
            desc.lpDescription = (LPSTR)"Automatically extracts archives in the Downloads folder using PeaZip";
            ChangeServiceConfig2A(hService, SERVICE_CONFIG_DESCRIPTION, &desc);
            
            CloseServiceHandle(hService);
            MessageBoxA(NULL, "Service installed successfully!\nThe service will start automatically on system boot.", 
                       "Auto Unzip Service", MB_OK | MB_ICONINFORMATION);
        } else {
            DWORD error = GetLastError();
            if (error == ERROR_SERVICE_EXISTS) {
                MessageBoxA(NULL, "Service is already installed!", "Auto Unzip Service", MB_OK | MB_ICONWARNING);
            } else {
                MessageBoxA(NULL, "Failed to install service!", "Auto Unzip Service", MB_OK | MB_ICONERROR);
            }
        }
        
        CloseServiceHandle(hSCManager);
        return success;
    }
    
    static bool UninstallService() {
        SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (!hSCManager) return false;
        
        SC_HANDLE hService = OpenServiceA(hSCManager, "AutoUnzipService", DELETE | SERVICE_STOP);
        if (!hService) {
            CloseServiceHandle(hSCManager);
            MessageBoxA(NULL, "Service not found or access denied!", "Auto Unzip Service", MB_OK | MB_ICONERROR);
            return false;
        }
        
        // Stop the service first
        SERVICE_STATUS status;
        ControlService(hService, SERVICE_CONTROL_STOP, &status);
        
        bool success = DeleteService(hService);
        
        if (success) {
            MessageBoxA(NULL, "Service uninstalled successfully!", "Auto Unzip Service", MB_OK | MB_ICONINFORMATION);
        } else {
            MessageBoxA(NULL, "Failed to uninstall service!", "Auto Unzip Service", MB_OK | MB_ICONERROR);
        }
        
        CloseServiceHandle(hService);
        CloseServiceHandle(hSCManager);
        
        return success;
    }
};

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Parse command line arguments
    if (strstr(lpCmdLine, "-install")) {
        return ServiceManager::InstallService() ? 0 : 1;
    }
    
    if (strstr(lpCmdLine, "-uninstall")) {
        return ServiceManager::UninstallService() ? 0 : 1;
    }
    
    if (strstr(lpCmdLine, "-help") || strstr(lpCmdLine, "/?")) {
        MessageBoxA(NULL, 
                   "Auto Unzip Service - Command Line Options:\n\n"
                   "-install    Install as Windows service\n"
                   "-uninstall  Uninstall Windows service\n"
                   "-help       Show this help message\n\n"
                   "Run without parameters to start in user mode.",
                   "Auto Unzip Service Help", MB_OK | MB_ICONINFORMATION);
        return 0;
    }
    
    // Initialize common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);
    
    // Check if PeaZip is installed using the same logic as the service
    std::string tempPeaZipPath;
    
    std::vector<std::string> registryPaths = {
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PeaZip",
        "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\PeaZip",
        "SOFTWARE\\PeaZip"
    };
    
    for (const auto& regPath : registryPaths) {
        HKEY hKey;
        char buffer[MAX_PATH];
        DWORD bufferSize = sizeof(buffer);
        
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            if (RegQueryValueExA(hKey, "InstallLocation", NULL, NULL, (LPBYTE)buffer, &bufferSize) == ERROR_SUCCESS) {
                tempPeaZipPath = std::string(buffer) + "\\peazip.exe";
                RegCloseKey(hKey);
                break;
            }
            RegCloseKey(hKey);
        }
    }
    
    if (tempPeaZipPath.empty() || !std::filesystem::exists(tempPeaZipPath)) {
        int result = MessageBoxA(NULL, 
                               "PeaZip was not found on your system.\n\n"
                               "Please install PeaZip from https://peazip.github.io/ "
                               "before using this service.\n\n"
                               "Do you want to continue anyway?",
                               "PeaZip Not Found", 
                               MB_YESNO | MB_ICONWARNING);
        if (result == IDNO) {
            return 1;
        }
    }
    
    // Run the service
    try {
        AutoUnzipService service;
        service.Run();
    } catch (const std::exception& e) {
        MessageBoxA(NULL, 
                   ("An error occurred: " + std::string(e.what())).c_str(),
                   "Auto Unzip Service Error", 
                   MB_OK | MB_ICONERROR);
        return 1;
    }
    
    return 0;
}