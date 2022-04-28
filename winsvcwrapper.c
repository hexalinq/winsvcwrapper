/*
MIT License

Copyright © 2022 Hexalinq.
Web: https://hexalinq.com/
Email: info@hexalinq.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <windows.h>

int printf(const char*, ...);

HANDLE _hStopEvent = NULL;
SERVICE_STATUS _tServiceStatus = {};
SERVICE_STATUS_HANDLE _hServiceStatus = NULL;

void __attribute__((noreturn)) abort() {
	asm("int3"); // Try to break into the debugger
	ExitProcess(1); // Terminate process with 1 as the exit code
	*(void**)0; // Dereference a NULL pointer to force a program crash in case ExitProcess(1) fails
}

wchar_t* wcsdup(const wchar_t* wString) {
	size_t iBytes = (wcslen(wString) + 1) * sizeof(wchar_t);
	wchar_t* wNew = malloc(iBytes);
	if(!wNew) abort();
	memcpy(wNew, wString, iBytes);
	return wNew;
}

// Strip the first argument (the executable name), so "winsvcwrap.exe program.exe arg1" becomes "program.exe arg1"
wchar_t* ParseArgumentString(wchar_t* wArgs) {
	int iLength = wcslen(wArgs);
	if(wArgs[0] != '"') {
		int iSeparator = 0;
		for(; iSeparator < iLength; ++iSeparator)
			if(wArgs[iSeparator] == ' ') goto found_space;

		abort();
		found_space:
		return wArgs + iSeparator + 1;

	} else {
		int iSeparator = 1;
		int bEscape = FALSE;
		for(; iSeparator < iLength; ++iSeparator) {
			if(wArgs[iSeparator] == '\\') bEscape ^= 1;
			else if(wArgs[iSeparator] == '"' && !bEscape) goto found_quote;
		}

		abort();
		found_quote:
		if(wArgs[iSeparator + 1] != ' ') abort();
		return wArgs + iSeparator + 2;
	}
}

// Report status to the Windows service manager
void ReportStatus(DWORD iState, DWORD iExitCode, DWORD iWaitHint) {
	static DWORD iCheckpoint = 1;

	_tServiceStatus.dwCurrentState = iState;
	_tServiceStatus.dwServiceSpecificExitCode = iExitCode;
	_tServiceStatus.dwWaitHint = iWaitHint;

	switch(iState) {
		case SERVICE_START_PENDING:
			_tServiceStatus.dwControlsAccepted = 0;
			_tServiceStatus.dwCheckPoint = iCheckpoint++;
			break;

		case SERVICE_RUNNING:
			_tServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
			_tServiceStatus.dwCheckPoint = 0;
			break;

		case SERVICE_STOPPED:
			_tServiceStatus.dwControlsAccepted = 0;
			_tServiceStatus.dwCheckPoint = 0;
			break;

		default:
			_tServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
			_tServiceStatus.dwCheckPoint = iCheckpoint++;
	}

	SetServiceStatus(_hServiceStatus, &_tServiceStatus);
}

// This is called when a service control command is received (Stop, Pause, Resume, etc.)
void ServiceControlHandler(DWORD iCommand) {
	switch(iCommand) {
		case SERVICE_CONTROL_STOP:
			SetEvent(_hStopEvent);
			break;
	}
}

void ServiceMain(DWORD iArgs, wchar_t** aArgs) {
	// Create a waitable event
	_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!_hStopEvent) abort();

	// This process only hosts a single service, thus OWN_PROCESS
	_tServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	// Handle control commands in the ServiceControlHandler function
	_hServiceStatus = RegisterServiceCtrlHandler(L"", ServiceControlHandler);
	if(!_hServiceStatus) abort();

	ReportStatus(SERVICE_START_PENDING, NO_ERROR, 5000);

	// Create a job object with the "KILL_ON_JOB_CLOSE" flag to ensure, that the
	// child process is killed when the service stops
	HANDLE hJob = CreateJobObjectW(NULL, NULL);
	if(!hJob) abort();

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION tJobExLimit = { .BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE };
	if(!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &tJobExLimit, sizeof(tJobExLimit))) abort();

	wchar_t* wCommandLine = wcsdup(GetCommandLineW());
	if(!wCommandLine) abort();

	wchar_t* wChildArguments = ParseArgumentString(wCommandLine);

	STARTUPINFOW tStartupInfo = {};
	PROCESS_INFORMATION tProcessInfo = {};
	if(!CreateProcessW(NULL, wChildArguments, NULL, NULL, FALSE, 0, NULL, NULL, &tStartupInfo, &tProcessInfo)) abort();
	if(!AssignProcessToJobObject(hJob, tProcessInfo.hProcess)) {
		TerminateProcess(tProcessInfo.hProcess, 1);
		abort();
	}

	CloseHandle(tProcessInfo.hThread);
	free(wCommandLine);

	// Wait for either a service stop command or the termination of the child process
	ReportStatus(SERVICE_RUNNING, NO_ERROR, 0);
	HANDLE aHandles[] = { _hStopEvent, tProcessInfo.hProcess };
	DWORD iStatus = WaitForMultipleObjects(2, aHandles, FALSE, INFINITE);

	ReportStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

	DWORD iExitCode;
	if(!GetExitCodeProcess(tProcessInfo.hProcess, &iExitCode)) abort();
	if(iExitCode == STILL_ACTIVE) iExitCode = 1;

	CloseHandle(tProcessInfo.hProcess);
	CloseHandle(hJob);
	CloseHandle(_hStopEvent);

	ReportStatus(SERVICE_STOPPED, iExitCode, 0);
}

// Entry point
void ProcessMain() {
	// Call ServiceMain if started as a service; print an error message otherwise
	if(!StartServiceCtrlDispatcherW((SERVICE_TABLE_ENTRYW[]){{L"", ServiceMain}, {NULL, NULL}})) {
		if(GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) abort();
		printf("This executable is meant to be started by the Windows service manager.\n");
		ExitProcess(1);
	}

	ExitProcess(0);
}
