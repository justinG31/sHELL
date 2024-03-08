
#include <windows.h>

#include "../include/opcodes.h"
#include "../include/sHELL.h"

#include "../include/readf.h"

const char Name[] = "cat";
const char Help[] = "Concatenate and print files to the standard output.\n"
                    "Usage:\n"
                    "    cat <file1> <file2> ...\n";

// declue dependenceies
// always ensure null terminated
// this command depends on readf

#define INDEX_cat_readf 0

// note that order of dependencies matters
CommandDependency deps[] = {DECLARE_DEP(OPCODE_readf), DECLARE_DEP(0)};
InternalAPI *core = NULL;

LPVOID lpOut = NULL;
CommandA *readf = NULL;

char **FileNameToArgv(int *argc, char *lpFileName) {

  char fmt[] = "readf %s";
  // TODO migrate to macro
  char *argvBuff =
      (char *)core->malloc(core->strlen(lpFileName) + core->strlen(fmt) - 1);
  core->sprintf(argvBuff, fmt, lpFileName);
  char **readfArgv = core->CommandLineToArgvA(argvBuff, argc);
  // free templated string
  core->free(argvBuff);
  return readfArgv;
}

__declspec(dllexport) VOID CommandCleanup() {

  for (int i = 0; deps[i].lpCmd != NULL; i++) {
    debug_wprintf(L"Cleaning up %i:%lu\n", i, deps[i].hash);
    deps[i].lpCmd->fnCleanup();
  }
  if (lpOut) {
    core->free(lpOut);
    lpOut = NULL;
  }
}

// Initialization code
__declspec(dllexport) BOOL CommandInit(InternalAPI *lpCore) {
  core = lpCore;
  debug_wprintf(L"[+] Initializing %S\n", Name);
  if (!core->ResolveCommandDependnecies(deps)) {
    core->wprintf(L"Dependency failed!\n");
    return FALSE;
  }
  debug_wprintf(L"Setting readf to lpCMD...\n");
  readf = deps[INDEX_cat_readf].lpCmd;
  debug_wprintf(L"Found readf: %p\n", (void *)readf);
  return TRUE;
}

// Exported function - Name
__declspec(dllexport) const char *CommandNameA() { return Name; }

// Exported function - Help
__declspec(dllexport) const char *CommandHelpA() { return Help; }

#define DEBUG
// Exported function - Run
__declspec(dllexport) LPVOID CommandRunA(int argc, char **argv) {
  if (argc < 2) {
    core->wprintf(L"Invalid arguments.\n%S", CommandHelpA());
    return NULL; // Error code for invalid arguments
  }

  LPVOID readfOut = NULL;
  size_t totalSize = 0;
  char* bufferPtr = NULL;
  // // your answer here
  //traverse through all files given
  for(int i = 1; i< argc; i++){
    HANDLE hFile = CreateFileA(argv[i], GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        debug_wprintf(L"Error opening file. Error code: %lu\n", GetLastError());
        return lpOut;
    }
      
    DWORD fileSize = GetFileSize(hFile, NULL);
    LPVOID partialBuffer = core->realloc(readfOut, totalSize + fileSize);
    if (partialBuffer == NULL) {
    CloseHandle(hFile);
    debug_wprintf(L"Error allocating memory for file content.\n");
    return lpOut;
    }

    //update buffer ptr and buffer
    bufferPtr = (char*)partialBuffer + totalSize;
    readfOut = partialBuffer;

    DWORD bytesRead;
    if(ReadFile(hFile, bufferPtr, fileSize, &bytesRead, NULL) == 0) {
        debug_wprintf(L"Error Reading File:  %lu\n", GetLastError());
        core->free(readfOut);
        CloseHandle(hFile);
        return lpOut;   
    }

    //update size
    totalSize += bytesRead;
    CloseHandle(hFile);
    
  }
  core->WriteStdOut(readfOut, totalSize);
  core->free(readfOut);
  return (LPVOID)1;
}

// Entrypoint for the DLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    break;
  case DLL_PROCESS_DETACH:
    break;
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
    break;
  }
  return TRUE; // Successful
}
