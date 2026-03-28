#!/usr/bin/env python3
"""
Test script for worker.py to verify connection management fix
"""

import sys
sys.path.append('.')

from worker import process_queue
import logging

if __name__ == "__main__":
    # Set up logging
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')

    print("Testing worker.py with connection management fixes...")
    print("Press Ctrl+C to stop after a few seconds")

    try:
        # Run for a short time to test
        process_queue()
    except KeyboardInterrupt:
        print("\nTest stopped by user")
    except Exception as e:
        print(f"Error during test: {e}")