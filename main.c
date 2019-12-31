// -lws2_32 -lpthread -I../mbedtls/include

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "http2.h"
#include "lite-list.h"

#define DEFAULT_HOST "openapi.youdao.com"
#define DEFAULT_PORT "403"

#ifndef container_of
#    define container_of(ptr, type, member) \
        ((type*)((char*)(ptr)-offsetof(type, member)))
#endif

typedef struct word
{
    char* buf;
    list_head_t list;
} word_t;

int contains(list_head_t* list, const char* s)
{
    word_t* pos;
    list_for_each_entry(pos, list, list, word_t)
    {
        if (strcmp(pos->buf, s) == 0)
            return 1;
    }
    return 0;
}

list_head_t* collect()
{

    // 初始化列表
    static LIST_HEAD(word_list);
    INIT_LIST_HEAD(&word_list);

    // 加载文本文件
    FILE* txt = fopen("words.txt", "r");
    if (!txt)
    {
        return 0;
    }
    // 单词最大长度
    size_t len = 30, i = 0;
    // 字符
    int c;
    // 单词
    char buf[len];
    memset(buf, 0, len);

    while ((c = fgetc(txt)) != EOF)
    {

        if (isalpha(c) && i < len)
        {
            // 连续英文字符转化为小写后
            // 写入相应的内存块
            buf[i++] = isupper(c) ? tolower(c) : c;
        }
        else if (buf[0] == 0 || strlen(buf) == 1)
        {
            // 如果单词为空或长度小于2
            // 继续下一个循环
            continue;
        }
        else
        {
            // 如果列表中不包括单词
            if (!contains(&word_list, buf))
            {
                word_t* word = malloc(sizeof(word_t));
                word->buf = strdup(buf);
                list_add_tail(&word->list, &word_list);
            }
            // 重设长度标记
            // 清空单词内存块
            i = 0;
            memset(buf, 0, len);
        }
    }
    fclose(txt);
    return &word_list;
}
int main()
{
#if defined(_WIN32)
    WSADATA d;
    if (WSAStartup(MAKEWORD(2, 2), &d))
    {
        printf("Failed to initialize.\n");
        return EXIT_FAILURE;
    }
#endif
    const char* host = DEFAULT_HOST;
    const char* port = DEFAULT_PORT;

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* peer_address;
    if (getaddrinfo(host, port, &hints, &peer_address))
    {
        printf("getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return EXIT_FAILURE;
    }

    char address_buf[128];
    char service_buf[128];

    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
        address_buf, sizeof(address_buf),
        service_buf, sizeof(service_buf),
        NI_NUMERICHOST);

    printf("%s %s\n", address_buf, service_buf);

    //collect();

    // word_t *pos, *tmp;
    // list_for_each_entry_safe(pos, tmp, &word_list, list, word_t)
    // {
    //     printf("%s\n", pos->buf);
    //     list_del(&pos->list);
    //     free(pos->buf);
    //     free(pos);
    // }

#if defined(_WIN32)
    WSACleanup();
#endif
    return EXIT_SUCCESS;
}