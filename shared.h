#ifndef SHARED_H__
#define SHARED_H__

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define LOG_COLOURED 1
#define DEBUG 0

#ifdef LOG_COLOURED
#    define GRAY(s) "\033[1;30m" s "\033[0m"
#    define RED(s) "\033[0;31m" s "\033[0m"
#    define GREEN(s) "\033[0;32m" s "\033[0m"
#    define YELLOW(s) "\033[1;33m" s "\033[0m"
#else
#    define GRAY(s) s
#    define RED(s) s
#    define GREEN(s) s
#    define YELLOW(s) s
#endif

#ifdef NDEBUG
#    define debug(M, ...)
#else
#    define debug(M, ...) fprintf(stderr, "DEBUG %s:%d: " M "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define log(format, loglevel, ...) printf("%s " format "\n", loglevel, ##__VA_ARGS__)

#define clean_errno() (errno == 0 ? "None" : strerror(errno))

#define log_err(format, ...) log(format, RED("ERR"), ##__VA_ARGS__)
#define log_fatal(format, ...)                \
    log(format, RED("FATAL"), ##__VA_ARGS__); \
    exit(1)
#define log_warn(format, ...) log(format, YELLOW("WARN"), ##__VA_ARGS__)
#define log_info(format, ...) log(format, GREEN("INFO"), ##__VA_ARGS__)

#define log_dbg(format, ...) \
    if (DEBUG)               \
    log("[%s:%d] " format, GREEN("DBG"), __FILE__, __LINE__, ##__VA_ARGS__)

void log_pid(const char* msg);

#define check(A, M, ...)           \
    if (!(A))                      \
    {                              \
        log_err(M, ##__VA_ARGS__); \
        errno = 0;                 \
        goto error;                \
    }

#define sentinel(M, ...)           \
    {                              \
        log_err(M, ##__VA_ARGS__); \
        errno = 0;                 \
        goto error;                \
    }

#define check_mem(A) check((A), "Out of memory.")

#define check_debug(A, M, ...)   \
    if (!(A))                    \
    {                            \
        debug(M, ##__VA_ARGS__); \
        errno = 0;               \
        goto error;              \
    }
int indexof(const char* s1, const char* s2)
{
    if (s1 == NULL || s2 == NULL || *s1 == 0 || *s2 == 0)
    {
        return -1;
    }
    const char* p1 = s1;

    size_t len = strlen(p1);

    for (size_t i = 0; i < len; i++)
    {
        if (p1[i] == *s2)
        {
            const char* p2 = s2;
            while (*p2 && *p2 == p1[i])
            {
                i++;
                p2++;
            }
            if (*p2 == 0)
                return i;
        }
    }
    return -1;
}

static uint64_t _linux_get_time_ms(void)
{
#if defined(_WIN32)
    return GetTickCount64();
#else
    struct timeval tv = { 0 };
    uint64_t time_ms;

    gettimeofday(&tv, NULL);

    time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    return time_ms;
#endif
}

static uint64_t _linux_time_left(uint64_t t_end, uint64_t t_now)
{
    uint64_t t_left;

    if (t_end > t_now)
    {
        t_left = t_end - t_now;
    }
    else
    {
        t_left = 0;
    }

    return t_left;
}
#endif
