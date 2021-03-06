/**
  Copyright © 2020 lorenzoinvidia. All Rights Reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are
  met:
  
  1. Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  
  2. Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  
  3. The name of the author may not be used to endorse or promote products
  derived from this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY AUTHORS "AS IS" AND ANY EXPRESS OR
  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE. 
*/

#include <Windows.h>
#include <winternl.h>
#include <iostream>

using namespace std;
#define THREADCOUNT 3
#define MAXBUFFSIZE 50
#define DESC L"test"

typedef HANDLE(WINAPI *_CreateTransaction)(
	IN LPSECURITY_ATTRIBUTES lpTransactionAttributes,
	IN LPGUID                UOW,
	IN DWORD                 CreateOptions,
	IN DWORD                 IsolationLevel,
	IN DWORD                 IsolationFlags,
	IN DWORD                 Timeout,
	LPWSTR                   Description
);

typedef BOOL (WINAPI *_CommitTransaction)(
	IN HANDLE TransactionHandle
);


DWORD __stdcall threadFn(LPVOID parameter) {

    HANDLE hTr = (HANDLE)parameter;

	/* Open named mutex */
    HANDLE hMutex = CreateMutexA(NULL,FALSE,"mtx");

    if (!hMutex){
        cout << GetCurrentThreadId() << " failed open the mutex" << endl;
        return 0;
    }

    /* Wait to acquire mutex */
    WaitForSingleObject(hMutex, INFINITE);
    
    /* Create file */
	HANDLE hTrFile = CreateFileTransactedA(
		"C:\\Users\\t\\Desktop\\TrFile.txt",	// Path
		FILE_APPEND_DATA,						// Append data
		0,										// Do not share
		NULL,									// Default security
		OPEN_ALWAYS,							// Open or create if not exists
		FILE_ATTRIBUTE_NORMAL,					// Normal file
		NULL,									// No template file
		hTr,									// Transaction handle
		NULL,									// Miniversion (?)
		NULL									// Reserved
	);

	if (hTrFile == INVALID_HANDLE_VALUE) {
		cout << "CreateFile failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}
    
    char buffer[MAXBUFFSIZE];
    sprintf(buffer, "%d\n", GetCurrentThreadId());
    short buffLen = strlen(buffer);

    /* File ops */
	WriteFile(
		hTrFile,	// file handle
		buffer,		// buffer
		buffLen,    // no bytes to write
		NULL,		// recv no bytes written
		NULL		// overlap
	);

    /* Close file handle */
	CloseHandle(hTrFile);
    
    /* Release mutex */
    ReleaseMutex(hMutex);
    
    return 0;
}


int main(){
   

	/* Runtime loading */
	HMODULE hKtmW32;
	if (!(hKtmW32 = LoadLibraryA("KtmW32.dll"))) {
		cout << "LoadLibrary failed" << endl;
		return EXIT_FAILURE;
	}

	_CreateTransaction CreateTransaction = NULL;
	if (!(CreateTransaction = (_CreateTransaction)GetProcAddress(hKtmW32, "CreateTransaction"))) {
		cout << "GetProcAddress failed" << endl;
		return EXIT_FAILURE;
	}

	_CommitTransaction CommitTransaction = NULL;
	if (!(CommitTransaction = (_CommitTransaction)GetProcAddress(hKtmW32, "CommitTransaction"))) {
		cout << "GetProcAddress failed" << endl;
		return EXIT_FAILURE;
	}


	/* Create Transaction */
	HANDLE hTr = CreateTransaction(
		NULL,							// No inheritance
		0,								// Reserved
		TRANSACTION_DO_NOT_PROMOTE,		// The transaction cannot be distributed
		0,								// Reserved
		0,								// Reserved
		0,								// Abort after timeout (ms), 0 = infinite
		(LPWSTR)DESC					// Description
	);

	if (hTr == INVALID_HANDLE_VALUE) {
		cout << "CreateTransaction failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}


    /* Create Named mutex */
    HANDLE hMutex = CreateMutex(
                        NULL,			// default , no inheritance
                        FALSE,			// init not owned
                        "mtx"			// named mutex
                    ); 
    
    DWORD threadId;
    HANDLE hThread[THREADCOUNT];
    int i;

    /* Start working threads */
    for (i=0; i<THREADCOUNT; i++){
        hThread[i] = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadFn, (LPVOID)hTr, 0, &threadId);
    }

    /* Wait for all threads to terminate */
    WaitForMultipleObjects(THREADCOUNT, hThread, TRUE, INFINITE);

    /* Commit transaction */
	BOOL res = CommitTransaction(hTr);
	
	if (!res) {
		cout << "CommitTransaction failed with err: " << GetLastError() << endl;
		return EXIT_FAILURE;
	}

	/* Close transaction handle */
	CloseHandle(hTr);

    /* Close thread and mutex handles */
    for(i=0; i<THREADCOUNT; i++)
        CloseHandle(hThread[i]);
    CloseHandle(hMutex);
    
    cout << "All threads end" << endl;

	std::cin.get();
    return EXIT_SUCCESS;
}
