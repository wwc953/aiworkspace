# mock_service.py
# 依赖安装: pip install flask

from flask import Flask, request, jsonify
import random
import time
import threading
import logging

app = Flask(__name__)
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# 全局统计 (线程安全)
stats = {
    "total_received": 0,
    "success_count": 0,
    "error_count": 0
}
lock = threading.Lock()


@app.route('/api/udf_callback', methods=['POST'])
def udf_callback():
    """模拟被 MySQL UDF 调用的微服务接口"""
    start_time = time.time()

    # 1. 模拟随机处理耗时 (0.05s - 1.5s)
    # 偶尔模拟慢请求 (3s)，测试 UDF 超时机制
    delay = random.uniform(0.05, 1.5)
    if random.random() < 0.05:  # 5% 概率慢请求
        delay = 3.0
        logger.warning(f"Simulating slow response ({delay:.2f}s) for request")

    time.sleep(delay)

    # 2. 模拟随机业务错误 (5% 概率返回 500)
    if random.random() < 0.05:
        with lock:
            stats["error_count"] += 1
        logger.error("Simulated internal server error")
        return jsonify({"error": "Internal Server Error", "simulated": True}), 500

    # 3. 正常处理
    # data = request.get_json() or {}
    data = request.form.to_dict() or {}

    with lock:
        stats["total_received"] += 1
        stats["success_count"] += 1

    process_time = time.time() - start_time
    logger.info(f"Received Order: {data.get('order_id', 'N/A')} | Time: {process_time:.3f}s")

    return jsonify({
        "status": "processed",
        "order_no": data.get('order_id'),
        "process_time": process_time
    }), 200


@app.route('/stats', methods=['GET'])
def get_stats():
    """查看当前服务接收统计"""
    return jsonify(stats)


if __name__ == '__main__':
    # 监听所有网卡，端口 5000
    print("🚀 Mock Service Starting on port 6000...")
    app.run(host='0.0.0.0', port=6000, threaded=True)