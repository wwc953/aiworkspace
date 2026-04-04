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

my_bool http_post_file_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *http_post_file(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void http_post_file_deinit(UDF_INIT *initid);

my_bool http_post_files_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
char *http_post_files(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length, char *is_null, char *error);
void http_post_files_deinit(UDF_INIT *initid);

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

/* ------------------------ HTTP POST File Upload with Headers and Timeout ------------------------------ */

my_bool http_post_file_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    http_multipart_data *container;

    if (args->arg_count != 6)
    {
        strncpy(message,
                "six arguments must be supplied: http_post_file('<url>','<field_name>','<filename>','<content_type>','<headers>','<timeout_ms>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    // 所有参数均视为字符串类型
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    args->arg_type[2] = STRING_RESULT;
    args->arg_type[3] = STRING_RESULT;
    args->arg_type[4] = STRING_RESULT;
    args->arg_type[5] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (http_multipart_data *)malloc(sizeof(http_multipart_data));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    memset(container, 0, sizeof(http_multipart_data));
    strncpy(container->url, args->args[0], sizeof(container->url) - 1);
    container->url[sizeof(container->url) - 1] = '\0';

    // 解析文件名和内容类型
    if (args->args[2])
    {
        strncpy(container->files[0].filename, args->args[2], MAX_FILENAME_LENGTH - 1);
        container->files[0].filename[MAX_FILENAME_LENGTH - 1] = '\0';
    }

    if (args->args[3])
    {
        strncpy(container->files[0].content_type, args->args[3], sizeof(container->files[0].content_type) - 1);
        container->files[0].content_type[sizeof(container->files[0].content_type) - 1] = '\0';
    }
    else
    {
        strcpy(container->files[0].content_type, "application/octet-stream");
    }

    // 解析请求头字符串
    if (args->args[4])
    {
        container->headers_str = strndup(args->args[4], args->lengths[4]);
    }

    // 解析超时时间（毫秒）
    long timeout_ms = REQ_TIMEOUT_MS;
    if (args->args[5] && args->lengths[5] > 0)
    {
        char *endptr;
        long val = strtol(args->args[5], &endptr, 10);
        if (*args->args[5] != '\0' && *endptr == '\0' && val > 0)
        {
            timeout_ms = val;
        }
    }
    container->timeout_ms = timeout_ms;

    initid->ptr = (char *)container;

    return 0;
}

char *http_post_file(UDF_INIT *initid, UDF_ARGS *args,
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
    FILE *file = NULL;
    char *field_name = args->args[1];
    char *filename = args->args[2];
    char *headers_str = NULL;

    if (!res || !initid->ptr || !data || !args || args->arg_count < 6)
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

        // 设置基本选项
        chunk = curl_slist_append(chunk, "Expect:");

        // 解析自定义请求头
        if (data->headers_str)
        {
            char *line = strtok(data->headers_str, "\n");
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
        }

        curl_easy_setopt(curl, CURLOPT_URL, data->url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, data->timeout_ms);

        // 构建multipart表单
        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;

        // 添加文件字段
        if (filename && strlen(filename) > 0)
        {
            file = fopen(filename, "rb");
            if (file)
            {
                fseek(file, 0, SEEK_END);
                long file_size = ftell(file);
                fseek(file, 0, SEEK_SET);

                formcode = curl_formadd(&formpost,
                                        &lastptr,
                                        CURLFORM_COPYNAME, field_name ? field_name : "file",
                                        CURLFORM_FILE, filename,
                                        CURLFORM_CONTENTTYPE, data->files[0].content_type,
                                        CURLFORM_END);

                if (formcode != CURL_FORMADD_OK)
                {
                    fprintf(stderr, "<%s:%d> ERROR. curl_formadd failed\n", __FUNCTION__, __LINE__);
                    fclose(file);
                    *length = 0;
                    return NULL;
                }
            }
            else
            {
                fprintf(stderr, "<%s:%d> ERROR. Cannot open file %s\n", __FUNCTION__, __LINE__, filename);
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

        // 清理资源
        if (file)
            fclose(file);

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

void http_post_file_deinit(UDF_INIT *initid)
{
    http_multipart_data *data = (http_multipart_data *)initid->ptr;

    if (data)
    {
        // 释放任何已分配的内存
        int i;
        for ( i = 0; i < data->file_count; i++)
        {
            if (data->files[i].data)
            {
                free(data->files[i].data);
                data->files[i].data = NULL;
            }
        }
     
        for ( i = 0; i < data->post_field_count; i++)
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

/* ------------------------ HTTP POST Multiple Files with Headers and Timeout ------------------------------ */

my_bool http_post_files_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    http_multipart_data *container;
    int field_count, file_count, type_count;

    if (args->arg_count != 6)
    {
        strncpy(message,
                "six arguments must be supplied: http_post_files('<url>','<field_names>','<filenames>','<content_types>','<headers>','<timeout_ms>').",
                MYSQL_ERRMSG_SIZE);
        return 1;
    }

    // 所有参数均视为字符串类型
    args->arg_type[0] = STRING_RESULT;
    args->arg_type[1] = STRING_RESULT;
    args->arg_type[2] = STRING_RESULT;
    args->arg_type[3] = STRING_RESULT;
    args->arg_type[4] = STRING_RESULT;
    args->arg_type[5] = STRING_RESULT;

    initid->max_length = CURL_UDF_MAX_SIZE;
    container = (http_multipart_data *)malloc(sizeof(http_multipart_data));

    if (!container)
    {
        strncpy(message, "out of memory", MYSQL_ERRMSG_SIZE);
        return 1;
    }

    memset(container, 0, sizeof(http_multipart_data));
    strncpy(container->url, args->args[0], sizeof(container->url) - 1);
    container->url[sizeof(container->url) - 1] = '\0';

    // 解析field names (comma-separated)
    if (args->args[1])
    {
        field_count = count_comma_items(args->args[1]);
        if (field_count > MAX_FORM_FIELDS)
        {
            field_count = MAX_FORM_FIELDS;
            strncpy(message, "too many field names (max 10)", MYSQL_ERRMSG_SIZE);
            free(container);
            return 1;
        }

        char temp_fields[MAX_FORM_FIELDS][256];
        parse_comma_separated(args->args[1], (char **)temp_fields, MAX_FORM_FIELDS);

        int i;
        for ( i = 0; i < field_count && i < MAX_FORM_FIELDS; i++)
        {
            if (strlen(temp_fields[i]) == 0)
                strcpy(container->files[i].name, "file");
            else
                strncpy(container->files[i].name, temp_fields[i], MAX_FILENAME_LENGTH - 1);
            container->files[i].name[MAX_FILENAME_LENGTH - 1] = '\0';
        }
        container->file_count = field_count;
    }

    // 解析filenames (comma-separated)
    if (args->args[2])
    {
        file_count = count_comma_items(args->args[2]);
        if (file_count > MAX_FORM_FIELDS)
        {
            file_count = MAX_FORM_FIELDS;
            strncpy(message, "too many files (max 10)", MYSQL_ERRMSG_SIZE);
            free(container);
            return 1;
        }

        char temp_files[MAX_FORM_FIELDS][MAX_FILENAME_LENGTH];
        parse_comma_separated(args->args[2], (char **)temp_files, MAX_FORM_FIELDS);

        int i;
        for ( i = 0; i < file_count && i < MAX_FORM_FIELDS; i++)
        {
            strncpy(container->files[i].filename, temp_files[i], MAX_FILENAME_LENGTH - 1);
            container->files[i].filename[MAX_FILENAME_LENGTH - 1] = '\0';
        }
        container->file_count = file_count;
    }

    // 解析content types (comma-separated)
    if (args->args[3])
    {
        type_count = count_comma_items(args->args[3]);
        if (type_count > MAX_FORM_FIELDS)
        {
            type_count = MAX_FORM_FIELDS;
        }

        char temp_types[MAX_FORM_FIELDS][128];
        parse_comma_separated(args->args[3], (char **)temp_types, MAX_FORM_FIELDS);

        int i;
        for ( i = 0; i < type_count && i < MAX_FORM_FIELDS; i++)
        {
            if (strlen(temp_types[i]) == 0)
                strcpy(container->files[i].content_type, "application/octet-stream");
            else
                strncpy(container->files[i].content_type, temp_types[i], sizeof(container->files[i].content_type) - 1);
            container->files[i].content_type[sizeof(container->files[i].content_type) - 1] = '\0';
        }
    }

    // 解析请求头字符串
    if (args->args[4])
    {
        container->headers_str = strndup(args->args[4], args->lengths[4]);
    }

    // 解析超时时间（毫秒）
    long timeout_ms = REQ_TIMEOUT_MS;
    if (args->args[5] && args->lengths[5] > 0)
    {
        char *endptr;
        long val = strtol(args->args[5], &endptr, 10);
        if (*args->args[5] != '\0' && *endptr == '\0' && val > 0)
        {
            timeout_ms = val;
        }
    }
    container->timeout_ms = timeout_ms;

    initid->ptr = (char *)container;

    return 0;
}

char *http_post_files(UDF_INIT *initid, UDF_ARGS *args,
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
    FILE *file = NULL;

    if (!res || !initid->ptr || !data || !args || args->arg_count < 6)
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

        // 设置基本选项
        chunk = curl_slist_append(chunk, "Expect:");

        // 解析自定义请求头
        if (data->headers_str)
        {
            char *line = strtok(data->headers_str, "\n");
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
        }

        curl_easy_setopt(curl, CURLOPT_URL, data->url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, data->timeout_ms);

        // 构建multipart表单
        struct curl_httppost *formpost = NULL;
        struct curl_httppost *lastptr = NULL;

        // 添加所有文件字段
        int i;
        for (i = 0; i < data->file_count && i < MAX_FORM_FIELDS; i++)
        {
            if (strlen(data->files[i].filename) > 0)
            {
                file = fopen(data->files[i].filename, "rb");
                if (file)
                {
                    fseek(file, 0, SEEK_END);
                    long file_size = ftell(file);
                    fseek(file, 0, SEEK_SET);

                    formcode = curl_formadd(&formpost,
                                            &lastptr,
                                            CURLFORM_COPYNAME, data->files[i].name,
                                            CURLFORM_FILE, data->files[i].filename,
                                            CURLFORM_CONTENTTYPE, data->files[i].content_type,
                                            CURLFORM_END);

                    if (formcode != CURL_FORMADD_OK)
                    {
                        fprintf(stderr, "<%s:%d> ERROR. curl_formadd failed for file %s\n", __FUNCTION__, __LINE__, data->files[i].filename);
                        fclose(file);
                        *length = 0;
                        return NULL;
                    }
                }
                else
                {
                    fprintf(stderr, "<%s:%d> ERROR. Cannot open file %s\n", __FUNCTION__, __LINE__, data->files[i].filename);
                    *length = 0;
                    return NULL;
                }
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

        // 清理资源 
        for (  i = 0; i < data->file_count && i < MAX_FORM_FIELDS; i++)
        {
            if (strlen(data->files[i].filename) > 0)
            {
                file = fopen(data->files[i].filename, "rb");
                if (file) fclose(file);
            }
        }

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

void http_post_files_deinit(UDF_INIT *initid)
{
    http_multipart_data *data = (http_multipart_data *)initid->ptr;

    if (data)
    {
        // 释放任何已分配的内存
        int i;
        for ( i = 0; i < data->file_count; i++)
        {
            if (data->files[i].data)
            {
                free(data->files[i].data);
                data->files[i].data = NULL;
            }
        } 
        for (  i = 0; i < data->post_field_count; i++)
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



/* Module unload function to cleanup global resources */
void mysql_udf_http_deinit(void)
{
    if (curl_initialized)
    {
        curl_global_cleanup();
        curl_initialized = 0;
    }
}