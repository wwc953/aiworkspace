#include <curl/curl.h>
/* Common definitions for all functions */
#define CURL_UDF_MAX_SIZE 1024*1024*1024  // Increased to 1GB for large file uploads
#define MAX_FILENAME_LENGTH 256
#define MAX_FORM_FIELDS 10
#define MAX_DATA_LENGTH 1024*1024         // Max data per field (1MB)

#define VERSION_STRING "1.0\n"
#define VERSION_STRING_LENGTH 4

typedef struct st_curl_results st_curl_results;
struct st_curl_results {
  char *result;
  size_t size;
  CURL* curl;
};

typedef struct {
  char name[MAX_FILENAME_LENGTH];
  char filename[MAX_FILENAME_LENGTH];
  char content_type[128];
  unsigned char *data;                    // Raw binary data
  unsigned long length;                   // Data length in bytes
} file_upload_field;
