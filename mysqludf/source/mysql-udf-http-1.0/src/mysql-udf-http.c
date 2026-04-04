#include <mysql.h>
#include <string.h>
#include <ctype.h>

#include <stdio.h>
#include <stdlib.h> // avoid realloc and free warning
#include <curl/curl.h>
#include "mysql-udf-http.h"

const static long REQ_TIMEOUT_MS = 30000; // millisecond. (30s)
static int curl_initialized = 0;

my_bool http_get_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *http_get(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void http_get_deinit(UDF_INIT *initid);

my_bool http_post_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *http_post(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void http_post_deinit(UDF_INIT *initid);

my_bool http_put_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *http_put(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void http_put_deinit(UDF_INIT *initid);

my_bool http_delete_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *http_delete(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void http_delete_deinit(UDF_INIT *initid);

static void *myrealloc(void *ptr, size_t size)
{
    /* There might be a realloc() out there that doesn't like reallocing
       NULL pointers, so we take care of it here */
    if (ptr)
        return realloc(ptr, size);
    else
        return malloc(size);
}

static size_t
result_cb(void *ptr, size_t size, size_t nmemb, void *data)
{
    size_t realsize = size * nmemb;
    struct st_curl_results *res = (struct st_curl_results *)data;

    res->result = (char *)myrealloc(res->result, res->size + realsize + 1);
    if (res->result)
    {
        memcpy(&(res->result[res->size]), ptr, realsize);
        res->size += realsize;
        res->result[res->size] = 0;
    }
    return realsize;
}

static void cleanup_allocated_strings(char **headers_str, char **timeout_str)
{
    if (*headers_str) {
        free(*headers_str);
        *headers_str = NULL;
    }
    if (*timeout_str) {
        free(*timeout_str);
        *timeout_str = NULL;
    }
}

// Helper function to count items in comma-separated string
int count_comma_items(const char *input)
{
    if (!input || !*input) return 0;

    int count = 1; // At least one item
    const char *p = input;

    while (*p) {
        if (*p == ',') {
            count++;
            // Skip consecutive commas
            while (*p == ',') p++;
        } else {
            p++;
        }
    }

    return count > MAX_FORM_FIELDS ? MAX_FORM_FIELDS : count;
}

// Helper function to parse comma-separated string into array
int parse_comma_separated(const char *input, char *output[], int max_items)
{
    if (!input || !*input || max_items <= 0) return 0;

    int item_count = 0;
    const char *start = input;
    const char *end;

    while (item_count < max_items && *start) {
        end = start;

        // Find the end of current item
        while (*end && *end != ',') end++;

        // Calculate length and copy (trim whitespace)
        size_t len = end - start;
        if (len > 0) {
            char *item = output[item_count];
            size_t copy_len = len > 255 ? 255 : len; // Max 255 chars per item

            // Trim leading whitespace
            while (copy_len > 0 && isspace(start[copy_len - 1])) copy_len--;

            strncpy(item, start, copy_len);
            item[copy_len] = '\0';

            // Trim trailing whitespace
            char *trim_end = item + strlen(item) - 1;
            while (trim_end >= item && isspace(*trim_end)) {
                *trim_end-- = '\0';
            }
        } else {
            output[item_count][0] = '\0'; // Empty item
        }

        item_count++;

        // Move to next item
        if (*end == ',') {
            start = end + 1;
        } else {
            break;
        }
    }

    return item_count;
}
static CURL *my_container_curl_init(st_curl_results *res)
{
    CURL *curl = NULL;

    if (!res)
        return NULL;

    if (!curl_initialized)
    {
        curl_global_init(CURL_GLOBAL_ALL);
        curl_initialized = 1;
    }

    curl = curl_easy_init();
    if (curl)
    {
        // some global default option
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, REQ_TIMEOUT_MS);
    }

    res->curl = curl;
    return curl;
}
/* ------------------------HTTP GET----------------------------- */

my_bool http_get_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    if (args->arg_count != 1)
    {
        strncpy(message,
                "one argument must be supplied: http_get('<url>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    args->arg_type[0] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (st_curl_results *)malloc(sizeof(st_curl_results));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    if (!my_container_curl_init(container))
    {
        free(container);
        strncpy(message, "curl initialization failed", MYSQL_ERRMSG_SIZE);
        return 1;
    }
    initid->ptr = (char *)container;

    return 0;
}

char *http_get(UDF_INIT *initid, UDF_ARGS *args,
               __attribute__((unused)) char *result,
               unsigned long *length,
               __attribute__((unused)) char *is_null,
               __attribute__((unused)) char *error)
{
    CURLcode retref;
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (!res || !initid->ptr)
    {
        *length = 0;
        return NULL;
    }

    curl = res->curl;

    res->result = NULL;
    res->size = 0;

    if (curl && args && args->args[0])
    {
        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, REQ_TIMEOUT_MS);
        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }
    }
    else
    {
        *length = 0;
    }
    *length = res->size;
    return res->result ? (char *)res->result : NULL;
}

void http_get_deinit(UDF_INIT *initid)
{
    /* if we allocated initid->ptr, free it here */
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (res)
    {
        free(res->result);
        curl_easy_cleanup(res->curl);
        free(res);
    }
    return;
}

/* ------------------------HTTP POST----------------------------- */

my_bool http_post_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    if (args->arg_count != 2)
    {
        strncpy(message,
                "two arguments must be supplied: http_post('<url>','<data>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    args->arg_type[0] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (st_curl_results *)malloc(sizeof(st_curl_results));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    if (!my_container_curl_init(container))
    {
        free(container);
        strncpy(message, "curl initialization failed", MYSQL_ERRMSG_SIZE);
        return 1;
    }
    initid->ptr = (char *)container;

    return 0;
}

char *http_post(UDF_INIT *initid, UDF_ARGS *args,
                __attribute__((unused)) char *result,
                unsigned long *length,
                __attribute__((unused)) char *is_null,
                __attribute__((unused)) char *error)
{
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURLcode retref;
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (!res || !initid->ptr || !args || args->arg_count < 2)
    {
        *length = 0;
        return NULL;
    }

    curl = res->curl;

    res->result = NULL;
    res->size = 0;

    if (curl && args->args[0] && args->args[1])
    {
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Expect:");

        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, REQ_TIMEOUT_MS);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, args->args[1]);
        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }
        curl_slist_free_all(chunk);
    }
    else
    {
        *length = 0;
    }
    *length = res->size;
    return res->result ? (char *)res->result : NULL;
}

void http_post_deinit(UDF_INIT *initid)
{
    /* if we allocated initid->ptr, free it here */
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (res)
    {
        free(res->result);
        curl_easy_cleanup(res->curl);
        free(res);
    }
    return;
}

/* ------------------------HTTP PUT----------------------------- */

my_bool http_put_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    if (args->arg_count != 2)
    {
        strncpy(message,
                "two arguments must be supplied: http_put('<url>','<data>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    args->arg_type[0] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (st_curl_results *)malloc(sizeof(st_curl_results));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    if (!my_container_curl_init(container))
    {
        free(container);
        strncpy(message, "curl initialization failed", MYSQL_ERRMSG_SIZE);
        return 1;
    }
    initid->ptr = (char *)container;

    return 0;
}

char *http_put(UDF_INIT *initid, UDF_ARGS *args,
               __attribute__((unused)) char *result,
               unsigned long *length,
               __attribute__((unused)) char *is_null,
               __attribute__((unused)) char *error)
{
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURLcode retref;
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (!res || !initid->ptr || !args || args->arg_count < 2)
    {
        *length = 0;
        return NULL;
    }

    curl = res->curl;

    res->result = NULL;
    res->size = 0;

    if (curl && args->args[0] && args->args[1])
    {
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, "Expect:");

        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, REQ_TIMEOUT_MS);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, args->args[1]);
        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }
        curl_slist_free_all(chunk);
    }
    else
    {
        *length = 0;
    }
    *length = res->size;
    return res->result ? (char *)res->result : NULL;
}

void http_put_deinit(UDF_INIT *initid)
{
    /* if we allocated initid->ptr, free it here */
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (res)
    {
        free(res->result);
        curl_easy_cleanup(res->curl);
        free(res);
    }
    return;
}

/* ------------------------HTTP DELETE----------------------------- */

my_bool http_delete_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    if (args->arg_count != 1)
    {
        strncpy(message,
                "one arguments must be supplied: http_delete('<url>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    args->arg_type[0] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (st_curl_results *)malloc(sizeof(st_curl_results));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    if (!my_container_curl_init(container))
    {
        free(container);
        strncpy(message, "curl initialization failed", MYSQL_ERRMSG_SIZE);
        return 1;
    }
    initid->ptr = (char *)container;

    return 0;
}

char *http_delete(UDF_INIT *initid, UDF_ARGS *args,
                  __attribute__((unused)) char *result,
                  unsigned long *length,
                  __attribute__((unused)) char *is_null,
                  __attribute__((unused)) char *error)
{
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURLcode retref;
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (!res || !initid->ptr || !args || args->arg_count < 1)
    {
        *length = 0;
        return NULL;
    }

    curl = res->curl;

    res->result = NULL;
    res->size = 0;

    if (curl && args->args[0])
    {
        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, REQ_TIMEOUT_MS);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }
    }
    else
    {
        *length = 0;
    }
    *length = res->size;
    return res->result ? (char *)res->result : NULL;
}

void http_delete_deinit(UDF_INIT *initid)
{
    /* if we allocated initid->ptr, free it here */
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (res)
    {
        free(res->result);
        curl_easy_cleanup(res->curl);
        free(res);
    }
    return;
}

/* ------------------------ HTTP GET with Headers ------------------------------ */

my_bool http_get_headers_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    if (args->arg_count != 3)
    {
        strncpy(message,
                "three arguments must be supplied: http_get_headers('<url>','<headers>','<timeout_ms>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    // 所有参数均视为字符串类型
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    args->arg_type[2] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (st_curl_results *)malloc(sizeof(st_curl_results));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    if (!my_container_curl_init(container))
    {
        free(container);
        strncpy(message, "curl initialization failed", MYSQL_ERRMSG_SIZE);
        return 1;
    }
    initid->ptr = (char *)container;

    return 0;
}

char *http_get_headers(UDF_INIT *initid, UDF_ARGS *args,
                       __attribute__((unused)) char *result,
                       unsigned long *length,
                       __attribute__((unused)) char *is_null,
                       __attribute__((unused)) char *error)
{
    CURLcode retref;
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;
    char *headers_str = NULL;
    char *timeout_str = NULL;

    if (!res || !initid->ptr || !args || args->arg_count < 3)
    {
        *length = 0;
        cleanup_allocated_strings(&headers_str, &timeout_str);
        return NULL;
    }

    curl = res->curl;
    res->result = NULL;
    res->size = 0;

    if (curl)
    {
        struct curl_slist *chunk = NULL;

        chunk = curl_slist_append(chunk, "Expect:");

        // 解析请求头字符串
        if (args->args[1] && args->lengths[1] > 0)
        {
            headers_str = strndup(args->args[1], args->lengths[1]);
            if (headers_str)
            {
                char *line = strtok(headers_str, "\n");
                while (line != NULL)
                {
                    // 去除行首尾空白
                    while (*line == ' ' || *line == '\t')
                        line++;
                    char *end = line + strlen(line) - 1;
                    while (end > line && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
                        *end-- = '\0';

                    if (strlen(line) > 0)
                        chunk = curl_slist_append(chunk, line);
                    line = strtok(NULL, "\n");
                }
                // 不需要立即释放，strtok 使用同一块内存
            }
        }

        // 解析超时时间（毫秒）
        long timeout_ms = REQ_TIMEOUT_MS;
        if (args->args[2] && args->lengths[2] > 0)
        {
            timeout_str = strndup(args->args[2], args->lengths[2]);
            if (timeout_str)
            {
                char *endptr;
                long val = strtol(timeout_str, &endptr, 10);
                if (*timeout_str != '\0' && *endptr == '\0' && val > 0)
                {
                    timeout_ms = val;
                }
            }
        }

        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }

        // 释放自定义请求头链表和分配的字符串
        if (chunk)
            curl_slist_free_all(chunk);

        // 释放分配的字符串内存
        cleanup_allocated_strings(&headers_str, &timeout_str);
    }
    else
    {
        *length = 0;
        cleanup_allocated_strings(&headers_str, &timeout_str);
    }

    *length = res->size;
    return ((char *)res->result);
}

void http_get_headers_deinit(UDF_INIT *initid)
{
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (res)
    {
        free(res->result);
        curl_easy_cleanup(res->curl);
        free(res);
    }
    return;
}

/* ------------------------ HTTP POST with Headers ------------------------------ */

my_bool http_post_headers_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    if (args->arg_count != 4)
    {
        strncpy(message,
                "four arguments must be supplied: http_post_headers('<url>','<data>','<headers>','<timeout_ms>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    args->arg_type[2] = STRING_RESULT;
    args->arg_type[3] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (st_curl_results *)malloc(sizeof(st_curl_results));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    if (!my_container_curl_init(container))
    {
        free(container);
        strncpy(message, "curl initialization failed", MYSQL_ERRMSG_SIZE);
        return 1;
    }
    initid->ptr = (char *)container;

    return 0;
}

char *http_post_headers(UDF_INIT *initid, UDF_ARGS *args,
                        __attribute__((unused)) char *result,
                        unsigned long *length,
                        __attribute__((unused)) char *is_null,
                        __attribute__((unused)) char *error)
{
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURLcode retref;
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;
    char *headers_str = NULL;
    char *timeout_str = NULL;

    if (!res || !initid->ptr || !args || args->arg_count < 4)
    {
        *length = 0;
        cleanup_allocated_strings(&headers_str, &timeout_str);
        return NULL;
    }

    curl = res->curl;
    res->result = NULL;
    res->size = 0;

    if (curl)
    {
        struct curl_slist *chunk = NULL;

        chunk = curl_slist_append(chunk, "Expect:");

        // 解析请求头字符串
        if (args->args[2] && args->lengths[2] > 0)
        {
            headers_str = strndup(args->args[2], args->lengths[2]);
            if (headers_str)
            {
                char *line = strtok(headers_str, "\n");
                while (line != NULL)
                {
                    // 去除行首尾空白
                    while (*line == ' ' || *line == '\t')
                        line++;
                    char *end = line + strlen(line) - 1;
                    while (end > line && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
                        *end-- = '\0';

                    if (strlen(line) > 0)
                        chunk = curl_slist_append(chunk, line);
                    line = strtok(NULL, "\n");
                }
                // 不需要立即释放，strtok 使用同一块内存
            }
        }

        // 解析超时时间（毫秒）
        long timeout_ms = REQ_TIMEOUT_MS;
        if (args->args[3] && args->lengths[3] > 0)
        {
            timeout_str = strndup(args->args[3], args->lengths[3]);
            if (timeout_str)
            {
                char *endptr;
                long val = strtol(timeout_str, &endptr, 10);
                if (*timeout_str != '\0' && *endptr == '\0' && val > 0)
                {
                    timeout_ms = val;
                }
            }
        }

        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, args->args[1]);

        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }

        // 释放自定义请求头链表和分配的字符串
        if (chunk)
            curl_slist_free_all(chunk);

        // 释放分配的字符串内存
        cleanup_allocated_strings(&headers_str, &timeout_str);
    }
    else
    {
        *length = 0;
        cleanup_allocated_strings(&headers_str, &timeout_str);
    }

    *length = res->size;
    return ((char *)res->result);
}

void http_post_headers_deinit(UDF_INIT *initid)
{
    st_curl_results *res = (st_curl_results *)initid->ptr;

    if (res)
    {
        free(res->result);
        curl_easy_cleanup(res->curl);
        free(res);
    }
    return;
}

/* Module unload function to cleanup global resources */
void mysql_udf_http_deinit(void)
{
    if (curl_initialized)
    {
        curl_global_cleanup();
        curl_initialized = 0;
    }
}

/* ------------------------ HTTP Multipart Form Data POST with Multiple Files ------------------------------ */

my_bool http_post_multipart_multi_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    http_multipart_data *container;
    int file_count;

    if (args->arg_count < 7 || (args->arg_count - 4) % 3 != 0)
    {
        strncpy(message,
                "invalid argument count for multi-file upload. Use: http_post_multipart_multi('<url>','<headers>','<timeout_ms>','<fieldtxt>', '<uploaded_file1>','<filename>1','<file_data1>', '<uploaded_file2>','<filename>2','<file_data2>', ...)",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    // Calculate number of file groups (every 3 args after the first 4)
    int base_args = 4; // url, headers, timeout_ms, fieldtxt
    file_count = (args->arg_count - base_args) / 3;

    // Limit to max form fields
    if (file_count > MAX_FORM_FIELDS)
    {
        file_count = MAX_FORM_FIELDS;
        strncpy(message, "too many files (max 10)", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    // All arguments are treated as string type
    for (int i = 0; i < args->arg_count; i++) {
        args->arg_type[i] = STRING_RESULT;
    }

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (http_multipart_data *)malloc(sizeof(http_multipart_data));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    memset(container, 0, sizeof(http_multipart_data));

    // Store URL
    strncpy(container->url, args->args[0], sizeof(container->url) - 1);
    container->url[sizeof(container->url) - 1] = '\0';

    // Parse headers
    if (args->args[1])
    {
        container->headers_str = strndup(args->args[1], args->lengths[1]);
    }

    // Parse timeout (default to REQ_TIMEOUT_MS)
    long timeout_ms = REQ_TIMEOUT_MS;
    if (args->args[2] && args->lengths[2] > 0)
    {
        char *endptr;
        long val = strtol(args->args[2], &endptr, 10);
        if (*args->args[2] != '\0' && *endptr == '\0' && val > 0)
        {
            timeout_ms = val;
        }
    }
    container->timeout_ms = timeout_ms;

    // Parse text fields (format: name=value;name2=value2)
    if (args->args[3] && strlen(args->args[3]) > 0)
    {
        char *fieldtxt = strdup(args->args[3]);
        char *saveptr = NULL;
        char *pair = strtok_r(fieldtxt, ";", &saveptr);

        while (pair && container->post_field_count < MAX_FORM_FIELDS)
        {
            char *eq_pos = strchr(pair, '=');
            if (eq_pos)
            {
                *eq_pos = '\0';
                container->post_fields[container->post_field_count] = strdup(eq_pos + 1);
                container->post_field_count++;
            }
            pair = strtok_r(NULL, ";", &saveptr);
        }
        free(fieldtxt);
    }

    // Initialize multi-file arrays
    container->field_names = malloc(MAX_FORM_FIELDS * sizeof(char *));
    container->filenames = malloc(MAX_FORM_FIELDS * sizeof(char *));
    container->file_datas = malloc(MAX_FORM_FIELDS * sizeof(unsigned char *));
    container->data_lengths = malloc(MAX_FORM_FIELDS * sizeof(unsigned long));
    container->max_files = MAX_FORM_FIELDS;

    for (int i = 0; i < MAX_FORM_FIELDS; i++) {
        container->field_names[i] = NULL;
        container->filenames[i] = NULL;
        container->file_datas[i] = NULL;
        container->data_lengths[i] = 0;
    }

    // Process each file group (uploaded_file, filename, file_data)
    for (int file_idx = 0; file_idx < file_count; file_idx++)
    {
        int arg_offset = 4 + (file_idx * 3); // Base offset + file_idx * 3

        // Extract field name
        const char *field_name = args->arg_count > arg_offset ? args->args[arg_offset] : "";

        // Extract filename
        const char *filename = args->arg_count > arg_offset + 1 ? args->args[arg_offset + 1] : "";

        // Extract file data
        const char *file_data = args->arg_count > arg_offset + 2 ? args->args[arg_offset + 2] : "";

        // Set field name
        if (strlen(field_name) == 0)
            strcpy(container->files[file_idx].name, "file");
        else
            strncpy(container->files[file_idx].name, field_name, MAX_FILENAME_LENGTH - 1);
        container->files[file_idx].name[MAX_FILENAME_LENGTH - 1] = '\0';

        // Set filename
        strncpy(container->files[file_idx].filename, filename, MAX_FILENAME_LENGTH - 1);
        container->files[file_idx].filename[MAX_FILENAME_LENGTH - 1] = '\0';

        // Store field name pointer
        if (field_name && strlen(field_name) > 0) {
            container->field_names[file_idx] = strdup(field_name);
        }

        // Store filename pointer
        if (filename && strlen(filename) > 0) {
            container->filenames[file_idx] = strdup(filename);
        }

        // Store file data
        if (file_data && args->lengths[arg_offset + 2] > 0) {
            container->file_datas[file_idx] = (unsigned char *)malloc(args->lengths[arg_offset + 2]);
            if (container->file_datas[file_idx]) {
                memcpy(container->file_datas[file_idx], file_data, args->lengths[arg_offset + 2]);
                container->data_lengths[file_idx] = args->lengths[arg_offset + 2];
            }
        }

        // Also copy to the files array for compatibility
        if (container->file_datas[file_idx]) {
            container->files[file_idx].data = container->file_datas[file_idx];
            container->files[file_idx].length = container->data_lengths[file_idx];
        }
    }

    container->file_count = file_count;

    return 0;

    initid->ptr = (char *)container;
    return 0;
}

char *http_post_multipart_multi(UDF_INIT *initid, UDF_ARGS *args,
                                __attribute__((unused)) char *result,
                                unsigned long *length,
                                __attribute__((unused)) char *is_null,
                                __attribute__((unused)) char *error)
{
    CURLcode retref;
    char err_msg[CURL_ERROR_SIZE] = {0};
    CURL *curl;
    st_curl_results *res = (st_curl_results *)initid->ptr;
    http_multipart_data *data = (http_multipart_data *)initid->ptr;

    if (!res || !initid->ptr || !data || !args || args->arg_count < 7)
    {
        *length = 0;
        return NULL;
    }

    curl = res->curl;
    res->result = NULL;
    res->size = 0;

    if (curl)
    {
        struct curl_slist *chunk = NULL;
        CURLFORMcode formcode;

        // Set basic options
        chunk = curl_slist_append(chunk, "Expect:");

        // Add custom headers
        if (data->headers_str)
        {
            char *line = strtok(data->headers_str, "\n");
            while (line != NULL)
            {
                // Trim whitespace
                while (*line == ' ' || *line == '\t')
                    line++;
                char *end = line + strlen(line) - 1;
                while (end > line && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n'))
                    *end-- = '\0';

                if (strlen(line) > 0)
                    chunk = curl_slist_append(chunk, line);
                line = strtok(NULL, "\n");
            }
        }

        // Set request options
        curl_easy_setopt(curl, CURLOPT_URL, data->url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, data->timeout_ms);

        // Build multipart form
        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;

        // Add text fields first
        int i;
        for (i = 0; i < data->post_field_count && i < MAX_FORM_FIELDS; i++)
        {
            if (data->post_fields[i])
            {
                formcode = curl_formadd(&formpost,
                                        &lastptr,
                                        CURLFORM_COPYNAME, data->post_fields[i],
                                        CURLFORM_PTRCONTENTS, data->post_fields[i],
                                        CURLFORM_CONTENTSLENGTH, strlen(data->post_fields[i]),
                                        CURLFORM_END);

                if (formcode != CURL_FORMADD_OK)
                {
                    fprintf(stderr, "<%s:%d> ERROR. curl_formadd failed for text field %d\n", __FUNCTION__, __LINE__, i);
                    *length = 0;
                    return NULL;
                }
            }
        }

        // Add all file fields
        for (i = 0; i < data->file_count && i < MAX_FORM_FIELDS; i++)
        {
            if (data->file_datas[i] && data->data_lengths[i] > 0)
            {
                // Use in-memory binary data
                formcode = curl_formadd(&formpost,
                                        &lastptr,
                                        CURLFORM_COPYNAME, data->files[i].name,
                                        CURLFORM_BUFFER, data->filenames[i] ? data->filenames[i] : "uploaded_file",
                                        CURLFORM_BUFFERPTR, data->file_datas[i],
                                        CURLFORM_BUFFERLENGTH, data->data_lengths[i],
                                        CURLFORM_CONTENTTYPE, "application/octet-stream",
                                        CURLFORM_END);
            }
            else if (strlen(data->files[i].filename) > 0)
            {
                // Use file from disk
                FILE *file = fopen(data->files[i].filename, "rb");
                if (file)
                {
                    fseek(file, 0, SEEK_END);
                    long file_size = ftell(file);
                    fseek(file, 0, SEEK_SET);

                    formcode = curl_formadd(&formpost,
                                            &lastptr,
                                            CURLFORM_COPYNAME, data->files[i].name,
                                            CURLFORM_FILE, data->files[i].filename,
                                            CURLFORM_CONTENTTYPE, "application/octet-stream",
                                            CURLFORM_END);

                    fclose(file);
                }
                else
                {
                    // Fallback to buffer with placeholder data
                    formcode = curl_formadd(&formpost,
                                            &lastptr,
                                            CURLFORM_COPYNAME, data->files[i].name,
                                            CURLFORM_BUFFER, data->files[i].filename[0] ? data->files[i].filename : "uploaded_file",
                                            CURLFORM_BUFFERPTR, "",
                                            CURLFORM_BUFFERLENGTH, 0,
                                            CURLFORM_CONTENTTYPE, "application/octet-stream",
                                            CURLFORM_END);
                }
            }

            if (formcode != CURL_FORMADD_OK)
            {
                fprintf(stderr, "<%s:%d> ERROR. curl_formadd failed for file %d\n", __FUNCTION__, __LINE__, i);
                *length = 0;
                return NULL;
            }
        }

        curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }

        // Clean up resources
        curl_formfree(formpost);
        if (chunk)
            curl_slist_free_all(chunk);
    }
    else
    {
        *length = 0;
    }

    *length = res->size;
    return ((char *)res->result);
}

void http_post_multipart_multi_deinit(UDF_INIT *initid)
{
    http_multipart_data *data = (http_multipart_data *)initid->ptr;

    if (data)
    {
        // Free multi-file arrays
        if (data->field_names) {
            for (int i = 0; i < data->max_files; i++) {
                if (data->field_names[i]) {
                    free(data->field_names[i]);
                    data->field_names[i] = NULL;
                }
            }
            free(data->field_names);
            data->field_names = NULL;
        }

        if (data->filenames) {
            for (int i = 0; i < data->max_files; i++) {
                if (data->filenames[i]) {
                    free(data->filenames[i]);
                    data->filenames[i] = NULL;
                }
            }
            free(data->filenames);
            data->filenames = NULL;
        }

        if (data->file_datas) {
            for (int i = 0; i < data->max_files; i++) {
                if (data->file_datas[i]) {
                    free(data->file_datas[i]);
                    data->file_datas[i] = NULL;
                }
            }
            free(data->file_datas);
            data->file_datas = NULL;
        }

        if (data->data_lengths) {
            free(data->data_lengths);
            data->data_lengths = NULL;
        }

        // Free file upload field data
        int i;
        for (i = 0; i < data->file_count; i++)
        {
            if (data->files[i].data && data->files[i].data != data->file_datas[i])
            {
                // Only free if it's not already freed by the above loop
                free(data->files[i].data);
                data->files[i].data = NULL;
            }
        }
        for (i = 0; i < data->post_field_count; i++)
        {
            if (data->post_fields[i])
            {
                free(data->post_fields[i]);
                data->post_fields[i] = NULL;
            }
        }

        if (data->headers_str)
        {
            free(data->headers_str);
            data->headers_str = NULL;
        }

        free(data);
    }
    return;
}