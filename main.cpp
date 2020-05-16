
#include <stdio.h>

#include "rekka/rekka.h"

#include <time.h>
#include <vector>

#include "rekka/script_core.h"
#include "rekka/scheduler.h"
#include "rekka/core.h"

#ifdef WIN32
#include <windows.h>
#include <dbghelp.h>
LONG WINAPI _ExpetionFilter(EXCEPTION_POINTERS *pExptInfo)
{	
	LONG ret = EXCEPTION_CONTINUE_SEARCH;
	char szExePath[MAX_PATH] = { 0 };
	if (GetModuleFileNameA(NULL, szExePath, MAX_PATH) > 0) {
		char* p = strrchr(szExePath, '\\');
		if (p == NULL) return ret;
		*p = 0;
		strcat(szExePath, "\\minidump.dmp");
	}	
	HANDLE hFile = ::CreateFileA(szExePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile != INVALID_HANDLE_VALUE) {		
		MINIDUMP_EXCEPTION_INFORMATION exptInfo;
		exptInfo.ThreadId = ::GetCurrentThreadId();
		exptInfo.ExceptionPointers = pExptInfo;
		if (::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), hFile, MiniDumpNormal, &exptInfo, NULL, NULL)) {
			ret = EXCEPTION_EXECUTE_HANDLER;
		}
	}
	return ret;
}
#endif

USING_NS_REK;

int main(int argc, char *args[])
{
	//freopen("xxx.log", "w", stdout);
	// seed random number generator
	srand(time(0));
#ifdef WIN32
	::SetUnhandledExceptionFilter(_ExpetionFilter);
#endif
	ScriptCore* scripter = ScriptCore::getInstance();
	scripter->initEngine(32L * 1024L * 1024L, 32 * 1024);
	Core* core = Core::getInstance();
	core->init();	
	
	// run script	
	scripter->runScript("rekka.js");
	scripter->runScript("index.js");
	
	// main loop
	core->run();
		
	// shutdown
	Core::destroyInstance();	
	ScriptCore::destroyInstance();
	fflush(stdout);
	return 0;
}


