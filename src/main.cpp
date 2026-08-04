#define _CRT_SECURE_NO_WARNINGS

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

// Windows API 
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <shlobj.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define WM_TRAYICON (WM_USER + 1)
#define IDI_ICON1 101

// Data Structures 
struct Preset {
    char name[32] = "";
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
    int virtualKey = 0;
    float gamma = 1.0f;
    float contrast = 100.0f;
    float brightness = 100.0f;
    bool was_down = false; 
    bool confirming_delete = false; 
};

struct AppSettings {
    bool start_with_windows = false;
    bool minimize_to_tray = false;
    bool dont_reset_on_close = false;
    bool always_on_top = false;
    char custom_save_dir[MAX_PATH] = "";
    float last_gamma = 1.0f;
    float last_contrast = 100.0f;
    float last_brightness = 100.0f;
};

// Global Application State
float cur_gamma = 1.0f, cur_contrast = 100.0f, cur_brightness = 100.0f;
float default_gamma = 1.0f, default_contrast = 100.0f, default_brightness = 100.0f;
std::string active_preset_name = "None";

WORD originalRamp[3][256];
bool hasOriginalRamp = false;

std::vector<Preset> presets;
AppSettings app_settings;

char new_preset_name[32] = "";
bool new_preset_ctrl = false;
bool new_preset_shift = false;
bool new_preset_alt = false;
int new_preset_vk = 0;

bool binding_key = false;
bool binding_just_started = false;
bool initial_key_state[256] = { false };

bool show_default_popup_1 = false;
bool show_default_popup_2 = false;
bool show_duplicate_popup = false;

char save_dir[MAX_PATH] = "";

bool app_open = true;
bool is_minimized = false;

HWND hwnd = NULL;
WNDPROC original_wndproc = NULL;
GLFWwindow* g_window = NULL;

void SaveSettings();
void LoadSettings();
void SavePresets();
void LoadPresets();

// Save and Load Paths
void GetBaseAppDataDirectory(char* pathOut, size_t maxLen) {
    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        snprintf(pathOut, maxLen, "%s\\Gammie", appdata);
        CreateDirectoryA(pathOut, NULL);
    } else {
        snprintf(pathOut, maxLen, ".");
    }
}

void GetActiveSaveDirectory(char* pathOut, size_t maxLen) {
    if (strlen(app_settings.custom_save_dir) > 0) {
        snprintf(pathOut, maxLen, "%s", app_settings.custom_save_dir);
    } else {
        GetBaseAppDataDirectory(pathOut, maxLen);
    }
    CreateDirectoryA(pathOut, NULL);
}

void SaveSettings() {
    char baseDir[MAX_PATH];
    GetBaseAppDataDirectory(baseDir, sizeof(baseDir));
    std::string fullPath = std::string(baseDir) + "\\settings.dat";

    FILE* f = fopen(fullPath.c_str(), "w");
    if (f) {
        fprintf(f, "%d %d %d %d\n", 
            app_settings.start_with_windows, 
            app_settings.minimize_to_tray, 
            app_settings.dont_reset_on_close, 
            app_settings.always_on_top);
        fprintf(f, "%.2f %.2f %.2f\n", 
            app_settings.last_gamma, 
            app_settings.last_contrast, 
            app_settings.last_brightness);
        fprintf(f, "%s\n", app_settings.custom_save_dir);
        fclose(f);
    }
}

void LoadSettings() {
    char baseDir[MAX_PATH];
    GetBaseAppDataDirectory(baseDir, sizeof(baseDir));
    std::string fullPath = std::string(baseDir) + "\\settings.dat";

    FILE* f = fopen(fullPath.c_str(), "r");
    if (f) {
        int sw, mtr, drc, aot;
        if (fscanf(f, "%d %d %d %d", &sw, &mtr, &drc, &aot) == 4) {
            app_settings.start_with_windows = (sw != 0);
            app_settings.minimize_to_tray = (mtr != 0);
            app_settings.dont_reset_on_close = (drc != 0);
            app_settings.always_on_top = (aot != 0);
        }
        if (fscanf(f, "%f %f %f", &app_settings.last_gamma, &app_settings.last_contrast, &app_settings.last_brightness) != 3) {
            app_settings.last_gamma = 1.0f;
            app_settings.last_contrast = 100.0f;
            app_settings.last_brightness = 100.0f;
        }
        char dirBuf[MAX_PATH];
        if (fscanf(f, " %[^\n]", dirBuf) == 1) {
            strncpy(app_settings.custom_save_dir, dirBuf, sizeof(app_settings.custom_save_dir) - 1);
            app_settings.custom_save_dir[sizeof(app_settings.custom_save_dir) - 1] = '\0';
        }
        fclose(f);
    }
}

void SaveDefaultDisplay(float g, float c, float b) {
    GetActiveSaveDirectory(save_dir, sizeof(save_dir));
    std::string fullPath = std::string(save_dir) + "\\default_display.dat";
    FILE* f = fopen(fullPath.c_str(), "w");
    if (f) {
        fprintf(f, "%.2f %.2f %.2f\n", g, c, b);
        fclose(f);
    }
}

void LoadDefaultDisplay() {
    GetActiveSaveDirectory(save_dir, sizeof(save_dir));
    std::string fullPath = std::string(save_dir) + "\\default_display.dat";
    FILE* f = fopen(fullPath.c_str(), "r");
    if (f) {
        float g, c, b;
        if (fscanf(f, "%f %f %f", &g, &c, &b) == 3) {
            default_gamma = g;
            default_contrast = c;
            default_brightness = b;
        }
        fclose(f);
    } else {
        default_gamma = 1.0f;
        default_contrast = 100.0f;
        default_brightness = 100.0f;
        SaveDefaultDisplay(default_gamma, default_contrast, default_brightness);
    }
}

void SavePresets() {
    GetActiveSaveDirectory(save_dir, sizeof(save_dir));
    std::string fullPath = std::string(save_dir) + "\\presets.dat";
    FILE* f = fopen(fullPath.c_str(), "w");
    if (f) {
        fprintf(f, "%zu\n", presets.size());
        for (const auto& p : presets) {
            fprintf(f, "%s\n%d %d %d %d %.2f %.2f %.2f\n", 
                p.name, p.ctrl, p.shift, p.alt, p.virtualKey, p.gamma, p.contrast, p.brightness);
        }
        fclose(f);
    }
}

void LoadPresets() {
    GetActiveSaveDirectory(save_dir, sizeof(save_dir));
    std::string fullPath = std::string(save_dir) + "\\presets.dat";
    FILE* f = fopen(fullPath.c_str(), "r");
    if (f) {
        size_t count = 0;
        if (fscanf(f, "%zu", &count) == 1) {
            presets.clear();
            for (size_t i = 0; i < count; i++) {
                Preset p;
                char nameBuf[64];
                if (fscanf(f, " %[^\n]", nameBuf) == 1) {
                    strncpy(p.name, nameBuf, sizeof(p.name) - 1);
                    p.name[sizeof(p.name) - 1] = '\0';
                }
                int c, s, a, vk;
                float g, ct, b;
                if (fscanf(f, "%d %d %d %d %f %f %f", &c, &s, &a, &vk, &g, &ct, &b) == 7) {
                    p.ctrl = (c != 0);
                    p.shift = (s != 0);
                    p.alt = (a != 0);
                    p.virtualKey = vk;
                    p.gamma = g;
                    p.contrast = ct;
                    p.brightness = b;
                    p.was_down = false;
                    p.confirming_delete = false;
                    presets.push_back(p);
                }
            }
        }
        fclose(f);
    }
}

std::string BrowseFolder(HWND owner) {
    char path[MAX_PATH] = "";
    BROWSEINFOA bi = {0};
    bi.hwndOwner = owner;
    bi.lpszTitle = "Select Preset Save Directory";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
}

// Helper Functions for Key Formatting because nobody wants to see VK_F1 instead of F1
std::string GetSingleKeyName(int vk) {
    if (vk == 0) return "";

    switch (vk) {
        case VK_SPACE:   return "Space";
        case VK_RETURN:  return "Enter";
        case VK_ESCAPE:  return "Esc";
        case VK_TAB:     return "Tab";
        case VK_BACK:    return "Backspace";
        case VK_DELETE:  return "Delete";
        case VK_INSERT:  return "Insert";
        case VK_HOME:    return "Home";
        case VK_END:     return "End";
        case VK_PRIOR:   return "PageUp";
        case VK_NEXT:    return "PageDown";
        case VK_LEFT:    return "Left";
        case VK_RIGHT:   return "Right";
        case VK_UP:      return "Up";
        case VK_DOWN:    return "Down";
        case VK_CAPITAL: return "CapsLock";
        case VK_NUMLOCK: return "NumLock";
        case VK_SCROLL:  return "ScrollLock";
        case VK_PAUSE:   return "Pause";
    }

    if (vk >= VK_F1 && vk <= VK_F24) {
        char buf[16];
        snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
        return std::string(buf);
    }

    UINT scanCode = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC);
    if (scanCode != 0) {
        char keyName[64] = {0};
        if (GetKeyNameTextA(scanCode << 16, keyName, sizeof(keyName)) > 0) {
            return std::string(keyName);
        }
    }

    char fallback[32];
    snprintf(fallback, sizeof(fallback), "Key 0x%02X", vk);
    return std::string(fallback);
}

std::string GetFullKeyComboString(bool ctrl, bool shift, bool alt, int vk) {
    std::string result = "";
    if (ctrl)  result += "Ctrl + ";
    if (shift) result += "Shift + ";
    if (alt)   result += "Alt + ";

    if (vk != 0) {
        result += GetSingleKeyName(vk);
    } else if (!result.empty()) {
        result = result.substr(0, result.length() - 3);
    }

    return result.empty() ? "None" : result;
}

// Windows Display API
void ApplyDisplaySettings(float g, float c_val, float b_val) {
    WORD ramp[3][256];
    
    float gamma = (g < 0.1f) ? 0.1f : g;
    float inv_gamma = 1.0f / gamma;

    float c = c_val / 100.0f;
    float b = b_val / 100.0f;

    for (int i = 0; i < 256; i++) {
        float norm = (float)i / 255.0f;

        float v = powf(norm, inv_gamma);
        v = (v - 0.5f) * c + 0.5f;
        v = v + (b - 1.0f);

        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;

        WORD val = (WORD)(v * 65280.0f + (float)i);

        ramp[0][i] = val;
        ramp[1][i] = val;
        ramp[2][i] = val;
    }

    HDC hdc = GetDC(NULL);
    if (hdc) {
        SetDeviceGammaRamp(hdc, ramp);
        ReleaseDC(NULL, hdc);
    }
}

void CaptureInitialDisplaySettings() {
    HDC hdc = GetDC(NULL);
    if (hdc) {
        if (GetDeviceGammaRamp(hdc, originalRamp)) {
            hasOriginalRamp = true;
        }
        ReleaseDC(NULL, hdc);
    }
}

void RestoreDefaults() {
    cur_gamma = default_gamma; 
    cur_contrast = default_contrast; 
    cur_brightness = default_brightness;
    active_preset_name = "None";

    ApplyDisplaySettings(cur_gamma, cur_contrast, cur_brightness);
}

// Windows Registry and Tray Icon Functions
void SetStartupRegistry(bool enable) {
    HKEY hKey;
    const char* appName = "Gammie";
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            RegSetValueExA(hKey, appName, 0, REG_SZ, (const BYTE*)exePath, (DWORD)(strlen(exePath) + 1));
        } else {
            RegDeleteValueA(hKey, appName);
        }
        RegCloseKey(hKey);
    }
}

void CheckStartupRegistry() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char buf[MAX_PATH];
        DWORD size = sizeof(buf);
        if (RegQueryValueExA(hKey, "Gammie", NULL, NULL, (LPBYTE)buf, &size) == ERROR_SUCCESS) {
            app_settings.start_with_windows = true;
        }
        RegCloseKey(hKey);
    }
}

void ShowTrayIcon() {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1001;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    snprintf(nid.szTip, sizeof(nid.szTip), "Gammie");
    Shell_NotifyIconA(NIM_ADD, &nid);
}

void HideTrayIcon() {
    NOTIFYICONDATAA nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1001;
    Shell_NotifyIconA(NIM_DELETE, &nid);
}

LRESULT CALLBACK CustomWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_TRAYICON) {
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
            ShowWindow(hWnd, SW_RESTORE);
            SetForegroundWindow(hWnd);
            HideTrayIcon();
            is_minimized = false;
        }
        return 0;
    }

    if (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP) {
        if (binding_key) return 0;
    }

    return CallWindowProc(original_wndproc, hWnd, uMsg, wParam, lParam);
}

// UI rendering
void RenderUI(GLFWwindow* window) {
    (void)window;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 3));
    ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 3.0f);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    
    ImGui::Begin("Display Controller", nullptr, 
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);

    if (ImGui::BeginTabBar("MainTabBar", ImGuiTabBarFlags_None)) {
        
        // Display tab
        if (ImGui::BeginTabItem("Display")) {
            ImGui::Spacing();
            
            bool changed = false;
            ImGui::PushItemWidth(130);
            changed |= ImGui::SliderFloat("Gamma", &cur_gamma, 0.30f, 2.65f, "%.2f");
            changed |= ImGui::SliderFloat("Contrast", &cur_contrast, 80.0f, 120.0f, "%.0f");
            changed |= ImGui::SliderFloat("Bright", &cur_brightness, 80.0f, 116.0f, "%.0f");
            ImGui::PopItemWidth();

            if (changed) {
                active_preset_name = "None";
                ApplyDisplaySettings(cur_gamma, cur_contrast, cur_brightness);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Restore Defaults", ImVec2(-FLT_MIN, 26))) {
                RestoreDefaults();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Restores display settings to startup default state.");
            }

            ImGui::Spacing();
            ImGui::Text("Current: %s", active_preset_name.c_str());
            if (active_preset_name != "None") {
                ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Swapped to: %s", active_preset_name.c_str());
            }
            
            ImGui::EndTabItem();
        }

        // presets tab
        if (ImGui::BeginTabItem("Presets")) {
            ImGui::Spacing();
            
            ImGui::PushItemWidth(110);
            ImGui::InputText("Name", new_preset_name, IM_ARRAYSIZE(new_preset_name));
            ImGui::PopItemWidth();

            if (binding_key) {
                if (binding_just_started) {
                    for (int i = 0; i < 256; i++) {
                        initial_key_state[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
                    }
                    binding_just_started = false;
                }

                ImGui::Button("Press key combo...", ImVec2(130, 22));

                bool isCtrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
                bool isShift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
                bool isAlt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;

                for (int vk = 8; vk <= 255; vk++) {
                    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                        vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
                        vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU    ||
                        vk == VK_LWIN    || vk == VK_RWIN) {
                        continue;
                    }

                    bool isDownNow = (GetAsyncKeyState(vk) & 0x8000) != 0;
                    if (isDownNow && !initial_key_state[vk]) {
                        new_preset_ctrl = isCtrl;
                        new_preset_shift = isShift;
                        new_preset_alt = isAlt;
                        new_preset_vk = vk;
                        binding_key = false;
                        break;
                    }
                }
            } else {
                std::string comboStr = GetFullKeyComboString(new_preset_ctrl, new_preset_shift, new_preset_alt, new_preset_vk);
                if (ImGui::Button(comboStr.c_str(), ImVec2(130, 22))) {
                    binding_key = true;
                    binding_just_started = true;
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to assign hotkey (optional)");
            }

            ImGui::SameLine();
            if (ImGui::Button("Save", ImVec2(-FLT_MIN, 22))) {
                if (strlen(new_preset_name) > 0) {
                    bool duplicate = false;
                    for (const auto& p : presets) {
                        if (_stricmp(p.name, new_preset_name) == 0) {
                            duplicate = true;
                            break;
                        }
                        if (new_preset_vk != 0 && p.virtualKey == new_preset_vk && 
                            p.ctrl == new_preset_ctrl && p.shift == new_preset_shift && p.alt == new_preset_alt) {
                            duplicate = true;
                            break;
                        }
                    }

                    if (duplicate) {
                        show_duplicate_popup = true;
                    } else {
                        Preset p;
                        strncpy(p.name, new_preset_name, sizeof(p.name) - 1);
                        p.name[sizeof(p.name) - 1] = '\0';
                        p.ctrl = new_preset_ctrl;
                        p.shift = new_preset_shift;
                        p.alt = new_preset_alt;
                        p.virtualKey = new_preset_vk;
                        p.gamma = cur_gamma;
                        p.contrast = cur_contrast;
                        p.brightness = cur_brightness;
                        p.was_down = false;
                        p.confirming_delete = false;
                        
                        presets.push_back(p);
                        SavePresets();
                        
                        new_preset_name[0] = '\0';
                        new_preset_ctrl = false;
                        new_preset_shift = false;
                        new_preset_alt = false;
                        new_preset_vk = 0;
                    }
                }
            }

            if (show_duplicate_popup) {
                ImGui::OpenPopup("DuplicateErrorModal");
                show_duplicate_popup = false;
            }
            if (ImGui::BeginPopupModal("DuplicateErrorModal", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("A preset with this name or key combination already exists!");
                ImGui::Spacing();
                if (ImGui::Button("OK", ImVec2(100, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::Text("Saved Presets:");
            
            float availHeight = ImGui::GetContentRegionAvail().y - 30.0f;
            if (availHeight < 50.0f) availHeight = 50.0f;

            ImGui::BeginChild("PresetList", ImVec2(0, availHeight), true);
            for (size_t i = 0; i < presets.size(); ++i) {
                ImGui::PushID((int)i);
                
                ImGui::Text("%s", presets[i].name);
                std::string keyStr = GetFullKeyComboString(presets[i].ctrl, presets[i].shift, presets[i].alt, presets[i].virtualKey);
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "[Key: %s] G:%.2f C:%.0f B:%.0f", 
                    keyStr.c_str(), presets[i].gamma, presets[i].contrast, presets[i].brightness);
                
                if (ImGui::Button("Apply", ImVec2(45, 20))) {
                    cur_gamma = presets[i].gamma;
                    cur_contrast = presets[i].contrast;
                    cur_brightness = presets[i].brightness;
                    active_preset_name = presets[i].name;
                    ApplyDisplaySettings(cur_gamma, cur_contrast, cur_brightness);
                }
                ImGui::SameLine();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));

                const char* delLabel = presets[i].confirming_delete ? "Sure?" : "Del";
                if (ImGui::Button(delLabel, ImVec2(45, 20))) {
                    if (!presets[i].confirming_delete) {
                        presets[i].confirming_delete = true;
                    } else {
                        if (active_preset_name == presets[i].name) active_preset_name = "None";
                        presets.erase(presets.begin() + i);
                        SavePresets();
                        ImGui::PopStyleColor(3);
                        ImGui::PopID();
                        break;
                    }
                }
                ImGui::PopStyleColor(3);
                
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
            
            ImGui::EndTabItem();
        }

        // settings tab
        if (ImGui::BeginTabItem("Settings")) {
            ImGui::Spacing();
            
            if (ImGui::Checkbox("Start with Windows", &app_settings.start_with_windows)) {
                SetStartupRegistry(app_settings.start_with_windows);
                SaveSettings();
            }
            
            if (ImGui::Checkbox("Minimize to tray when closed", &app_settings.minimize_to_tray)) {
                SaveSettings();
            }

            if (ImGui::Checkbox("Don't reset display on close", &app_settings.dont_reset_on_close)) {
                SaveSettings();
            }

            if (ImGui::Checkbox("Always on top", &app_settings.always_on_top)) {
                if (g_window) {
                    glfwSetWindowAttrib(g_window, GLFW_FLOATING, app_settings.always_on_top ? GLFW_TRUE : GLFW_FALSE);
                }
                SaveSettings();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Default Startup Display:");
            if (ImGui::Button("Set Current as Default", ImVec2(-FLT_MIN, 24))) {
                show_default_popup_1 = true;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Saves current display settings as the startup DEFAULT.");
            }

            if (show_default_popup_1) {
                ImGui::OpenPopup("DefaultConfirm1");
                show_default_popup_1 = false;
            }
            if (ImGui::BeginPopupModal("DefaultConfirm1", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure you want to update the default startup display state?");
                ImGui::Spacing();
                if (ImGui::Button("Yes", ImVec2(100, 0))) {
                    ImGui::CloseCurrentPopup();
                    show_default_popup_2 = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (show_default_popup_2) {
                ImGui::OpenPopup("DefaultConfirm2");
                show_default_popup_2 = false;
            }
            if (ImGui::BeginPopupModal("DefaultConfirm2", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Are you sure SURE?????");
                ImGui::Spacing();
                if (ImGui::Button("Yess im sureee shut up", ImVec2(120, 0))) {
                    default_gamma = cur_gamma;
                    default_contrast = cur_contrast;
                    default_brightness = cur_brightness;
                    SaveDefaultDisplay(default_gamma, default_contrast, default_brightness);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Text("Save Location:");
            ImGui::PushItemWidth(170);
            ImGui::InputText("##SavePath", save_dir, sizeof(save_dir), ImGuiInputTextFlags_ReadOnly);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Browse")) {
                std::string chosenDir = BrowseFolder(hwnd);
                if (!chosenDir.empty()) {
                    std::string oldDir = save_dir;
                    std::string newDir = chosenDir;

                    if (oldDir != newDir) {
                        std::string oldPresets = oldDir + "\\presets.dat";
                        std::string newPresets = newDir + "\\presets.dat";
                        CopyFileA(oldPresets.c_str(), newPresets.c_str(), FALSE);
                        DeleteFileA(oldPresets.c_str());

                        std::string oldDefault = oldDir + "\\default_display.dat";
                        std::string newDefault = newDir + "\\default_display.dat";
                        CopyFileA(oldDefault.c_str(), newDefault.c_str(), FALSE);
                        DeleteFileA(oldDefault.c_str());
                    }

                    strncpy(app_settings.custom_save_dir, newDir.c_str(), sizeof(app_settings.custom_save_dir) - 1);
                    app_settings.custom_save_dir[sizeof(app_settings.custom_save_dir) - 1] = '\0';
                    
                    SaveSettings();
                    GetActiveSaveDirectory(save_dir, sizeof(save_dir));
                    LoadPresets();
                    LoadDefaultDisplay();
                }
            }
            
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 25);
    ImGui::Separator();
    ImGui::TextDisabled("v1");
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 85);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
    ImGui::Text("GitHub");
    if (ImGui::IsItemClicked()) {
        ShellExecuteA(NULL, "open", "https://github.com/mokkito/Gammie", NULL, NULL, SW_SHOWNORMAL);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("Open GitHub repository");
    }
    ImGui::PopStyleColor();

    ImGui::End();
    
    ImGui::PopStyleVar(4);
}

// Windows Application Entry Point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance; (void)lpCmdLine; (void)nCmdShow;

    LoadSettings();

    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(260, 280, "Gammie", nullptr, nullptr);
    if (!window) return 1;
    
    g_window = window;

    if (app_settings.always_on_top) {
        glfwSetWindowAttrib(window, GLFW_FLOATING, GLFW_TRUE);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - 260) / 2 - (screenWidth / 4);
    int posY = (screenHeight - 280) / 2;
    glfwSetWindowPos(window, posX, posY);

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    hwnd = glfwGetWin32Window(window);
    original_wndproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);

    // hopefully fixed icon loading
    HICON hIconBig = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON hIconSmall = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    if (hIconBig) {
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    }
    if (hIconSmall) {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
    }

    GetActiveSaveDirectory(save_dir, sizeof(save_dir));
    LoadDefaultDisplay();
    LoadPresets();

    if (app_settings.dont_reset_on_close) {
        cur_gamma = app_settings.last_gamma;
        cur_contrast = app_settings.last_contrast;
        cur_brightness = app_settings.last_brightness;
    } else {
        cur_gamma = default_gamma;
        cur_contrast = default_contrast;
        cur_brightness = default_brightness;
    }
    ApplyDisplaySettings(cur_gamma, cur_contrast, cur_brightness);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    style.Colors[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.09f, 0.12f, 1.00f);
    style.Colors[ImGuiCol_Header]            = ImVec4(0.18f, 0.22f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(0.24f, 0.30f, 0.42f, 1.00f);
    style.Colors[ImGuiCol_HeaderActive]      = ImVec4(0.28f, 0.36f, 0.50f, 1.00f);
    style.Colors[ImGuiCol_Button]            = ImVec4(0.20f, 0.25f, 0.35f, 1.00f);
    style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(0.28f, 0.35f, 0.48f, 1.00f);
    style.Colors[ImGuiCol_ButtonActive]      = ImVec4(0.35f, 0.44f, 0.60f, 1.00f);
    style.Colors[ImGuiCol_FrameBg]           = ImVec4(0.04f, 0.04f, 0.06f, 1.00f);
    style.Colors[ImGuiCol_Tab]               = ImVec4(0.15f, 0.18f, 0.25f, 1.00f);
    style.Colors[ImGuiCol_TabHovered]        = ImVec4(0.25f, 0.32f, 0.45f, 1.00f);
    style.Colors[ImGuiCol_TabActive]         = ImVec4(0.22f, 0.35f, 0.55f, 1.00f);
    style.Colors[ImGuiCol_SliderGrab]        = ImVec4(0.35f, 0.48f, 0.70f, 1.00f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.60f, 0.85f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    CaptureInitialDisplaySettings();
    CheckStartupRegistry();

    glfwSetWindowCloseCallback(window, [](GLFWwindow* w) {
        if (app_settings.minimize_to_tray) {
            glfwSetWindowShouldClose(w, GLFW_FALSE);
            ShowWindow(hwnd, SW_HIDE);
            ShowTrayIcon();
            is_minimized = true;
        } else {
            app_open = false;
        }
    });

    while (app_open) {
        glfwPollEvents();

        for (auto& preset : presets) {
            if (preset.virtualKey == 0) continue;

            bool vkDown    = (GetAsyncKeyState(preset.virtualKey) & 0x8000) != 0;
            bool ctrlDown  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
            bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0 || (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
            bool altDown   = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;

            bool modsMatch = (preset.ctrl == ctrlDown) &&
                             (preset.shift == shiftDown) &&
                             (preset.alt == altDown);

            bool comboActive = vkDown && modsMatch;

            if (comboActive && !preset.was_down) {
                cur_gamma = preset.gamma;
                cur_contrast = preset.contrast;
                cur_brightness = preset.brightness;
                active_preset_name = preset.name;
                ApplyDisplaySettings(cur_gamma, cur_contrast, cur_brightness);
            }

            preset.was_down = comboActive;
        }

        if (!is_minimized) {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            RenderUI(window);

            ImGui::Render();
            int w, h;
            glfwGetFramebufferSize(window, &w, &h);
            glViewport(0, 0, w, h);
            glClearColor(0.08f, 0.09f, 0.12f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            glfwSwapBuffers(window);
        } else {
            Sleep(16);
        }
    }

    if (app_settings.dont_reset_on_close) {
        app_settings.last_gamma = cur_gamma;
        app_settings.last_contrast = cur_contrast;
        app_settings.last_brightness = cur_brightness;
        SaveSettings();
    } else {
        RestoreDefaults();
    }

    HideTrayIcon();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
