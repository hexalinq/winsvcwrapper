#include <windows.h>

int printf(const char*, ...);

HANDLE _hStopEvent = NULL;
SERVICE_STATUS _tServiceStatus = {};
SERVICE_STATUS_HANDLE _hServiceStatus = NULL;

void __attribute__((noreturn)) abort() {
	asm("int3");
	ExitProcess(1);
	*(void**)0;
}

wchar_t* wcsdup(const wchar_t* wString) {
	size_t iBytes = (wcslen(wString) + 1) * sizeof(wchar_t);
	wchar_t* wNew = malloc(iBytes);
	if(!wNew) abort();
	memcpy(wNew, wString, iBytes);
	return wNew;
}

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

void ServiceControlHandler(DWORD iCommand) {
	switch(iCommand) {
		case SERVICE_CONTROL_STOP:
			SetEvent(_hStopEvent);
			break;
	}
}

void ServiceMain(DWORD iArgs, wchar_t** aArgs) {
	_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if(!_hStopEvent) abort();

	_tServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;

	_hServiceStatus = RegisterServiceCtrlHandler(L"", ServiceControlHandler);
	if(!_hServiceStatus) abort();

	ReportStatus(SERVICE_START_PENDING, NO_ERROR, 5000);

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

void ProcessMain() {
	if(!StartServiceCtrlDispatcherW((SERVICE_TABLE_ENTRYW[]){{L"", ServiceMain}, {NULL, NULL}})) {
		if(GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) abort();
		printf("This executable is meant to be started by the Windows service manager.\n");
		ExitProcess(1);
	}

	ExitProcess(0);
}
