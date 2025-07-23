#ifndef __CC_COMMON_H__
#define __CC_COMMON_H__

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

extern int g_depth;
extern int g_log_num;
extern std::vector<std::string> g_log_filter;

#include <hilog/log.h>
#define APP_LOG_DOMAIN 0x0001
#define APP_LOG_TAG "ccnto"
#define CLOGI(...) ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))
#define CLOGD(...) ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))
#define CLOGW(...) ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))
#define CLOGE(...) ((void)OH_LOG_Print(LOG_APP, LOG_ERROR, APP_LOG_DOMAIN, APP_LOG_TAG, __VA_ARGS__))

double GetUS();

class CTrace {
public:
    CTrace(const std::string &func)
    {
        if (std::find(g_log_filter.begin(), g_log_filter.end(), func) != g_log_filter.end()) {
            pass_ = true;
            return;
        }

        func_ = std::move(func);
        depth_ = g_depth++;
        char buf[1024];
        sprintf(buf,"T %5d", g_log_num++);
        for (int i = 0; i < depth_; i++)
            strcat(buf,"    ");
        sprintf(&buf[strlen(buf)],"%s {\n", func_.c_str());
        CLOGE("%{public}s",buf);
    }
    ~CTrace()
    {
        if (pass_) {
            return;
        }
        char buf[1024];
        sprintf(buf,"T %5d", g_log_num++);
        for (int i = 0; i < depth_; i++)
            strcat(buf, "    ");
        sprintf(&buf[strlen(buf)], "}\n");
        CLOGE("%{public}s", buf);
        // printf("} %s\n", func_.c_str());
        g_depth--;
    }

private:
    std::string func_;
    int depth_;
    bool pass_ = false;
};

struct CData {
    uint32_t x, y, w, h;
    unsigned char *data;
};
class CQueueUpdater {
public:
    static CQueueUpdater &GetInstance()
    {
        static CQueueUpdater instance;
        return instance;
    }
    void Add(
        uint32_t dx, uint32_t dy, unsigned char *data, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t stride)
    {
        CData cdata = { .x = dx, .y = dy, .w = w, .h = h, .data = (unsigned char *)malloc(w * h * 4) };

        for (int i = 0; i < h; i++) {
            memcpy(cdata.data + i * w * 4, data + ((y + i) * stride) + (x * 4), w * 4);
        }
        queue_.push(cdata);
    }
    void DoUpdate(std::function<void(CData &)> func)
    {
        while (!queue_.empty()) {
            CData cdata = queue_.front();
            queue_.pop();
            func(cdata);
            free(cdata.data);
        }
    }
    int GetQueueSize()
    {
        return queue_.size();
    }

private:
    CQueueUpdater()
    {
    }
    std::queue<CData> queue_;
};

#endif