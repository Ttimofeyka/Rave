/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#pragma once
#include <string>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <vector>

static std::string replaceAll(std::string str, std::string substr1, std::string substr2) {
    for(size_t index = str.find(substr1, 0); index != std::string::npos && substr1.length(); index = str.find(substr1, index + substr2.length()))
        str.replace(index, substr1.length(), substr2);
    return str;
}

static std::string namespacesToString(std::vector<std::string> namespacesNames, std::string n) {
    std::string ret = n;
    for(int i=0; i<namespacesNames.size(); i++) ret = namespacesNames[i] + "::" + ret;
    return ret;
}

static bool isBasicType(std::string s) {
    return s == "char" || s == "uchar" || s == "short" || s == "ushort" || s == "int" ||s == "uint" || s == "long" || s == "ulong"
    || s == "cent" || s == "ucent" || s == "void" || s == "float" || s == "double" || s == "half" || s == "bhalf";
}

#ifdef _WIN32
#include <windows.h>
static std::vector<std::string> filesInDirectory(std::string path) {
    std::vector<std::string> files;

    // check directory exists
    char fullpath[MAX_PATH];
    GetFullPathNameA(path.c_str(), MAX_PATH, fullpath, 0);
    std::string fp(fullpath);
    if(GetFileAttributesA(fp.c_str()) != FILE_ATTRIBUTE_DIRECTORY) return files;

    // get file names
    WIN32_FIND_DATAA findfiledata;
    HANDLE hFind = FindFirstFileA((LPCSTR)(fp + "\\*").c_str(), &findfiledata);
    if(hFind != INVALID_HANDLE_VALUE) {
        do {
            files.push_back(findfiledata.cFileName);
        } 
        while(FindNextFileA(hFind, &findfiledata));
        FindClose(hFind);
    }

    // delete current and parent directories
    files.erase(std::find(files.begin(), files.end(), "."));
    files.erase(std::find(files.begin(), files.end(), ".."));

    // sort in alphabetical order
    std::sort(files.begin(), files.end());

    return files;
}
#else
#include <dirent.h>
static std::vector<std::string> filesInDirectory(std::string directory) {
    std::vector<std::string> files;

    DIR *dir;
    dir = opendir(directory.c_str());
    if(dir == NULL) return files;

    struct dirent *ent;
    while((ent = readdir(dir)) != NULL) files.push_back(ent->d_name);
    closedir(dir);

    // delete current and parent directories
    files.erase(std::find(files.begin(), files.end(), "."));
    files.erase(std::find(files.begin(), files.end(), ".."));

    // sort in alphabetical order
    std::sort(files.begin(), files.end());
    return files;
}
#endif

template<typename Base, typename T>
static bool instanceof(const T* ptr) {
    return dynamic_cast<const Base*>(ptr) != nullptr;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #define RAVE_OS "WINDOWS"
#elif __linux__
    #define RAVE_OS "LINUX"
#elif __APPLE__
    #define RAVE_OS "MACOS"
#else
    #define RAVE_OS "UNKNOWN"
#endif

#if defined(__x86_64__)
    #define RAVE_PLATFORM "X86_64"
#elif defined(__i386__)
    #define RAVE_PLATFORM "X86"
#elif defined(__aarch64__) || defined(__ARM_ARCH_ISA_A64)
    #define RAVE_PLATFORM "AARCH64"
#elif defined(__arm__)
    #define RAVE_PLATFORM "ARM"
#else
    #define RAVE_PLATFORM "UNKNOWN"
#endif

typedef struct genSettings {
    bool noPrelude = false;
    bool runtimeChecks = true;
    bool emitLLVM = false;
    bool onlyObject = false;
    bool noEntry = false;
    bool noStd = false;
    int optLevel = 1;
    bool isPIE = false;
    bool isPIC = true;
    bool disableWarnings = false;
    bool saveObjectFiles = false;
    bool isStatic = false;
    bool recompileStd = false;
    bool noChecks = false;
    std::string linkParams = "";

    #if defined(__linux__)
        std::string linker = "lld-11";
    #else
        std::string linker = "lld";
    #endif
} genSettings;

// trim from start (in place)
static inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    return s;
}

// trim from end (in place)
static inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
    return s;
}

// trim from both ends (in place)
static inline std::string trim(std::string s) {
    return rtrim(ltrim(s));
}

#if defined(_WIN32)
#include <algorithm> // for transform() in get_exe_path()
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

static std::string getExePath() {
    std::string path = "";
#if defined(_WIN32)
    wchar_t wc[260] = {0};
    GetModuleFileNameW(NULL, wc, 260);
    std::wstring ws(wc);
    std::transform(ws.begin(), ws.end(), std::back_inserter(path), [](wchar_t c) { return (char)c; });
    path = replaceAll(path, "\\", "/");
#elif defined(__linux__)
    char c[260];
    int length = (int)readlink("/proc/self/exe", c, 260);
    path = std::string(c, length > 0 ? length : 0);
#endif
    return path.substr(0, path.rfind('/')+1);
}

extern std::string exePath;

#define RAVE_DEBUG_MODE false