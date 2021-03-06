#include "platform.h"
#include <Windows.h>
#include <winnt.h>
#include <profileapi.h>
#include <string>
#include <codecvt>
#include "util_uint128.h"

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;
static inline uint64_t get_clockfreq(void)
{
	if (!have_clockfreq) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = true;
	}

	return clock_freq.QuadPart;
}

uint64_t os_gettime_ns(void)
{
	LARGE_INTEGER current_time;
	double time_val;

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)get_clockfreq();

	return (uint64_t)time_val;
}


uint64_t util_mul_div64(uint64_t num, uint64_t mul, uint64_t div)
{
#if defined(_MSC_VER) && defined(_M_X64)
	unsigned __int64 high;
	const unsigned __int64 low = _umul128(num, mul, &high);
	unsigned __int64 rem;
	return _udiv128(high, low, div, &rem);
#else
	const uint64_t rem = num % div;
	return (num / div) * mul + (rem * mul) / div;
#endif
}

bool initialize_com(void)
{
	const HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
	const bool success = SUCCEEDED(hr);
	return success;
}

void uninitialize_com(void)
{
	CoUninitialize();
}

uint64_t audio_frames_to_ns(size_t sample_rate, uint64_t frames)
{
	util_uint128_t val;
	val = util_mul64_64(frames, 1000000000ULL);
	val = util_div128_32(val, (uint32_t)sample_rate);
	return val.low;
}

const wchar_t* os_get_run_dev_wpath()
{
	static std::wstring runpath = L"";
	
	if (runpath.empty())
	{
		wchar_t dll_path[MAX_PATH];
		DWORD size = GetModuleFileNameW(NULL, dll_path, MAX_PATH);
		runpath = std::wstring(dll_path);
		runpath = runpath.substr(0, runpath.find_last_of(L"\\"));
	}

	return runpath.c_str();
}

const char* os_get_run_dev_path()
{
	static std::string runpath = "";

	if (runpath.empty())
	{
		wchar_t dll_path[MAX_PATH];
		DWORD size = GetModuleFileNameW(NULL, dll_path, MAX_PATH);
		std::wstring wrunpath = std::wstring(dll_path);
		wrunpath = wrunpath.substr(0, wrunpath.find_last_of(L"\\"));
		std::wstring_convert<std::codecvt_utf8<wchar_t>> cv;
		runpath = cv.to_bytes(wrunpath);
	}

	return runpath.c_str();
}

int UsSleep(int us)
{
	//储存计数的联合
	LARGE_INTEGER fre;
	//获取硬件支持的高精度计数器的频率
	if (QueryPerformanceFrequency(&fre))
	{
		LARGE_INTEGER run, priv, curr;
		run.QuadPart = fre.QuadPart * us / 1000000;//转换为微妙级
		//获取高精度计数器数值
		QueryPerformanceCounter(&priv);
		do
		{
			QueryPerformanceCounter(&curr);
		} while (curr.QuadPart - priv.QuadPart < run.QuadPart);
		curr.QuadPart -= priv.QuadPart;
		int nres = (curr.QuadPart * 1000000 / fre.QuadPart);//实际使用微秒时间
		return nres;
	}
	return -1;//
}

bool os_sleepto_ns(uint64_t time_target)
{
	uint64_t t = os_gettime_ns();
	uint32_t milliseconds;

	if (t >= time_target)
		return false;

	milliseconds = (uint32_t)((time_target - t) / 1000000);
	if (milliseconds > 1)
		Sleep(milliseconds - 1);

	for (;;) {
		t = os_gettime_ns();
		if (t >= time_target)
			return true;

#if 0
		Sleep(1);
#else
		Sleep(0);
#endif
	}
}