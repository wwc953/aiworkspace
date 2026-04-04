-- HTTP POST File Upload with Headers - Test Examples

-- 创建测试文件
\! echo "Creating test files..."

echo "This is a test file for HTTP upload" > /tmp/test_file.txt
echo '{"message": "test data", "timestamp": "2026-04-04"}' > /tmp/test_data.json

-- 示例1: 基础文件上传（无自定义headers）
SELECT 'Example 1: Basic file upload (no custom headers)' as info;
/*
SELECT http_post_file(
    'https://httpbin.org/post',
    'uploaded_file',
    '/tmp/test_file.txt',
    'text/plain',
    '',  -- 空字符串表示使用默认headers
    '30000'
) as response;
*/

-- 示例2: 带Authorization header的文件上传
SELECT 'Example 2: File upload with Authorization header' as info;
/*
SELECT http_post_file(
    'https://api.example.com/upload',
    'document',
    '/tmp/test_file.txt',
    'text/plain',
    'Authorization: Bearer your-api-token-here
X-File-Type: text-document',
    '60000'
) as response;
*/

-- 示例3: JSON数据上传
SELECT 'Example 3: JSON data upload with custom headers' as info;
/*
SELECT http_post_file(
    'https://jsonplaceholder.typicode.com/posts',
    'data',
    '/tmp/test_data.json',
    'application/json',
    'Content-Type: application/json
User-Agent: MySQL-UDF-Client
X-API-Version: v2',
    '45000'
) as response;
*/

-- 示例4: 多行headers示例
SELECT 'Example 4: Multiple custom headers' as info;
/*
SELECT http_post_file(
    'https://httpbin.org/post',
    'attachment',
    '/tmp/test_file.txt',
    'text/plain',
    'Authorization: Bearer token123
Content-Type: multipart/form-data
X-Upload-Source: mysql-udf
X-Timestamp: ' || UNIX_TIMESTAMP() || '
Cache-Control: no-cache',
    '90000'
) as response;
*/

-- 显示测试文件内容用于验证
SELECT 'Test file contents:' as info;
SELECT LOAD_FILE('/tmp/test_file.txt') as file_content;

SELECT 'JSON test data:' as info;
SELECT LOAD_FILE('/tmp/test_data.json') as json_content;

-- 函数语法总结
SELECT 'Function syntax:' as info;
SELECT 'http_post_file(<url>, <field_name>, <filename>, <content_type>, <headers>, <timeout_ms>)' as syntax;

-- 参数说明
SELECT 'Parameters:' as info;
SELECT '  URL: Target endpoint URL' as param1,
       'Field Name: Form field name for the file' as param2,
       'Filename: Local path to the file' as param3,
       'Content Type: MIME type (e.g., text/plain, application/json)' as param4,
       'Headers: Custom headers (one per line, e.g., Authorization: Bearer token)' as param5,
       'Timeout: Request timeout in milliseconds' as param6;