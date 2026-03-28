# worker.py
# 依赖安装: pip install pymysql

import pymysql
import time
import logging
import sys
import threading
from threading import Thread
from queue import Queue

# ================= 配置 =================
DB_CONFIG = {
    'host': '192.168.0.105',
    'port': 3306,
    'user': 'root',
    'password': 'rootpassword',  # <--- 请修改为你的数据库密码
    'database': 'udf_test_db',
    'charset': 'utf8mb4',
    'autocommit': False,  # 手动控制事务
    'connect_timeout': 10,
    'read_timeout': 30,
    'write_timeout': 30,
    'max_allowed_packet': 16777216  # 16MB
}

MAX_RETRY = 3  # 最大重试次数
SLEEP_INTERVAL = 0.1  # 没有任务时的休眠时间 (秒)
WORKER_THREADS = 50  # 固定5个工作线程


# ================= 主程序 =================
def process_single_task(queue_id, order_id):
    """处理单个任务的函数 - 确保线程安全，自己创建数据库连接"""
    conn = None
    cursor = None
    try:
        # 1. 为每个任务创建新的数据库连接
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor(pymysql.cursors.DictCursor)

        # 2. 开启事务
        conn.begin()

        # 3. 记录日志 - 添加线程安全日志处理
        # with threading.Lock():
        #     logging.info(f"Processing Queue ID: {queue_id}, Order ID: {order_id}")

        # 4. 调用 UDF (核心逻辑)
        udf_sql = """
                  SELECT http_post(%s, CONCAT('order_id=', %s)) AS response
                  FROM orders \
                  WHERE id = %s \
                  """

        # 这里的连接是独立的，不会触发 Error 1442
        cursor.execute(udf_sql, (
            'http://192.168.0.105:6000/api/udf_callback',  # URL (请确保与 mock_service 一致)
            order_id,
            order_id
        ))
        udf_result = cursor.fetchone()
        udf_response = udf_result['response'] if udf_result else 'NULL'

        # 5. 更新业务表
        update_order_sql = """
                           UPDATE orders
                           SET udf_response = %s, \
                               status       = 'SENT'
                           WHERE id = %s \
                           """
        cursor.execute(update_order_sql, (udf_response, order_id))

        # 6. 从队列中删除任务
        delete_queue_sql = "DELETE FROM orders_queue WHERE id = %s"
        cursor.execute(delete_queue_sql, (queue_id,))

        # 7. 提交事务
        conn.commit()

        # 8. 线程安全的日志输出
        with threading.Lock():
            logging.debug(f"Success: Order {order_id} processed.")
        return True

    except Exception as e:
        # 发生异常，回滚
        if conn:
            try:
                conn.rollback()
            except:
                pass
        # 线程安全的错误日志
        with threading.Lock():
            logging.error(f"Task Error (Queue ID: {queue_id}, Order ID: {order_id}): {e}")
        return False

    finally:
        # 确保游标和连接被正确关闭
        if cursor:
            cursor.close()
        if conn:
            conn.close()


def consumer_worker(task_queue):
    """消费者工作线程 - 从阻塞队列获取任务并处理"""
    thread_name = threading.current_thread().name

    while True:
        try:
            # 从阻塞队列获取任务（如果队列为空，会阻塞等待）
            
            queue_id, order_id = task_queue.get(timeout=30)  # 30秒超时
            
            logging.info(f"queue size: {task_queue.qsize()}   ")

            # 记录线程信息
            logging.info(f"[{thread_name}] Processing Queue ID: {queue_id}, Order ID: {order_id}")

            # 处理任务
            success = process_single_task(queue_id, order_id)

            if success:
                logging.info(f"[{thread_name}] Successfully processed Order ID: {order_id}")
            else:
                logging.error(f"[{thread_name}] Failed to process Order ID: {order_id}")

            # 标记任务完成
            task_queue.task_done()

        except Exception as e:
            logging.error(f"[{thread_name}] Consumer worker error: {e}")
            break  # 退出循环，线程结束


def process_queue():
    """生产者-消费者模式：先查询数据放入阻塞队列，再启动5个线程消费"""
    conn = None
    task_queue = Queue()  # 阻塞队列

    try:
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor(pymysql.cursors.DictCursor)

        logging.info("🚀 Worker Started. Querying all available tasks...")

        while True:
            try:
                # 1. 生产者：查询所有待处理的任务并放入阻塞队列
                select_sql = """
                SELECT id, order_id
                FROM orders_queue
                WHERE attempt_count < %s
                ORDER BY id ASC
                """
                cursor.execute(select_sql, (MAX_RETRY,))
                rows = cursor.fetchall()

                if not rows:
                    # 没有任务，休眠
                    time.sleep(SLEEP_INTERVAL)
                    continue

                logging.info(f"Found {len(rows)} tasks to process")

                # 将任务放入阻塞队列
                for row in rows:
                    task_queue.put((row['id'], row['order_id']))
                
                conn.commit()
                
                logging.info(f"All {task_queue.qsize()} tasks added to blocking queue")

                # 2. 消费者：启动5个工作线程从阻塞队列获取任务
                threads = []
                for i in range(WORKER_THREADS):
                    thread_name = f"Consumer-{i+1}"
                    logging.info(f"Starting {thread_name}")
                    thread = Thread(target=consumer_worker, args=(task_queue,), name=thread_name)
                    thread.daemon = True
                    thread.start()
                    threads.append(thread)

                # 3. 等待所有工作线程完成
                for thread in threads:
                    thread_name = thread.name
                    thread.join(timeout=120)  # 增加超时时间以处理大量任务
                    logging.info(f"Completed {thread_name}")
                
                logging.info("All tasks completed. Waiting for new tasks...")

            except Exception as e:
                logging.error(f"Main loop error: {e}")
                time.sleep(1)

    except KeyboardInterrupt:
        logging.info("Worker stopped by user.")
        sys.exit(0)
    except Exception as e:
        logging.critical(f"Worker Fatal Error: {e}")
        sys.exit(1)
    finally:
        if conn:
            conn.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    process_queue()