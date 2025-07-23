#include "cc_common.h"

int g_depth = 0;
int g_log_num = 0;

std::vector<std::string> g_log_filter = {
// "RdpClientEntry", "cc_client_global_init", "cc_UpdateSurfaces",
//     "cc_end_paint"
    };

double GetUS()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000000.0 * ts.tv_sec + 0.001 * ts.tv_nsec;
}