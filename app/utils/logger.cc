#include "logger.h"
#include <mutex>
#include <Windows.h>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

Logger* Logger::instance_ = nullptr;
std::once_flag oc_;


Logger::Logger()
{

}

Logger::~Logger()
{
    if (log_file_)
    {
        fclose(log_file_);
        log_file_ = nullptr;
    }
}

Logger* Logger::GetInstance()
{
    std::call_once(oc_, [&]() { instance_ = new Logger(); });
    return instance_;
}

void Logger::Release()
{
    delete this;
}

void Logger::InitLogger(bool bLogFile, std::string path)
{
    if (bLogFile)
    {
        std::string logPath = path;
        if (logPath.empty())
        {
            DWORD tick = ::GetTickCount();
            DWORD id = ::GetCurrentProcessId();
            logPath = "C:\\test";
            logPath += std::to_string(tick);
            logPath += ("_" + std::to_string(id));
            logPath += ".txt";
        }
        log_file_ = fopen(logPath.c_str(), "wb+");
        char dll_path[MAX_PATH];
        DWORD size = GetModuleFileNameA(NULL, dll_path, MAX_PATH);
        std::string path = std::string(dll_path);
    }
}

std::time_t getTimeStamp()
{
    std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
    std::time_t timestamp = tmp.count();
    return timestamp;
}

const char* Logger::MakeUpException(const char* classname, const char* funtion, const char* format, ...)
{
    last_error_.clear();
    std::string var_str;
    va_list ap;
    va_start(ap, format);
    int len = _vscprintf(format, ap);
    if (len > 0)
    {
        std::vector<char> buf(len + 1);
        vsprintf(&buf.front(), format, ap);
        var_str.assign(buf.begin(), buf.end() - 1);
    }
    va_end(ap);

    last_error_ += std::string(classname);
    last_error_ += "\r\n";
    last_error_ += std::string(funtion);
    last_error_ += " ";
    last_error_ += var_str;

    return last_error_.c_str();
}

void Logger::WriteLog(const char* prestring, int line, const char* function, const char* format, ...)
{
    std::string var_str;
    va_list ap;
    va_start(ap, format);
    int len = _vscprintf(format, ap);
    if (len > 0)
    {
        std::vector<char> buf(len + 1);
        vsprintf(&buf.front(), format, ap);
        var_str.assign(buf.begin(), buf.end() - 1);
    }
    va_end(ap);
    std::time_t milli = getTimeStamp()+ (std::time_t)8*60*60*1000;//此处转化为东八区北京时间，如果是其它时区需要按需求修改
    auto mTime = std::chrono::milliseconds(milli);
    auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(mTime);
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm* now = std::gmtime(&tt);
    char* str_time = (char*)malloc(256);
    memset(str_time, 0, 256);
    sprintf(str_time, "%4d-%02d-%02d %02d:%02d:%02d.%d", now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, milli % 1000);

    
    if (log_file_)
    {
        std::string head = " ";
        head += std::string(prestring);
        head += "[";
        head += std::string(function);
        head += ",";
        head += std::to_string(line);
        head += "] ";
        std::string output = std::string(str_time) + head + " " + var_str + "\r\n";
        fwrite(output.c_str(), output.length(), 1, log_file_);
    }
    else
    {
        std::string head = " ";
        head += std::string(prestring);
        head += "[";
        head += std::string(function);
        head += ",";
        head += std::to_string(line);
        head += "] ";
        OutputDebugStringA(str_time);
        OutputDebugStringA(head.c_str());
        OutputDebugStringA(" ");
        OutputDebugStringA(var_str.c_str());
        OutputDebugStringA("\r\n");
    }

    if (str_time)
    {
        free(str_time);
        str_time = nullptr;
    }
}