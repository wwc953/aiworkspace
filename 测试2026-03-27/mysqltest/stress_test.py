# stress_test.py
# 依赖安装: pip install pymysql

import pymysql
import time
import random
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
import logging

# ================= 配置区域 =================
DB_CONFIG = {
    'host': '192.168.0.105',
    'port': 3306,
    'user': 'root',
    'password': 'rootpassword',  # <--- 请修改为你的数据库密码
    'database': 'udf_test_db',
    'charset': 'utf8mb4',
    'connect_timeout': 5
}

CONCURRENT_THREADS = 50  # 并发线程数
TOTAL_REQUESTS = 5000  # 总测试请求数

# ================= 日志与统计 =================
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(threadName)s - %(message)s')
logger = logging.getLogger(__name__)

stats = {
    "success": 0,
    "db_exception": 0
}
stats_lock = threading.Lock()


def run_task(task_id):
    conn = None
    try:
        # 每个线程独立连接
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor()

        order_no = f"ORD-{int(time.time() * 1000)}-{random.randint(1000, 9999)}"
        amount = round(random.uniform(10, 500), 2)

        # 插入数据 (触发 Trigger -> 写入 orders_queue)
        sql = "INSERT INTO orders (order_no, amount) VALUES (%s, %s)"
        cursor.execute(sql, (order_no, amount))
        conn.commit()

        with stats_lock:
            stats["success"] += 1

        return True

    except Exception as e:
        with stats_lock:
            stats["db_exception"] += 1
        logger.error(f"Task {task_id} Error: {e}")
        return False
    finally:
        if conn: conn.close()


def main():
    logger.info(f"🚀 开始压力测试: {CONCURRENT_THREADS} 并发, 总量 {TOTAL_REQUESTS}")
    logger.info("⚠️ 请确保 worker.py 正在运行以处理队列！")

    start_time = time.time()

    with ThreadPoolExecutor(max_workers=CONCURRENT_THREADS) as executor:
        futures = [executor.submit(run_task, i) for i in range(TOTAL_REQUESTS)]

        for i, future in enumerate(as_completed(futures)):
            future.result()
            if (i + 1) % 100 == 0:
                logger.info(f"进度: {i + 1}/{TOTAL_REQUESTS}")

    total_time = time.time() - start_time

    # ================= 报告 =================
    print("\n" + "=" * 60)
    print("📊 压力测试结果")
    print("=" * 60)
    print(f"总耗时:          {total_time:.2f} 秒")
    print(f"TPS:             {TOTAL_REQUESTS / total_time:.2f}")
    print(f"成功插入:        {stats['success']}")
    print(f"数据库异常:      {stats['db_exception']}")
    print("-" * 60)

    if stats['db_exception'] == 0:
        print("✅ 结论: 数据库连接稳定，未发生崩溃。")
    else:
        print("❌ 结论: 检测到数据库连接异常。")

    print("=" * 60)


if __name__ == "__main__":
    main()