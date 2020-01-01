// -lws2_32 -lpthread -I../sqlite -L../sqlite -lsqlite3 tmd5/tmd5.c cJSON/cJSON.c
// -I../mbedtls/include

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sqlite3.h>
#include "tmd5/tmd5.h"
#include "cJSON/cJSON.h"
#include "http2.h"
#include "lite-list.h"

#include "rapidstring.h"

#if defined(_WIN32)
#    include <conio.h>
#endif

#define API_KEY "6543644a29c7b7d3"
#define API_SECRET "fv5H0x6bcz2AeJ5CCebHytQEr1TEOWLt"

#define ERR_TCP_WRITE_TIMEOUT 11;
#define ERR_TCP_WRITE_FAIL 12
#define RET_SUCCESS 0
#define ERR_TCP_READ_TIMEOUT 13
#define ERR_TCP_PEER_SHUTDOWN 14
#define ERR_TCP_READ_FAIL 15
#define ERR_TCP_NOTHING_TO_READ 16
// http://openapi.youdao.com/api?q=word&salt=1577810414&sign=9164952137F0281F2ADF734A8835900C&from=EN&appKey=1f5687b5a6b94361&to=zh-CHS
#define DEFAULT_HOST "openapi.youdao.com"

#ifdef USE_HTTPS
#    define DEFAULT_PORT 403
#else
#    define DEFAULT_PORT 80
#endif

#define DBNAME "youdao.db"
#define SQL_CREATE_TABLE "CREATE TABLE IF NOT EXISTS \"dic\" ( \"key\" varchar , \"word\" varchar, \"learned\" INTEGER)"
#define SQL_CREATE_INDEX "CREATE UNIQUE INDEX IF NOT EXISTS `key_UNIQUE` ON `dic` (`key` ASC)"
#define SQL_QUERY "SELECT key FROM dic WHERE key = ?"
#define SQL_INSERT "INSERT INTO dic VALUES(?,?,0)"

#ifndef container_of
#    define container_of(ptr, type, member) \
        ((type*)((char*)(ptr)-offsetof(type, member)))
#endif

static sqlite3_stmt* s_insert;
static sqlite3_stmt* s_query;
static sqlite3* db;

typedef struct word
{
    char* buf;
    list_head_t list;
} word_t;
static const char HEX_ARRAY[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    'A', 'B', 'C', 'D', 'E', 'F' };
static inline int indexof(const char* s1, const char* s2)
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

sqlite3* database()
{
    sqlite3* db = { 0 };

    int rc = sqlite3_open_v2(DBNAME, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (rc != SQLITE_OK)
    {
        printf("sqlite3_open_v2() failed.");
        return NULL;
    }

    return db;
}

int table(sqlite3* db)
{
    char* error;

    int rc = sqlite3_exec(db, SQL_CREATE_TABLE, 0, 0, &error);
    if (rc != SQLITE_OK)
    {
        printf("sqlite3_exec() failed. %s\n", error);
        return rc;
    }
    rc = sqlite3_exec(db, SQL_CREATE_INDEX, 0, 0, &error);
    if (rc != SQLITE_OK)
    {
        printf("sqlite3_exec() failed. %s\n", error);
        return rc;
    }
    return rc;
}
char* url(const char* word, char* buf_path, size_t buf_path_len)
{
    const char* from = "EN";
    const char* to = "zh-CHS";

    memset(buf_path, 0, buf_path_len);

    int salt = time(NULL);
    snprintf(buf_path, buf_path_len, "%s%s%d%s", API_KEY, word, salt, API_SECRET);

    char md5_buf[33];
    MD5_CTX md5_ctx;
    MD5Init(&md5_ctx);
    MD5Update(&md5_ctx, buf_path, strlen(buf_path));
    MD5Final(&md5_ctx);
    for (int i = 0, j = 0; i < 16; i++)
    {
        uint8_t t = md5_ctx.digest[i];
        md5_buf[j++] = HEX_ARRAY[t / 16];
        md5_buf[j++] = HEX_ARRAY[t % 16];
    }
    md5_buf[32] = 0;
    memset(buf_path, 0, buf_path_len);
    snprintf(buf_path, buf_path_len, "/api?q=%s&salt=%d&sign=%s&from=%s&appKey=%s&to=%s",
        word, salt, md5_buf, from, "1f5687b5a6b94361", to);
    return buf_path;
}
rapidstring* header(rapidstring* s, const char* word)
{

    size_t buf_path_len = MAX_PATH << 1;
    char buf_path[buf_path_len];
    url(word, buf_path, buf_path_len);

    rs_cat(s, "GET ");
    rs_cat(s, buf_path);
    rs_cat(s, " HTTP/1.1\r\n");
    rs_cat(s, "Host: openapi.youdao.com\r\n");
    rs_cat(s, "\r\n");

    return s;
}
uintptr_t connect_socket(const char* host, uint16_t port)
{
    int ret;
    struct addrinfo hints, *addr_list, *cur;
    int fd = 0;

    char port_str[6];
    snprintf(port_str, 6, "%d", port);

    memset(&hints, 0x00, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    ret = getaddrinfo(host, port_str, &hints, &addr_list);
    if (ret)
    {
        return 0;
    }

    for (cur = addr_list; cur != NULL; cur = cur->ai_next)
    {
        fd = (int)socket(cur->ai_family, cur->ai_socktype, cur->ai_protocol);
        if (fd < 0)
        {
            ret = 0;
            continue;
        }

        if (connect(fd, cur->ai_addr, cur->ai_addrlen) == 0)
        {
            ret = fd;
            break;
        }

        CLOSESOCKET(fd);
        ret = 0;
    }

    freeaddrinfo(addr_list);

    return (uintptr_t)ret;
}

int write(uintptr_t fd, const unsigned char* buf, uint32_t len, uint32_t timeout_ms, size_t* written_len)
{
    int ret;
    uint32_t len_sent;
    uint64_t t_end, t_left;
    fd_set sets;

    t_end = _linux_get_time_ms() + timeout_ms;
    len_sent = 0;
    ret = 1; /* send one time if timeout_ms is value 0 */

    do
    {
        t_left = _linux_time_left(t_end, _linux_get_time_ms());

        if (0 != t_left)
        {
            struct timeval timeout;

            FD_ZERO(&sets);
            FD_SET(fd, &sets);

            timeout.tv_sec = t_left / 1000;
            timeout.tv_usec = (t_left % 1000) * 1000;

            ret = select(fd + 1, NULL, &sets, NULL, &timeout);
            if (ret > 0)
            {
                if (0 == FD_ISSET(fd, &sets))
                {
                    /* If timeout in next loop, it will not sent any data */
                    ret = 0;
                    continue;
                }
            }
            else if (0 == ret)
            {
                ret = ERR_TCP_WRITE_TIMEOUT;
                break;
            }
            else
            {
                if (EINTR == errno)
                {
                    continue;
                }

                ret = ERR_TCP_WRITE_FAIL;
                break;
            }
        }
        else
        {
            ret = ERR_TCP_WRITE_TIMEOUT;
        }

        if (ret > 0)
        {
            ret = send(fd, buf + len_sent, len - len_sent, 0);
            if (ret > 0)
            {
                len_sent += ret;
            }
            else if (0 == ret)
            {
            }
            else
            {
                if (EINTR == errno)
                {
                    continue;
                }

                ret = ERR_TCP_WRITE_FAIL;
                break;
            }
        }
    } while ((len_sent < len) && (_linux_time_left(t_end, _linux_get_time_ms()) > 0));

    *written_len = (size_t)len_sent;

    return len_sent > 0 ? RET_SUCCESS : ret;
}

int read(uintptr_t fd, unsigned char* buf, uint32_t len, uint32_t timeout_ms, size_t* read_len)
{
    int ret, err_code;
    uint32_t len_recv;
    uint64_t t_end, t_left;
    fd_set sets;
    struct timeval timeout;

    t_end = _linux_get_time_ms() + timeout_ms;
    len_recv = 0;
    err_code = 0;

    do
    {
        t_left = _linux_time_left(t_end, _linux_get_time_ms());
        if (0 == t_left)
        {
            err_code = ERR_TCP_READ_TIMEOUT;
            break;
        }

        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        timeout.tv_sec = t_left / 1000;
        timeout.tv_usec = (t_left % 1000) * 1000;

        ret = select(fd + 1, &sets, NULL, NULL, &timeout);
        if (ret > 0)
        {
            ret = recv(fd, buf + len_recv, len - len_recv, 0);
            if (ret > 0)
            {
                len_recv += ret;
            }
            else if (0 == ret)
            {
                struct sockaddr_in peer;
                socklen_t sLen = sizeof(peer);
                int peer_port = 0;
                getpeername(fd, (struct sockaddr*)&peer, &sLen);
                peer_port = ntohs(peer.sin_port);

                err_code = ERR_TCP_PEER_SHUTDOWN;
                break;
            }
            else
            {
                if (EINTR == errno)
                {
                    continue;
                }
                err_code = ERR_TCP_READ_FAIL;
                break;
            }
        }
        else if (0 == ret)
        {
            err_code = ERR_TCP_READ_TIMEOUT;
            break;
        }
        else
        {
            err_code = ERR_TCP_READ_FAIL;
            break;
        }
    } while ((len_recv < len));

    *read_len = (size_t)len_recv;

    if (err_code == ERR_TCP_READ_TIMEOUT && len_recv == 0)
        err_code = ERR_TCP_NOTHING_TO_READ;

    return (len == len_recv) ? RET_SUCCESS : err_code;
}

int read_fully(uintptr_t fd, rapidstring* s, uint32_t timeout_ms)
{
    int ret, err_code;
    uint32_t len_recv;
    uint64_t t_end, t_left;
    fd_set sets;
    struct timeval timeout;

    size_t buf_size = 1024;
    char buf[buf_size + 1];
    memset(buf, 0, buf_size + 1);

    t_end = _linux_get_time_ms() + timeout_ms;
    err_code = 0;

    do
    {
        t_left = _linux_time_left(t_end, _linux_get_time_ms());
        if (0 == t_left)
        {
            err_code = ERR_TCP_READ_TIMEOUT;
            printf("timeout \n");
            break;
        }

        FD_ZERO(&sets);
        FD_SET(fd, &sets);

        timeout.tv_sec = t_left / 1000;
        timeout.tv_usec = (t_left % 1000) * 1000;

        ret = select(fd + 1, &sets, NULL, NULL, &timeout);
        if (ret > 0)
        {
            ret = recv(fd, buf, buf_size, 0);

            if (ret > 0)
            {

                rs_cat(s, buf);

                if (indexof(buf, "0\r\n\r\n") != -1)
                {
                    return RET_SUCCESS;
                }
                memset(buf, 0, buf_size);
            }
            else if (0 == ret)
            {
                err_code = ERR_TCP_PEER_SHUTDOWN;
                printf("tcp_peer_shutdown \n");

                break;
            }
            else
            {
                if (EINTR == errno)
                {
                    continue;
                }
                err_code = ERR_TCP_READ_FAIL;
                printf("ERR_TCP_READ_FAIL \n");

                break;
            }
        }
        else if (0 == ret)
        {
            err_code = ERR_TCP_READ_TIMEOUT;
            printf("ERR_TCP_READ_TIMEOUT \n");

            break;
        }
        else
        {
            err_code = ERR_TCP_READ_FAIL;
            printf("ERR_TCP_READ_FAIL \n");

            break;
        }
    } while (1);

    if (err_code == ERR_TCP_READ_TIMEOUT && len_recv == 0)
        err_code = ERR_TCP_NOTHING_TO_READ;

    return err_code;
}

int query_sql(sqlite3* db, const char* key, sqlite3_stmt* s)
{
    //sqlite3_clear_bindings(s);
    int rc = sqlite3_bind_text(s, 1, key, strlen(key), NULL);
    if (rc)
    {
        fprintf(stderr, "error: Bind %s to %d failed, %s\n", key, rc, sqlite3_errmsg(db));
        return rc;
    }
    rc = sqlite3_step(s);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW)
    {

        fprintf(stderr, "insert statement didn't return DONE (%i): %s\n", rc, sqlite3_errmsg(db));
        return rc;
    }
    const char* r = sqlite3_column_text(s, 0);
    rc = r != NULL;
    //sqlite3_finalize(
    sqlite3_reset(s);
    return rc;
}
int insert_sql(sqlite3* db, const char* key, const char* word, sqlite3_stmt* s)
{
    sqlite3_clear_bindings(s);

    int rc = sqlite3_bind_text(s, 1, key, strlen(key), NULL);
    if (rc)
    {
        fprintf(stderr, "error: Bind %s to %d failed, %s\n", key, rc, sqlite3_errmsg(db));
        return rc;
    }
    rc = sqlite3_bind_text(s, 2, word, strlen(word), NULL);
    if (rc)
    {
        fprintf(stderr, "error: Bind %s to %d failed, %s\n", key, rc, sqlite3_errmsg(db));
        return rc;
    }
    rc = sqlite3_step(s);
    if (SQLITE_DONE != rc)
    {
        fprintf(stderr, "insert statement didn't return DONE (%i): %s\n", rc, sqlite3_errmsg(db));
        return rc;
    }
    sqlite3_reset(s);

    return rc;
}
int query(const char* word)
{

    uintptr_t fd = connect_socket(DEFAULT_HOST, DEFAULT_PORT);

    if (fd == 0)
    {
        printf("connect_socket() \n");
        return 0;
    }

    rapidstring s;
    rs_init(&s);
    header(&s, word);

    size_t written_len = 0;

    int rc = write(fd, rs_data(&s), rs_len(&s), 10000, &written_len);

    if (rc != RET_SUCCESS)
    {
        CLOSESOCKET(fd);
        return 0;
    }

    size_t read_len = 0;

    rs_clear(&s);
    rc = read_fully(fd, &s, 10000);

    if (rc != RET_SUCCESS)
    {
        rs_free(&s);
        CLOSESOCKET(fd);
        return 0;
    }

    char* buf = rs_data(&s);

    buf = strstr(buf, "\r\n\r\n");
    if (buf != NULL || strlen(buf) < 4)
    {
        buf = buf + 4;
        buf = strstr(buf, "\r\n");
    }
    else
    {
        rs_free(&s);
        CLOSESOCKET(fd);
        return 0;
    }
    if (buf != NULL || strlen(buf) < 2)
    {
        buf = buf + 2;
    }
    else
    {
        rs_free(&s);
        CLOSESOCKET(fd);
        return 0;
    }
    size_t buf_body_len = 1024 << 2, buf_body_read_len = 0;
    char buf_body[buf_body_len];
    memset(buf_body, 0, buf_body_len);

    //printf("%s\n", buf);

    cJSON* json = cJSON_Parse(buf);
    if (json == NULL)
    {
        const char* error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            printf("%s\n", error_ptr);
            goto error;
        }
    }
    const cJSON* basic = cJSON_GetObjectItem(json, "basic");
    const cJSON* explains = cJSON_GetObjectItem(basic, "explains");
    const cJSON* explain = NULL;
    cJSON_ArrayForEach(explain, explains)
    {
        strcat(buf_body, explain->valuestring);
        strcat(buf_body, "\n");
    };
    const cJSON* web = cJSON_GetObjectItem(json, "web");
    const cJSON* w = NULL;
    cJSON_ArrayForEach(w, web)
    {
        strcat(buf_body, cJSON_GetObjectItem(w, "key")->valuestring);
        const cJSON* values = cJSON_GetObjectItem(w, "value");
        const cJSON* value = NULL;
        strcat(buf_body, " ");
        cJSON_ArrayForEach(value, values)
        {
            strcat(buf_body, value->valuestring);
            strcat(buf_body, ",");
        }
        buf_body[strlen(buf_body) - 1] = '\n';
    }

    if (strlen(buf_body) > 0)
        insert_sql(db, word, buf_body, s_insert);
    else
    {
        printf("[ERROR]: %s %s\n", word,rs_data(&s));
    }

error:
    cJSON_Delete(json);
    rs_free(&s);
    CLOSESOCKET(fd);
    return 0;
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

    db = database();

    table(db);

    if (sqlite3_prepare_v2(db, SQL_QUERY, -1, &s_query, NULL))
    {
        fprintf(stderr, "error: Prepare stmt stmt_insert_ failed, %s\n", sqlite3_errmsg(db));
        return EXIT_FAILURE;
    }
    if (sqlite3_prepare_v2(db, SQL_INSERT, -1, &s_insert, NULL))
    {
        fprintf(stderr, "error: Prepare stmt stmt_insert_ failed, %s\n", sqlite3_errmsg(db));
        return EXIT_FAILURE;
    }

    // char address_buf[128];
    // char service_buf[128];

    // getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
    //     address_buf, sizeof(address_buf),
    //     service_buf, sizeof(service_buf),
    //     NI_NUMERICHOST);

    // printf("%s %s\n", address_buf, service_buf);

    list_head_t* word_list = collect();
    word_t *pos, *tmp;
    list_for_each_entry_safe(pos, tmp, word_list, list, word_t)
    {

        int rc = query_sql(db, pos->buf, s_query);
        if (!rc)
        {

            printf("Processing: %s\n", pos->buf);
            query(pos->buf);
        }
        else
        {
            //printf("Processed: %s\n", pos->buf);
        }
        list_del(&pos->list);
        free(pos->buf);
        free(pos);
    }
    //query();
    //query("word");
#if defined(_WIN32)
    WSACleanup();
#endif
    return EXIT_SUCCESS;
}