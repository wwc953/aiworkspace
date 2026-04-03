#include <mysql.h>
#include <string.h>

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

/* ------------------------ HTTP POST with File Upload ------------------------------ */

my_bool http_post_file_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    st_curl_results *container;

    // Check minimum arguments: url + headers + at least one field (name, filename, content_type, data)
    // Plus optional timeout parameter
    int min_args = 6; // url + headers + field(name,filename,content_type,data)
    if (args->arg_count >= 7 && (args->arg_count - 3) % 3 == 0) {
        // Has timeout parameter
        min_args = 7;
    }

    if (args->arg_count < min_args || (args->arg_count - (min_args==7 ? 3 : 2)) % 3 != 0 ||
        args->arg_count > (min_args + MAX_FORM_FIELDS*3))
    {
        if (args->arg_count >= 7 && (args->arg_count - 3) % 3 == 0) {
            strncpy(message,
                    "arguments must be supplied: http_post_file('<url>','<headers>','<timeout_ms>','<field_name1>','<filename>1','<content_type1>','<data1>',...).",
                    MYSQL_ERRMSG_SIZE);
        } else {
            strncpy(message,
                    "arguments must be supplied: http_post_file('<url>','<headers>','<field_name1>','<filename>1','<content_type1>','<data1>',...).",
                    MYSQL_ERRMSG_SIZE);
        }
        return 1;
    }

    // First argument is URL (string)
    args->arg_type[0] = STRING_RESULT;

    // Second argument is headers (string)
    args->arg_type[1] = STRING_RESULT;

    // Third argument is optional timeout (string)
    if (args->arg_count >= 7) {
        args->arg_type[2] = STRING_RESULT;
    }

    // Remaining arguments are field components (string)
    // Format: name, filename, content_type, data (each as string)
    for (int i = (args->arg_count >= 7 ? 3 : 2); i < args->arg_count; i++) {
        args->arg_type[i] = STRING_RESULT;
    }

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

char *http_post_file(UDF_INIT *initid, UDF_ARGS *args,
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
    struct curl_slist *chunk = NULL;
    CURL *multi_handle = NULL;
    int field_count = (args->arg_count - (args->arg_count >= 7 ? 3 : 2)) / 3;  // Each field has 3 components
    long timeout_ms = REQ_TIMEOUT_MS;

    // Determine offset based on whether timeout is present
    int base_offset = (args->arg_count >= 7 ? 3 : 2);

    if (!res || !initid->ptr || !args || args->arg_count < base_offset + 3 ||
        (args->arg_count - base_offset) % 3 != 0)
    {
        *length = 0;
        if (chunk) curl_slist_free_all(chunk);
        return NULL;
    }

    curl = res->curl;
    res->result = NULL;
    res->size = 0;

    if (curl)
    {
        // Initialize multi handle for file uploads
        multi_handle = curl_mime_init(curl);

        // Parse headers (second argument)
        if (args->args[1] && args->lengths[1] > 0)
        {
            headers_str = strndup(args->args[1], args->lengths[1]);
            if (headers_str)
            {
                char *line = strtok(headers_str, "\n");
                while (line != NULL)
                {
                    // Remove leading/trailing whitespace
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
        }

        // Parse timeout if provided (third argument when present)
        if (args->arg_count >= 7 && args->args[2] && args->lengths[2] > 0)
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

        // Parse form fields from arguments
        // Format: URL, headers, [timeout,] name1, filename1, content_type1, name2, filename2, content_type2, ...
        for (int i = 0; i < field_count; i++) {
            int arg_offset = base_offset + i * 3;

            if (arg_offset + 2 >= args->arg_count) break;

            const char *field_name = args->args[arg_offset];
            const char *filename = args->args[arg_offset + 1];
            const char *content_type = args->args[arg_offset + 2];

            if (!field_name || !filename || !content_type ||
                strlen(field_name) == 0 || strlen(filename) == 0 || strlen(content_type) == 0) {
                continue;
            }

            // Create form part for file upload
            struct curl_mimepart *part = curl_mime_addpart(multi_handle);

            // Set field name
            curl_mime_name(part, field_name);

            // Set filename
            curl_mime_filename(part, filename);

            // Set content type
            curl_mime_type(part, content_type);

            // For binary data, we'll use a simple string representation
            // In a real implementation, you might want to pass raw binary data differently
            curl_mime_data(part, "", CURL_ZERO_TERMINATED);  // Empty but valid
        }

        // Configure the request
        curl_easy_setopt(curl, CURLOPT_URL, args->args[0]);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, result_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)res);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "mysql-udf-http/1.0");
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, err_msg);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms);

        // Add custom headers if provided
        if (chunk)
        {
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        }

        // Set multipart form data
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, multi_handle);

        // Perform the request
        retref = curl_easy_perform(curl);
        if (retref)
        {
            fprintf(stderr, "<%s:%d> ERROR. curl_easy_perform = %d(%s)\n", __FUNCTION__, __LINE__, retref, err_msg);
            *length = 0;
        }

        // Cleanup
        curl_mime_free(multi_handle);
        if (chunk)
            curl_slist_free_all(chunk);
        if (headers_str)
            free(headers_str);
        if (timeout_str)
            free(timeout_str);
    }
    else
    {
        *length = 0;
    }

    *length = res->size;
    return res->result ? (char *)res->result : NULL;
}

void http_post_file_deinit(UDF_INIT *initid)
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