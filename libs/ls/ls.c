
#include "../include/sHELL.h"
#include <windows.h>

const char Name[] = "ls";
const char Help[] = "list info in a directory\n"
                    "usage: ls <directory> \n"
                    "will be current directory if not  specified";

InternalAPI *core = NULL;

LPVOID lpOut = NULL;

//helper func
//take from this https://learn.microsoft.com/en-us/windows/win32/fileio/listing-the-files-in-a-directory
void ListFiles(LPCSTR directory) {
    WIN32_FIND_DATA findFileData;
    LARGE_INTEGER filesize;
    TCHAR szDir[MAX_PATH];
    HANDLE hFind = INVALID_HANDLE_VALUE;
    
   
    //copy string to buffer then append \* to name
    core->sprintf(szDir, "%s\\*", directory);

    //find first file
    hFind = FindFirstFileA(szDir, &findFileData);
    if (hFind == INVALID_HANDLE_VALUE) {
        debug_wprintf(L"Error opening directory %S with code: %lu\n", directory, GetLastError());
        return;
    }

    debug_wprintf(L"Listing files in directory %S\n", directory);
    do {
        if(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY){
          core->wprintf(L" %S   <DIR>\n", findFileData.cFileName);
        }
        else{
          filesize.LowPart = findFileData.nFileSizeLow;
          filesize.HighPart = findFileData.nFileSizeHigh;
          core->wprintf(L"%S     %ld bytes\n",findFileData.cFileName, filesize.QuadPart);
        }
        
    } while (FindNextFile(hFind, &findFileData) != 0);

    FindClose(hFind);
}

// Cleanup
__declspec(dllexport) VOID CommandCleanup() {
  if (lpOut) {
    core->free(lpOut);
    lpOut = NULL;
  }
}

// initialization code
__declspec(dllexport) BOOL CommandInit(InternalAPI *lpCore) { 
  core = lpCore;
  return TRUE;
  }

// Exported function - Name
__declspec(dllexport) const char *CommandNameA() { return Name; }

// Exported function - Help
__declspec(dllexport) const char *CommandHelpA() { return Help; }


// Exported function - Run
__declspec(dllexport) LPVOID CommandRunA(int argc, char **argv) {
  // Example implementation: print arguments and return count
  LPCSTR directory;
  if(argc==1){
    directory = "."; //current directory
  }
  else if(argc == 2){
    directory = argv[1];
  }
  else{
    core->wprintf(L"%S\n", CommandHelpA());
    return lpOut;
  }

  ListFiles(directory);
  
  return (LPVOID)1; //success
}

// Entrypoint for the DLL
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  switch (fdwReason) {
  case DLL_PROCESS_ATTACH:
    // Code to run when the DLL is loaded
    break;
  case DLL_PROCESS_DETACH:
    // Code to run when the DLL is unloaded
    break;
  case DLL_THREAD_ATTACH:
    // Code to run when a thread is created during DLL's existence
    break;
  case DLL_THREAD_DETACH:
    // Code to run when a thread ends normally
    break;
  }
  return TRUE; // Successful
}
