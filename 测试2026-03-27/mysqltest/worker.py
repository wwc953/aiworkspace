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
BATCH_SIZE = 50  # 每批处理的任务数量，避免连接过多
MAX_WORKERS = 5  # 最大工作线程数


# ================= 主程序 =================
def process_single_task(queue_id, order_id, conn):
    """处理单个任务的函数 - 确保线程安全"""
    cursor = None
    try:
        # 1. 确保每个线程使用自己的游标 - 添加线程锁保护游标创建
        with threading.Lock():
            cursor = conn.cursor(pymysql.cursors.DictCursor)

        # 2. 开启事务
        conn.begin()

        # 3. 记录日志 - 添加线程安全日志处理
        with threading.Lock():
            logging.info(f"Processing Queue ID: {queue_id}, Order ID: {order_id}")

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
        # 确保游标被正确关闭
        if cursor:
            cursor.close()


def worker_thread(task_queue):
    """工作线程函数"""
    while True:
        try:
            task = task_queue.get(timeout=1)
            if task is None:  # 终止信号
                break

            queue_id, order_id, conn = task
            process_single_task(queue_id, order_id, conn)

        except Exception as e:
            logging.error(f"Worker thread error: {e}")
        finally:
            task_queue.task_done()


def process_queue():
    """修改后的 process_queue 函数 - 先查询所有数据，然后使用多线程执行"""
    conn = None
    worker_threads = []
    task_queue = Queue()
    num_workers = MAX_WORKERS  # 设置工作线程数量
    batch_size = 50  # 限制每批处理的任务数量，避免连接过多

    try:
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor(pymysql.cursors.DictCursor)

        logging.info("🚀 Worker Started. Querying all available tasks...")

        while True:
            try:
                # 1. 查询所有待处理的任务（不使用FOR UPDATE，避免锁定）
                select_sql = """
                SELECT id, order_id
                FROM orders_queue
                WHERE attempt_count < %s
                ORDER BY id ASC
                LIMIT %s
                """
                cursor.execute(select_sql, (MAX_RETRY, batch_size))
                rows = cursor.fetchall()

                if not rows:
                    # 没有任务，休眠
                    time.sleep(SLEEP_INTERVAL)
                    continue

                logging.info(f"Found {len(rows)} tasks to process")

                # 2. 为每个任务创建一个独立的数据库连接
                task_conns = {}
                for row in rows:
                    queue_conn = pymysql.connect(**DB_CONFIG)
                    task_conns[(row['id'], row['order_id'])] = queue_conn

                # 3. 启动工作线程
                for _ in range(num_workers):
                    thread = Thread(target=worker_thread, args=(task_queue,))
                    thread.daemon = True
                    thread.start()
                    worker_threads.append(thread)

                # 4. 将任务添加到队列
                for row in rows:
                    task_queue.put((row['id'], row['order_id'], task_conns[(row['id'], row['order_id'])]))

                # 5. 等待所有任务完成
                task_queue.join()

                # 6. 关闭所有任务连接
                for conn in task_conns.values():
                    conn.close()

                # 7. 清理已完成的任务
                cleanup_sql = "DELETE FROM orders_queue WHERE id IN (%s)" % ','.join(['%s'] * len(rows))
                cursor.execute(cleanup_sql, [row['id'] for row in rows])
                conn.commit()

                logging.info("All tasks completed. Waiting for new tasks...")

            except Exception as e:
                logging.error(f"Main loop error: {e}")
                # 确保在异常时关闭连接
                for conn in task_conns.values():
                    try:
                        conn.close()
                    except:
                        pass
                time.sleep(1)

    except KeyboardInterrupt:
        logging.info("Worker stopped by user.")
        sys.exit(0)
    except Exception as e:
        logging.critical(f"Worker Fatal Error: {e}")
        sys.exit(1)
    finally:
        # 清理资源
        for thread in worker_threads:
            if thread.is_alive():
                task_queue.put(None)  # 发送终止信号
        if conn:
            conn.close()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
    process_queue()