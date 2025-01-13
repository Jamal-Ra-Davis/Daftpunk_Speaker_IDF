#include "Time_Helpers.h"
#include <stdbool.h>

void get_time_components(time_t *ts, int *hour, int *min, int *sec, bool *am)
{
    struct tm timeinfo;
    time(ts);
    localtime_r(ts, &timeinfo);
    int hour_ = timeinfo.tm_hour;
    *am = (hour_ < 12);
    hour_ = hour_ % 12;

    if (hour_ == 0) {
        *hour = 12;
    }
    else {
        *hour = hour_;
    }
    *min = timeinfo.tm_min;
    *sec = timeinfo.tm_sec;
    
}
void set_time_components(int hour, int min, int sec)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    // Set timezone to Pacific Standard Time
    setenv("TZ", "PST", 1);
    tzset();
    localtime_r(&now, &timeinfo);

    timeinfo.tm_hour = hour;
    timeinfo.tm_min = min;
    timeinfo.tm_sec = sec;
    time_t new_time = mktime(&timeinfo);
    struct timeval ts = {
        .tv_sec = new_time,
    };

    settimeofday(&ts, NULL);
    time(&now);
    localtime_r(&now, &timeinfo);
}
int convert_24hour_to_12hour(int hour)
{
    int hour_ = hour % 12;

    if (hour_ == 0) {
        hour_ = 12;
    }
    return hour_;
}