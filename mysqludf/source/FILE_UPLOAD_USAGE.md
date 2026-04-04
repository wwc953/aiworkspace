# HTTP POST File Upload with Headers and Timeout

This document describes the new `http_post_file` function that supports file upload via HTTP POST with custom headers and timeout configuration.

## Function Signature

```sql
SELECT http_post_file(
    '<url>',
    '<field_name>',
    '<filename>',
    '<content_type>',
    '<timeout_ms>'
) as response;
```

## Parameters

1. **url** (STRING): The target URL for the POST request
2. **field_name** (STRING): The form field name for the file upload
3. **filename** (STRING): Path to the local file to upload
4. **content_type** (STRING): MIME type of the file (e.g., 'text/plain', 'application/json', 'image/jpeg')
5. **timeout_ms** (STRING): Request timeout in milliseconds

## Usage Examples

### Basic File Upload
```sql
SELECT http_post_file(
    'https://httpbin.org/post',
    'myfile',
    '/path/to/document.pdf',
    'application/pdf',
    '30000'
) as response;
```

### Upload Text File
```sql
SELECT http_post_file(
    'https://api.example.com/upload',
    'logfile',
    '/var/log/app.log',
    'text/plain',
    '60000'
) as response;
```

### Upload JSON Data
```sql
SELECT http_post_file(
    'https://api.example.com/data',
    'payload',
    '/tmp/data.json',
    'application/json',
    '15000'
) as response;
```

## Requirements

1. **MySQL UDF Extension**: The mysql-udf-http extension must be compiled and installed
2. **CURL Library**: The system must have libcurl development libraries installed
3. **File Permissions**: MySQL must have read permissions for the file being uploaded
4. **Network Access**: MySQL server must have outbound network access to the target URL

## Compilation Notes

To compile the extension with file upload support:

```bash
cd mysql-udf-http-1.0/src
make clean
make
sudo make install
```

The extension will be built as `mysql-udf-http.so` and should be placed in your MySQL plugin directory.

## Registration in MySQL

After compilation and installation, register the functions in MySQL:

```sql
-- Register the file upload function
CREATE FUNCTION http_post_file RETURNS STRING SONAME 'mysql-udf-http.so';

-- You may also want to register other functions if not already done:
CREATE FUNCTION http_get RETURNS STRING SONAME 'mysql-udf-http.so';
CREATE FUNCTION http_post RETURNS STRING SONAME 'mysql-udf-http.so';
CREATE FUNCTION http_get_headers RETURNS STRING SONAME 'mysql-udf-http.so';
CREATE FUNCTION http_post_headers RETURNS STRING SONAME 'mysql-udf-http.so';
```

## Error Handling

The function returns:
- **NULL**: If there's an error or invalid parameters
- **HTTP Response**: As a string containing the full HTTP response from the server

Common errors:
- File not found or inaccessible
- Network connectivity issues
- Invalid timeout values
- Server-side errors (returned in the HTTP response)

## Security Considerations

1. **File Path Validation**: Ensure the filename parameter points to a valid, accessible file
2. **Timeout Limits**: Set appropriate timeout values to prevent hanging requests
3. **Content Type**: Validate content types to prevent security issues
4. **Network Security**: Use HTTPS URLs when handling sensitive data

## Troubleshooting

### Common Issues

1. **"Function doesn't exist" error**
   - Verify the .so file is in the correct MySQL plugin directory
   - Check MySQL error log for loading issues
   - Ensure proper file permissions

2. **File upload fails**
   - Verify MySQL has read permissions for the specified file
   - Check if the file path is accessible from the MySQL server
   - Test with a simple text file first

3. **Network timeout**
   - Increase the timeout value
   - Verify network connectivity from MySQL server
   - Check firewall settings

### Debugging Tips

1. Test with `httpbin.org` for reliable testing:
   ```sql
   SELECT http_post_file(
       'https://httpbin.org/post',
       'test',
       '/tmp/test.txt',
       'text/plain',
       '30000'
   ) as response;
   ```

2. Check MySQL error logs for detailed error messages
3. Verify CURL is properly linked during compilation

## Performance Considerations

1. **Large Files**: For files > 1GB, ensure adequate memory allocation
2. **Concurrent Uploads**: Multiple simultaneous uploads may impact performance
3. **Timeout Values**: Balance between user experience and resource usage
4. **Connection Pooling**: Reuse connections when making multiple requests

## Dependencies

- **libcurl**: Required for HTTP operations
- **MySQL Client Libraries**: For UDF integration
- **Standard C Library**: Memory management and string operations