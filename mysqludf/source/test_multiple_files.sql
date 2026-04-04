-- Multiple File Upload Test Script

-- 创建多个测试文件
\! echo "Creating multiple test files..."

echo "This is test document 1 content" > /tmp/doc1.txt
echo "This is test document 2 content with more text to demonstrate file upload functionality" > /tmp/doc2.txt
echo '{"name": "test-image", "size": 1024, "type": "png"}' > /tmp/metadata.json

-- 示例1: 单文件上传（使用新函数）
SELECT 'Example 1: Single file upload with http_post_file' as info;
/*
SELECT http_post_file(
    'https://httpbin.org/post',
    'single_file',
    '/tmp/doc1.txt',
    'text/plain',
    'X-Source: mysql-udf-test
X-File-Count: 1',
    '30000'
) as response;
*/

-- 示例2: 多文件上传
SELECT 'Example 2: Multiple file upload with http_post_files' as info;
/*
SELECT http_post_files(
    'https://httpbin.org/post',
    'doc1,doc2',
    '/tmp/doc1.txt,/tmp/doc2.txt',
    'text/plain,text/plain',
    'X-Multi-Upload: true
X-Total-Files: 2
User-Agent: MySQL-Multi-Client',
    '60000'
) as response;
*/

-- 示例3: 混合类型文件上传
SELECT 'Example 3: Mixed file types upload' as info;
/*
SELECT http_post_files(
    'https://api.example.com/upload',
    'document,metadata',
    '/tmp/doc1.txt,/tmp/metadata.json',
    'text/plain,application/json',
    'Authorization: Bearer mixed-token
Content-Type: multipart/form-data
X-File-Types: text+json',
    '90000'
) as response;
*/

-- 示例4: 单个字段名上传多个文件
SELECT 'Example 4: Single field name for multiple files' as info;
/*
SELECT http_post_files(
    'https://archive.example.com/batch',
    'files',  -- 所有文件使用同一个字段名
    '/tmp/doc1.txt,/tmp/doc2.txt',
    'text/plain,text/plain',
    'Authorization: Bearer archive-key
X-Batch-Mode: enabled
Cache-Control: no-cache',
    '120000'
) as response;
*/

-- 显示测试文件内容用于验证
SELECT 'Test file 1 contents:' as info;
SELECT LOAD_FILE('/tmp/doc1.txt') as file1_content;

SELECT 'Test file 2 contents:' as info;
SELECT LOAD_FILE('/tmp/doc2.txt') as file2_content;

SELECT 'Metadata file contents:' as info;
SELECT LOAD_FILE('/tmp/metadata.json') as metadata_content;

-- 函数语法对比
SELECT 'Function syntax comparison:' as info;
SELECT 'Single file: http_post_file(<url>, <field_name>, <filename>, <content_type>, <headers>, <timeout_ms>)' as single_syntax,
       'Multiple files: http_post_files(<url>, <field_names>, <filenames>, <content_types>, <headers>, <timeout_ms>)' as multi_syntax;

-- 参数说明
SELECT 'Parameters for http_post_files:' as info;
SELECT '  Field Names: Comma-separated field names (e.g., "doc1,doc2")' as param1,
       '  Filenames: Comma-separated file paths (e.g., "/tmp/file1.txt,/tmp/file2.txt")' as param2,
       '  Content Types: Comma-separated MIME types (e.g., "text/plain,application/json")' as param3,
       '  Headers: Custom headers (one per line)' as param4,
       '  Timeout: Request timeout in milliseconds' as param5;

-- 注意事项
SELECT 'Important Notes:' as info;
SELECT '• Maximum 10 files per request' as note1,
       '• All arrays must have same number of items' as note2,
       '• Empty values will use defaults (e.g., "file" for field names)' as note3,
       '• Files are processed in order of the comma-separated lists' as note4;