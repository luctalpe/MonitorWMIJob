// MonitorWmiJob.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

HANDLE hJob = NULL;
HANDLE hCompletionPort = NULL;
JOBOBJECT_ASSOCIATE_COMPLETION_PORT JobCompletionPort;
#define LTERROR(b, x,y)  if( b) { printf("Error: %s %s 0x%x\n", #x, #y, GetLastError()); return GetLastError();} else printf("Ok: %s %s\n", #x,#y );

bool consoleHandler(int signal) {

	if (hJob != NULL) {
		JobCompletionPort.CompletionKey = 0;
		JobCompletionPort.CompletionPort = NULL;
		BOOL bStatus = SetInformationJobObject(hJob, JobObjectAssociateCompletionPortInformation, &JobCompletionPort, sizeof(JobCompletionPort));
		LTERROR( !bStatus, SetInformationJobObject, WmiProviderSubSystemHostJob);
		CloseHandle(hCompletionPort);
		hCompletionPort = NULL;
		CloseHandle(hJob);
		hJob = NULL;
	}
	return true;
}

WCHAR Path[5000];
WCHAR FullPath[5000];


BOOL ExecuteCmd(DWORD dwWaitTimeout, DWORD Pid) {

	SYSTEMTIME st;
	GetLocalTime(&st);
	WCHAR*  TimeCur = Path;

	GetModuleFileNameW(NULL, Path, sizeof(Path) / sizeof(WCHAR));
	WCHAR *p = NULL;
	GetFullPathNameW(Path, sizeof(FullPath) / 2, FullPath, &p);
	if (p) *p = 0;

	WCHAR pCmd[5000];
	swprintf(pCmd, sizeof(pCmd)/sizeof(pCmd[0]), L"cmd.exe /c \"%s%s\" %d", FullPath, L"script.cmd", Pid);

	swprintf(Path, sizeof(Path) / sizeof(Path[0]),  L"%s%2.2d_%2.2d_%4.4d__%2.2d_%2.2d_%2.2d_%3.3d", FullPath, st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);


	if (!CreateDirectoryW(Path, NULL)) {
		LTERROR(true, CreateDirectoryW, Path);
		return FALSE;
	}

	STARTUPINFO          StartupInfo;
	PROCESS_INFORMATION  ProcessInformation = { 0 };

	memset(&StartupInfo, 0, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(STARTUPINFO);
	StartupInfo.dwFlags = 0;
	StartupInfo.hStdOutput = NULL;
	StartupInfo.hStdError = NULL;
	StartupInfo.hStdInput = NULL;
	BOOL bStatus = FALSE;
	// run a batch script without creating a window
	printf("Executing %S\n", pCmd);
	printf("in Path %S\n", FullPath);

	bStatus = CreateProcessW(NULL, pCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_DEFAULT_ERROR_MODE, NULL, FullPath, &StartupInfo, &ProcessInformation);


	if (bStatus == FALSE) {
		printf("CreateProcess Failed %S %d\n", pCmd, GetLastError());
		return FALSE;
	}
	else {
		DWORD dwStatus = WaitForSingleObject(ProcessInformation.hProcess, dwWaitTimeout);
		if (dwStatus == WAIT_ABANDONED) {
			printf("%S wait abandonned\n", pCmd);
		}
		else {

			DWORD dwExitCode;
			GetExitCodeProcess(ProcessInformation.hProcess, &dwExitCode);
			printf("%S exited with code %d\n", pCmd, dwExitCode);
		}
		CloseHandle(ProcessInformation.hProcess);
		CloseHandle(ProcessInformation.hThread);
	}

	return TRUE;

}

void DisplayLimitJob(HANDLE hJob, BOOL bForce = FALSE) {
	static ULONG64 DetectedJobPeakLimit = 0, DetectedProcessPeakLimit = 0;


	if (hJob != NULL) {
		JOBOBJECT_LIMIT_VIOLATION_INFORMATION JobLimitViolationInfo = { 0 };

		BOOL bRet = QueryInformationJobObject(hJob, JobObjectLimitViolationInformation, &JobLimitViolationInfo, sizeof(JobLimitViolationInfo), NULL);
		if (bRet) {
			if (JobLimitViolationInfo.LimitFlags & JOB_OBJECT_LIMIT_JOB_MEMORY) {
				printf("JobMemoryLimit (MBs) = %d\n", (DWORD)(JobLimitViolationInfo.JobMemoryLimit / 0x100000));

				if (JobLimitViolationInfo.ViolationLimitFlags & JOB_OBJECT_LIMIT_JOB_MEMORY) {
					printf("JobMemoryViolation Limit (MBs) = %d\n", (DWORD)(JobLimitViolationInfo.JobMemory / 0x100000));

				}
			}
		}
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobExtendedLimitInfo;
		bRet = QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation, &JobExtendedLimitInfo, sizeof(JobExtendedLimitInfo), NULL);
		if (bRet) {
			printf("Job Limit Flags : 0x%x\n", JobExtendedLimitInfo.BasicLimitInformation.LimitFlags);
			if ((JobExtendedLimitInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)) {
				//if (bForce || (DetectedProcessPeakLimit < JobExtendedLimitInfo.PeakProcessMemoryUsed ) ) {
				printf("ProcessMemoryLimit (MBs) = %d\n", (DWORD)(JobExtendedLimitInfo.ProcessMemoryLimit / 0x100000));
				printf("Peak Process Memory(MBs) = %d\n", (DWORD)(JobExtendedLimitInfo.PeakProcessMemoryUsed / 0x100000));
				DetectedProcessPeakLimit = JobExtendedLimitInfo.PeakProcessMemoryUsed;
				//}
			}
			if (JobExtendedLimitInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_JOB_MEMORY) {
				// if (bForce ||(DetectedJobPeakLimit < JobExtendedLimitInfo.PeakJobMemoryUsed))  {
				printf("JobMemoryLimit     (MBs) = %d\n", (DWORD)(JobExtendedLimitInfo.JobMemoryLimit / 0x100000));
				printf("Peak Job Memory    (MBs) = %d\n", (DWORD)(JobExtendedLimitInfo.PeakJobMemoryUsed / 0x100000));
				// }
				DetectedJobPeakLimit = JobExtendedLimitInfo.PeakJobMemoryUsed;

			}
			if ((JobExtendedLimitInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS)) {
				printf("ActiveProcessLimit        = %d\n", (DWORD)(JobExtendedLimitInfo.BasicLimitInformation.ActiveProcessLimit));
			}
			if ((JobExtendedLimitInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK)) {
				printf("CREATE_BREAKAWAY_FROM_JOB\n");
			}
		}

	}
}

WCHAR CmdLine[2500];
BOOL bDisplay = FALSE;
BOOL bIdna = FALSE;
BOOL bMonitor = FALSE;

DWORD JobAccess;
int main()
{
	WCHAR *p = GetCommandLineW();
	swprintf(CmdLine, sizeof(CmdLine)/sizeof(CmdLine[0]) ,  L"%s", GetCommandLineW());

	_wcsupr_s(CmdLine);
	if (wcsstr(CmdLine, L"-DISPLAY") != NULL) { bDisplay = TRUE; }
	if (wcsstr(CmdLine, L"-IDNA") != NULL) { bIdna = TRUE; }
	if (wcsstr(CmdLine, L"-MONITOR") != NULL) { bMonitor = TRUE; }

	if (!(bDisplay | bIdna | bMonitor)) {
		printf("Usage : MonWMIJob [-DISPLAY] [-IDNA] [-MONITOR]\n\n");
		printf("\t-DISPLAY :\n\t\tprint the memory quotas and stats for WMI Job Global\\WmiProviderSubSystemHostJob\n");
		printf("\t-MONITOR :\n\t\twill monitor the WMI job\n\t\tand will execute script.cmd, the first time a job memory quota violation will occur\n");
		printf("\t-IDNA    :\n\t\twill suspend quota limits to allow IDNA (TTTRacer.exe) to capture traces\n\t\tQuotas limitation will be restored at exit time\n\n");
		printf("Warning: to access to this WMI Job, you need to run this program in a system context (you can use psexec -i -s cmd.exe from sysinternals)\n\n");

		return 1;
	}

	JobAccess = JOB_OBJECT_QUERY;
	hJob = OpenJobObjectW(JobAccess, FALSE, L"Global\\WmiProviderSubSystemHostJob");
	LTERROR((hJob == NULL), OpenJobObjectW, WmiProviderSubSystemHostJob);
	printf("\nCurrent limits:\n===============\n");
	DisplayLimitJob(hJob);
	CloseHandle(hJob);
	hJob = NULL;

	BOOL bRet;
	if (bIdna | bMonitor) {
		JobAccess = JobAccess | JOB_OBJECT_SET_ATTRIBUTES;
		hJob = OpenJobObjectW(JobAccess, FALSE, L"Global\\WmiProviderSubSystemHostJob");
		LTERROR((hJob == NULL), OpenJobObjectW, WmiProviderSubSystemHostJob);
	}
	else
		return 0;


	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	if (bIdna) {
		JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobExtendedLimitInfo;
		bRet = QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation, &JobExtendedLimitInfo, sizeof(JobExtendedLimitInfo), NULL);
		if (bRet) {
			JOBOBJECT_EXTENDED_LIMIT_INFORMATION JobExtendedLimitInfoNew = JobExtendedLimitInfo;

			// JobExtendedLimitInfoNew.ProcessMemoryLimit = 3221225472; // 3GB
			// JobExtendedLimitInfoNew.JobMemoryLimit = 4294967296; // 4GB

			JobExtendedLimitInfoNew.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
			bRet = SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &JobExtendedLimitInfoNew, sizeof(JobExtendedLimitInfoNew));
			LTERROR((bRet == FALSE), SetInformationJobObject, WmiProviderSubSystemHostJob);
		}
		else {
			LTERROR((bRet == FALSE), QueryInformationJobObject, WmiProviderSubSystemHostJob);
		}
		printf("\nNew limits:\n==========\n");
		DisplayLimitJob(hJob, TRUE);
		printf("\nYou can now collect a IDNA (TTTracer) trace for a WMIPRVSE process.\nWhen finished press enter to restore the previous quotas\n");
		getchar();

		bRet = SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &JobExtendedLimitInfo, sizeof(JobExtendedLimitInfo));
		LTERROR((bRet == FALSE), SetInformationJobObject, WmiProviderSubSystemHostJob);
		printf("\nAfter restoring limits:\n=======================\n");


	}

	DisplayLimitJob(hJob);
	if (!bMonitor) { return 1; }



	hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	LTERROR(hCompletionPort == NULL, CreateIoCompletionPort, WmiProviderSubSystemHostJob);

	JobCompletionPort.CompletionKey = (void*)1;
	JobCompletionPort.CompletionPort = hCompletionPort;

	BOOL bStatus = SetInformationJobObject(hJob, JobObjectAssociateCompletionPortInformation, &JobCompletionPort, sizeof(JobCompletionPort));
	LTERROR(bStatus == 0, SetInformationJobObject, WmiProviderSubSystemHostJob);

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)consoleHandler, TRUE);

	DWORD NumberOfBytes = 0;
	ULONG_PTR CompletionKey = 0;
	LPOVERLAPPED Overlapped = NULL;
	static int Nb = 0;

	while (GetQueuedCompletionStatus(hCompletionPort, &NumberOfBytes, &CompletionKey, &Overlapped, INFINITE)) {
		DWORD ProcessId;
		SYSTEMTIME lt;
		GetLocalTime(&lt);

		printf("\n%4.4d/%2.2d/%2.2d %2.2d:%2.2d:%2.2d.%3.3d ", lt.wYear, lt.wMonth, lt.wDay, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds); fflush(stdout);

		switch (NumberOfBytes) {
		case JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS:
			ProcessId = (DWORD)Overlapped;
			printf("JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS : Pid  %d\n", ProcessId);
			break;
		case JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT:
			printf("JOB_OBJECT_MSG_ACTIVE_PROCESS_LIMIT \n");
			break;
		case JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO:
			printf("JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO\n");
			break;
		case JOB_OBJECT_MSG_END_OF_JOB_TIME:
			printf("JOB_OBJECT_MSG_END_OF_JOB_TIME\n");
			goto end;
			break;
		case JOB_OBJECT_MSG_END_OF_PROCESS_TIME:
			ProcessId = (DWORD)Overlapped;
			printf("JOB_OBJECT_MSG_END_OF_PROCESS_TIME : Pid %d\n", ProcessId);
			break;
		case JOB_OBJECT_MSG_EXIT_PROCESS:
			ProcessId = (DWORD)Overlapped;
			printf("JOB_OBJECT_MSG_END_OF_PROCESS_TIME : Pid %d\n", ProcessId);
			break;
		case JOB_OBJECT_MSG_JOB_MEMORY_LIMIT:

			ProcessId = (DWORD)Overlapped;
			Nb++;
			if (Nb < 2) { ExecuteCmd(15 * 1000, ProcessId); }

			printf("JOB_OBJECT_MSG_JOB_MEMORY_LIMIT : caused by process %d\n", ProcessId);
			DisplayLimitJob(hJob);


			break;
		case JOB_OBJECT_MSG_NEW_PROCESS:
			ProcessId = (DWORD)Overlapped;
			printf("JOB_OBJECT_MSG_NEW_PROCESS : New process %d\n", ProcessId);
			break;
		case JOB_OBJECT_MSG_NOTIFICATION_LIMIT:
			ProcessId = (DWORD)Overlapped;
			printf("JOB_OBJECT_MSG_NOTIFICATION_LIMIT : caused process %d\n", ProcessId);
			DisplayLimitJob(hJob);
			break;
		case JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT:
			ProcessId = (DWORD)Overlapped;
			printf("JOB_OBJECT_MSG_PROCESS_MEMORY_LIMIT : process %d\n", ProcessId);
			DisplayLimitJob(hJob);

		}
	}

end:
	CloseHandle(hCompletionPort);

	return 0;
}

