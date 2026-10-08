//
// Created by fgsqme on 2022/3/21 0021.
//

#include "Utils.h"
#include "CodeUtils.h"
#include "LString.h"
#include <QUuid>

#if defined(PLATFORM_WINDOWS)

#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audioclient.h>

#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif

#include <iostream>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>

/**
 * 设置文本到系统剪切板
 */
bool Utils::SetClipboardText(const char *str) {
#if defined(PLATFORM_WINDOWS)
    if (::OpenClipboard(nullptr)) {
        ::EmptyClipboard();
        HGLOBAL clipbuffer;
        char *buffer;
        clipbuffer = ::GlobalAlloc(GMEM_DDESHARE, strlen(str) + 1);
        buffer = (char *) ::GlobalLock(clipbuffer);
        strcpy(buffer, str);
        ::GlobalUnlock(clipbuffer);
        ::SetClipboardData(CF_TEXT, clipbuffer);
        ::CloseClipboard();
        return true;
    }
    return false;
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    return false;
#endif
}
/*
void SendAscii(wchar_t data, BOOL shift) {
    INPUT input[2];
    memset(input, 0, 2 * sizeof(INPUT));

    if (shift) {
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_SHIFT;
        SendInput(1, input, sizeof(INPUT));
    }

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = data;

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = data;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, input, sizeof(INPUT));

    if (shift) {
        input[0].type = INPUT_KEYBOARD;
        input[0].ki.wVk = VK_SHIFT;
        input[0].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, input, sizeof(INPUT));
    }
}

void SendUnicode(wchar_t data) {
    INPUT input[2];
    memset(input, 0, 2 * sizeof(INPUT));

    input[0].type = INPUT_KEYBOARD;
    input[0].ki.wVk = 0;
    input[0].ki.wScan = data;
    input[0].ki.dwFlags = 0x4;//KEYEVENTF_UNICODE;
    SendInput(1, &input[0], sizeof(INPUT));

    input[1].type = INPUT_KEYBOARD;
    input[1].ki.wVk = 0;
    input[1].ki.wScan = data;
    input[1].ki.dwFlags = KEYEVENTF_KEYUP |
                          0x4;//KEYEVENTF_UNICODE;  这里是为了防止英文字符进入到系统输入法里面，则可以解决国内的输入法软件拦截的问题，但是国外的软件一般做了UNICODE兼容，所以还是会有问题。
    SendInput(1, &input[1], sizeof(INPUT));
}*/

/*void Utils::SendKeys(const string &msg) {
    short vk;
    BOOL shift;
    wstring data = CodeUtils::CharToWchar(msg.c_str());
    int len = data.size();
    for (int i = 0; i < len; i++) {
        if (data[i] >= 0 && data[i] < 256) //ascii字符
        {
            vk = VkKeyScanW(data[i]);
            if (vk == -1) {
                SendUnicode(data[i]);
            } else {
                if (vk < 0) {
                    vk = ~vk + 0x1;
                }
                shift = vk >> 8 & 0x1;
                if (GetKeyState(VK_CAPITAL) & 0x1) {
                    if (data[i] >= 'a' && data[i] <= 'z' || data[i] >= 'A' && data[i] <= 'Z') {
                        shift = !shift;
                    }
                }
                SendAscii(vk & 0xFF, shift);
            }
        } else //unicode字符
        {
            SendUnicode(data[i]);
        }
    }
}*/

/**
 * 根据文件路径获取文件名称
 */
std::string Utils::GetPathName(const std::string &path) {
    std::string::size_type iPos;
    if (strstr(path.c_str(), "\\")) {
        iPos = path.find_last_of('\\') + 1;
    } else {
        iPos = path.find_last_of('/') + 1;
    }
    return path.substr(iPos, path.length() - iPos);
}

std::string formatDobleValue(double val, int fixed) {
    auto str = std::to_string(val);
    return str.substr(0, str.find('.') + fixed + 1);
}

/**
 * 将字节数转换为可读的文件大小字符串
 */
std::string Utils::computeSize(int64_t size) {
    if (size <= 0) return "0B";
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int digitGroups = static_cast<int>(std::log10(size) / std::log10(1024));
    std::ostringstream formattedString;
    formattedString << std::fixed << std::setprecision(1) << static_cast<double>(size) / std::pow(1024, digitGroups)
                     << units[digitGroups];
    return formattedString.str();
}

/**
 * 设置系统音量
 */
bool Utils::setVolum(int level) {
#if defined(PLATFORM_WINDOWS)
    HRESULT hr;
    IMMDeviceEnumerator *pDeviceEnumerator = nullptr;
    IMMDevice *pDevice = nullptr;
    IAudioEndpointVolume *pAudioEndpointVolume = nullptr;
    IAudioClient *pAudioClient = nullptr;
    try {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              (void **) &pDeviceEnumerator);
        if (FAILED(hr)) throw "CoCreateInstance";
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr)) throw "GetDefaultAudioEndpoint";
        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void **) &pAudioEndpointVolume);
        if (FAILED(hr)) throw "pDevice->Active";
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **) &pAudioClient);
        if (FAILED(hr)) throw "pDevice->Active";

        if (level == -2) {
            hr = pAudioEndpointVolume->SetMute(FALSE, nullptr);
            if (FAILED(hr)) throw "SetMute";
        } else if (level == -1) {
            hr = pAudioEndpointVolume->SetMute(TRUE, nullptr);
            if (FAILED(hr)) throw "SetMute";
        } else {
            if (level < 0 || level > 100) {
                hr = E_INVALIDARG;
                throw "Invalid Arg";
            }

            float fVolume;
            fVolume = level / 100.0f;
            hr = pAudioEndpointVolume->SetMasterVolumeLevelScalar(fVolume, &GUID_NULL);
            if (FAILED(hr)) throw "SetMasterVolumeLevelScalar";

            pAudioClient->Release();
            pAudioEndpointVolume->Release();
            pDevice->Release();
            pDeviceEnumerator->Release();
            return true;
        }
    }
    catch (...) {
        if (pAudioClient) pAudioClient->Release();
        if (pAudioEndpointVolume) pAudioEndpointVolume->Release();
        if (pDevice) pDevice->Release();
        if (pDeviceEnumerator) pDeviceEnumerator->Release();
        throw;
    }
    return false;
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX)
    system("amixer set -c 0 Master 50");
    return false;
#elif defined(PLATFORM_MACOS)
#endif
}

/**
 * @brief volume
 * 获取系统音量
 * @return
 */
int Utils::volume() {
#if defined(PLATFORM_WINDOWS)
    HRESULT hr;
    IMMDeviceEnumerator *pDeviceEnumerator = 0;
    IMMDevice *pDevice = 0;
    IAudioEndpointVolume *pAudioEndpointVolume = 0;
    IAudioClient *pAudioClient = 0;
    try {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                              (void **) &pDeviceEnumerator);
        if (FAILED(hr)) throw "CoCreateInstance";
        hr = pDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &pDevice);
        if (FAILED(hr)) throw "GetDefaultAudioEndpoint";
        hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void **) &pAudioEndpointVolume);
        if (FAILED(hr)) throw "pDevice->Active";
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **) &pAudioClient);
        if (FAILED(hr)) throw "pDevice->Active";
        float fVolume;
        hr = pAudioEndpointVolume->GetMasterVolumeLevelScalar(&fVolume);
        if (FAILED(hr)) throw "SetMasterVolumeLevelScalar";
        pAudioClient->Release();
        pAudioEndpointVolume->Release();
        pDevice->Release();
        pDeviceEnumerator->Release();
        int intVolume = fVolume * 100 + 1;
        if (fVolume > 100) {
            fVolume = 100;
        }
        return intVolume;
    }
    catch (...) {
        if (pAudioClient) pAudioClient->Release();
        if (pAudioEndpointVolume) pAudioEndpointVolume->Release();
        if (pDevice) pDevice->Release();
        if (pDeviceEnumerator) pDeviceEnumerator->Release();
        throw;
    }
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
#endif
    return 0;
}

/**
 * 生成 UUID
 */
QString Utils::getUUID() {
    return QUuid::createUuid().toString().remove("{").remove("}").remove("-");
}

/**
 * URL 解码
 */
LString Utils::urlDecode(const LString &input) {
    LString decoded;
    for (std::size_t i = 0; i < input.getLength(); ++i) {
        if (input[i] == '%') {
            if (i + 2 < input.getLength()) {
                // 解析十六进制编码
                char hi = input[i + 1];
                char lo = input[i + 2];
                auto hexVal = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                int hiValue = hexVal(hi);
                int loValue = hexVal(lo);
                if (hiValue != -1 && loValue != -1) {
                    int hexValue = hiValue * 16 + loValue;
                    decoded += static_cast<char>(hexValue);
                    i += 2;
                } else {
                    // 处理无效的编码序列
                    // 可以选择抛出异常或者忽略这些错误
                }
            } else {
                // 处理不完整的编码序列
                // 可以选择抛出异常或者忽略这些错误
            }
        } else if (input[i] == '+') {
            decoded += ' ';
        } else {
            decoded += input[i];
        }
    }
    return decoded;
}

/**
 * 判断文件是否为图片
 */
bool Utils::isPhoto(const QString& fileName) {
    int lastDotIndex = fileName.lastIndexOf('.');
    if (lastDotIndex != -1) {
        QString name = fileName.section('.', -1); // -1表示最后一个分隔符之后的部分
        if (name == "jpg"
            || name == "png"
            || name == "bmp"
            || name == "jpeg") {
            return true;
        }
    }
    return false;
}