#include <Windows.h>
#include <cstdio>

CHAR* FormatLastError(CHAR szBuffer[], DWORD dwError)
{
	LPSTR lpError;
	FormatMessage
	(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dwError,
		MAKELANGID(LANG_NEUTRAL, LANG_SYSTEM_DEFAULT),
		(LPSTR)&lpError,
		NULL,
		NULL
	);
	//cout << lpError << endl;
	sprintf(szBuffer, "Error %i:%s", dwError, lpError);
	LocalFree(lpError);
	return szBuffer;
}