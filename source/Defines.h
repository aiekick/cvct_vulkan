#ifndef DEFINES_H
#define DEFINES_H

#include <assert.h>
#include <Windows.h>

#define _STR(x) #x
#define STR(x) _STR(x)
#define LOG(severity,format,...) printf(STR(__FILE__) ": " STR(__LINE__) "][" severity"] " format "\n",__VA_ARGS__)
#define RETURN_ERROR(errcode,format,...) return LOG("CRITICAL", format, __VA_ARGS__),assert(0),errcode
#define ERROR_VOID(format,...) return LOG("CRITICAL", format, __VA_ARGS__),assert(0)

#define AXISCOUNT 3

// in seconds
inline float Ctime()
{
	static __int64 start = 0;
	static __int64 frequency = 0;

	if (start == 0)
	{
		QueryPerformanceCounter((LARGE_INTEGER*)&start);
		QueryPerformanceFrequency((LARGE_INTEGER*)&frequency);
		return 0.0f;
	}

	__int64 counter = 0;
	QueryPerformanceCounter((LARGE_INTEGER*)&counter);
	return (float)((counter - start) / double(frequency));
}

#endif	//DEFINES_H