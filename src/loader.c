// TODO
#include "corecrt.h"
#include "debugapi.h"
#include "errhandlingapi.h"
#include "fileapi.h"
#include "handleapi.h"
#include "libloaderapi.h"
#include "minwindef.h"
#include "winnt.h"
#include <memoryapi.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include "loader.h"
/*
 * 64
 * (64 - 8) -->56 -- divide by sizeof(word) --> 28
 *  ( dwblocksize  (2 * sizeof(DWORD)) )/ (sizeof(WORD))
 */
typedef struct RelocationBlock {
  DWORD dwPageRVA;
  DWORD dwBlockSize;
  WORD awRelocations[];
} RelocationBlock;

BOOL HandleRelocations(LPVOID lpImageBase) {

  IMAGE_DOS_HEADER *lpImageDos = (IMAGE_DOS_HEADER *)lpImageBase;
  IMAGE_NT_HEADERS *lpNtHeader =
      (IMAGE_NT_HEADERS *)(lpImageBase + lpImageDos->e_lfanew);
  IMAGE_OPTIONAL_HEADER opHeader = lpNtHeader->OptionalHeader;
  DWORD relocationRVA =
      opHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
  ptrdiff_t delta =
      (ptrdiff_t)((UINT_PTR)lpImageBase - (UINT_PTR)opHeader.ImageBase);
  if (delta == 0) {
    //printf("[+] Got preffered addr, no relocations to perform!\n");
    return TRUE;
  }
  if (relocationRVA == 0) {
    //printf("[+] No relocation table found\n");
    return TRUE;
  }
  RelocationBlock *block = (RelocationBlock *)(lpImageBase + relocationRVA);
  while (block->dwBlockSize != 0) {
    // we have a relocation
    DWORD dwRelocations =
        (block->dwBlockSize - (sizeof(DWORD) * 2)) / (sizeof(WORD));
    for (DWORD i = 0; i < dwRelocations; i++) {
      DWORD dwRelocationType = block->awRelocations[i] >> 12;
      INT offset = 0xfff & block->awRelocations[i];

      ULONGLONG *reloVA = (lpImageBase + block->dwPageRVA + offset);
      switch (dwRelocationType) {
      case (IMAGE_REL_BASED_ABSOLUTE):
        // NOThiNG;
        //printf("|__[+] nothing to be done for IMAGE_REL_BASED_ABSOLUTE");
        break;
      case (IMAGE_REL_BASED_DIR64):
        //printf("|__[+] Performing IMAGE_REL_BASED_ABSOLUTE at %p\n", reloVA);
        *reloVA = *reloVA + delta;
        break;
      default:
        //printf("|__[!] unknown relocation type %d\n", dwRelocationType);
        return FALSE;
      }
    }
    block = (RelocationBlock *)((LPVOID)block + block->dwBlockSize);
  }
  return TRUE;
}

BYTE *LoadFileBytes(char *szFilePath, DWORD *lpDwSize) {
  //printf("[*] Loading binary payload at path %s\n", szFilePath);
  HANDLE hFile = NULL;
  hFile = CreateFileA(szFilePath, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (hFile == INVALID_HANDLE_VALUE) {
    //printf("Failed to open file %s because of %d\n", szFilePath,
       //    GetLastError());
    return NULL;
  }
  // we have a valid handle
  //
  *lpDwSize = GetFileSize(hFile, NULL);
  LPVOID lpFileBuffer =
      VirtualAlloc(NULL, *lpDwSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (lpFileBuffer == NULL) {
    //printf("Could not get page of memory because of %d\n", GetLastError());
    CloseHandle(hFile);
    return NULL;
  }
  DWORD dwBytesRead = 0;
  if (!ReadFile(hFile, lpFileBuffer, *lpDwSize, &dwBytesRead, NULL)) {
    //printf("Reading failed because of %d\n", GetLastError());
    CloseHandle(hFile);
    VirtualFree(lpFileBuffer, *lpDwSize, MEM_RELEASE);
    return NULL;
  }
  if (dwBytesRead != *lpDwSize) {

    //printf("Reading failed because of %d\n", GetLastError());
    CloseHandle(hFile);
    VirtualFree(lpFileBuffer, *lpDwSize, MEM_RELEASE);
    return NULL;
  }

  // cleanup
  CloseHandle(hFile);
  return (BYTE *)lpFileBuffer;
}

BOOL HandleTLSCallback(LPVOID lpImageBase) {

  IMAGE_DOS_HEADER *lpImageDos = (IMAGE_DOS_HEADER *)lpImageBase;
  IMAGE_NT_HEADERS *lpNtHeader =
      (IMAGE_NT_HEADERS *)(lpImageBase + lpImageDos->e_lfanew);
  //IMAGE_OPTIONAL_HEADER opHeader = lpNtHeader->OptionalHeader;

  if (lpNtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS]
          .Size) {
    IMAGE_TLS_DIRECTORY *tls =
        (PIMAGE_TLS_DIRECTORY)((UINT_PTR)lpImageBase +
                               lpNtHeader->OptionalHeader
                                   .DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS]
                                   .VirtualAddress);

    PIMAGE_TLS_CALLBACK *callback =
        (PIMAGE_TLS_CALLBACK *)tls->AddressOfCallBacks;

    while (*callback) {
      //printf("[+] TLS callback at %p\n", (void *)callback);
      (*callback)((LPVOID)lpImageBase, DLL_PROCESS_ATTACH, NULL);
      // get me the next TLS callback
      callback++;
    }
  } else {
    //printf("No Tls Callbacks!\n");
  }
  return TRUE;
}
typedef void EntryPoint(void);

void Run(LPVOID lpImageBase) {

  IMAGE_DOS_HEADER *lpImageDos = (IMAGE_DOS_HEADER *)lpImageBase;
  IMAGE_NT_HEADERS *lpNtHeader =
      (IMAGE_NT_HEADERS *)(lpImageBase + lpImageDos->e_lfanew);
  IMAGE_OPTIONAL_HEADER opHeader = lpNtHeader->OptionalHeader;
  LPVOID lpEntry = (lpImageBase + opHeader.AddressOfEntryPoint);
  ((EntryPoint *)lpEntry)();
  // CreateThread()
}

BYTE *MemoryMapPE(BYTE *lpFileBytes, DWORD dwSize) {
  IMAGE_DOS_HEADER *lpImageDos = (IMAGE_DOS_HEADER *)lpFileBytes;
  IMAGE_NT_HEADERS *lpNtHeader =
      (IMAGE_NT_HEADERS *)(lpFileBytes + lpImageDos->e_lfanew);
  IMAGE_OPTIONAL_HEADER opHeader = lpNtHeader->OptionalHeader;
  DWORD dwSizeOfImage = opHeader.SizeOfImage;

  BYTE *lpImageBase = VirtualAlloc(
      NULL, dwSizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  // TODO ommit error checking

  // Copy the headers
  memcpy(lpImageBase, lpFileBytes, opHeader.SizeOfHeaders);
  IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION(lpNtHeader);
  DWORD dwNumSections = lpNtHeader->FileHeader.NumberOfSections;
  for (DWORD i = 0; i < dwNumSections; i++) {
    LPVOID dest = (LPVOID)(lpImageBase + sections[i].VirtualAddress);
    LPVOID src = (LPVOID)(lpFileBytes + sections[i].PointerToRawData);
    // TODO: handle situation where virtual size/physical size is 0
    //
    if (sections[i].SizeOfRawData != 0) {
      memcpy(dest, src, sections[i].SizeOfRawData);
    }
  }
  return lpImageBase;
}

// handle Imports

BOOL HandleImports(LPVOID lpImageBase) {

  IMAGE_DOS_HEADER *lpImageDos = (IMAGE_DOS_HEADER *)lpImageBase;
  IMAGE_NT_HEADERS *lpNtHeader =
      (IMAGE_NT_HEADERS *)(lpImageBase + lpImageDos->e_lfanew);
  IMAGE_OPTIONAL_HEADER opHeader = lpNtHeader->OptionalHeader;
  IMAGE_IMPORT_DESCRIPTOR *lpImport =
      (IMAGE_IMPORT_DESCRIPTOR
           *)(lpImageBase + opHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                                .VirtualAddress);
  for (int i = 0; lpImport[i].FirstThunk != 0; i++) {
    char *szDllNmame = (char *)(lpImageBase + lpImport[i].Name);
    HMODULE hLib = LoadLibraryA(szDllNmame);
    if (!hLib) {
      //printf("[!] failed to load %s because of %d\n", szDllNmame,
        //     GetLastError());
      return FALSE;
    }
    //printf("[+] loaded Library %s:%p\n", szDllNmame, (void *)hLib);
    IMAGE_THUNK_DATA *lpLookupTable =
        (IMAGE_THUNK_DATA *)(lpImageBase + lpImport[i].OriginalFirstThunk);
    IMAGE_THUNK_DATA *lpAddresTable =
        (IMAGE_THUNK_DATA *)(lpImageBase + lpImport[i].FirstThunk);
    for (int j = 0; lpLookupTable[j].u1.AddressOfData != 0; j++) {
      FARPROC lpFunc = NULL;
      ULONGLONG qwLookupValue = lpLookupTable[j].u1.AddressOfData;
      // TODO handle import by Ordinal vlaue
      // If import by Name
      char *szFuncName = NULL;
      if ((qwLookupValue & IMAGE_ORDINAL_FLAG) == 0) {
        IMAGE_IMPORT_BY_NAME *lpImportByName =
            (IMAGE_IMPORT_BY_NAME *)(lpImageBase + qwLookupValue);
        szFuncName = lpImportByName->Name;
        lpFunc = GetProcAddress(hLib, szFuncName);
      } else {

        lpFunc = GetProcAddress(hLib, (LPSTR)qwLookupValue);
      }
      if (!lpFunc) {
        //printf("[!] Failed to find function %s because of %d\n", szDllNmame,
          //     GetLastError());
        // TODO: Cleanupup other libraried
        // WARN Leak
        FreeLibrary(hLib);
        return FALSE;
      }

      // TODO might be  type confusion
      lpAddresTable[j].u1.Function = (ULONGLONG)lpFunc;
      //printf("|___[+] Reoslved  %s!%s --> %p\n", szDllNmame, szFuncName,
       //      (LPVOID)lpFunc);
    }
  }
  return TRUE;
}

HMODULE LoadPe(LPVOID lpFileBytes, DWORD dwFileSize) {

  LPVOID lpImageBase = MemoryMapPE(lpFileBytes, dwFileSize);
  if (!HandleImports(lpImageBase)) {
    //printf("[!] Failed to resolve imports\n");
    return FALSE;
  }
  if (!HandleRelocations(lpImageBase)) {
    //printf("Failed to perform relocations!\n");
    return FALSE;
  }
  if (!HandleTLSCallback(lpImageBase)) {
    //printf("Failed to perform TLS callbacks\n");
    return FALSE;
  }
  // Free FileBytes
  //Run(lpImageBase);
  //return TRUE;
  return (HMODULE)lpImageBase;
}
/*
int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: %s path_to_exe\n", argv[0]);
    return 1;
  }
  DWORD dwFileSize = 0;
  BYTE *lpFileBytes = LoadFileBytes(argv[1], &dwFileSize);
  if (lpFileBytes == NULL) {
    printf("Failed to load file. Goodbyte!\n");
    return 1;
  }
  printf("[+] Loaded file bytes!\n");
  OutputDebugStringA("Test 0\n");
}

*/