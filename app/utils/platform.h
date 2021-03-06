#pragma once
#include <stdint.h>

uint64_t os_gettime_ns(void);
uint64_t util_mul_div64(uint64_t num, uint64_t mul, uint64_t div);
bool initialize_com();
void uninitialize_com();
uint64_t audio_frames_to_ns(size_t sample_rate, uint64_t frames);
const wchar_t* os_get_run_dev_wpath();
const char* os_get_run_dev_path();
bool os_sleepto_ns(uint64_t time_target);
int UsSleep(int us);