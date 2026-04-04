-- 测试文件上传功能的SQL脚本

-- 创建测试文件
\! echo "Creating test file..."
echo "This is a test file content for upload" > /tmp/test_file.txt

-- 检查是否已加载mysql-udf-http扩展
SELECT 'Checking if http_post_file function exists...' as info;
SELECT LOAD_FILE('/tmp/test_file.txt') as file_content;

-- 注意：实际的http_post_file函数需要在MySQL中注册后才能使用
-- 这里只是演示如何使用该函数的语法
/*
假设我们已经通过以下方式注册了函数：
CREATE FUNCTION http_post_file RETURNS STRING SONAME 'mysql-udf-http.so';

然后可以这样调用：
SELECT http_post_file(
    'http://httpbin.org/post',
    'uploaded_file',
    '/tmp/test_file.txt',
    'text/plain',
    '30000'
) as response;
*/

SELECT 'File upload function syntax example:' as info;
SELECT 'SELECT http_post_file(''http://httpbin.org/post'', ''field_name'', ''/path/to/file'', ''content/type'', ''timeout_ms'');' as syntax;