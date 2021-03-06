#pragma once

#include <string>
#include <fstream>

#define LOGGER_INFO(...)\
{\
	Logger::GetInstance()->WriteLog("[INFO]", __LINE__, __FUNCTION__, __VA_ARGS__);\
}

#define LOGGER_ERROR(...)\
{\
	Logger::GetInstance()->WriteLog("[ERROR]", __LINE__, __FUNCTION__, __VA_ARGS__);\
}

#define EXCEPTION_TEXT(...)\
{\
	throw Logger::GetInstance()->MakeUpException(typeid(this).name(), __FUNCTION__, __VA_ARGS__); \
}

#ifndef HR_EXCEPTION
#define HR_EXCEPTION(x)												\
	{															\
		HRESULT hr = (x);										\
		if(FAILED(hr))											\
		{														\
			EXCEPTION_TEXT("hr:0x%x", hr);	\
		}														\
	}
#endif

class Logger
{
private:
	Logger();
	~Logger();
	Logger(const Logger&) = delete;
	Logger& operator = (const Logger&) = delete;

public:
	static Logger* GetInstance();
	void Release();

	void InitLogger(bool bLogFile = true, std::string path = "");
	void WriteLog(const char* prestring, int line, const char* function, const char* format, ...);
	const char* MakeUpException(const char* classname, const char* funtion, const char* format, ...);

private:
	static Logger* instance_;
	FILE* log_file_ = nullptr;
	std::string last_error_ = "";
};