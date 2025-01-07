#include "esp_sntp.h"

void get_time_components(time_t *ts, int *hour, int *min, int *sec, bool *am);
void set_time_components(int hour, int min, int sec);
int convert_24hour_to_12hour(int hour);