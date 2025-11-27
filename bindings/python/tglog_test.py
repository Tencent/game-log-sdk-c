#!/usr/bin/env python3
"""
TgLog Python test client - equivalent to test/tglog_test.c
"""

import sys
import time
import argparse
import threading
import json
import ctypes
from ctypes import CFUNCTYPE, c_char_p, c_int, c_void_p
from tglog_client import TglogClientPy, reset_global_counter, get_global_counter

# ============================================================
# MODULE-LEVEL TEST CALLBACK FOR HIGH PERFORMANCE
# ============================================================

# Global counter for the test callback
test_counter = {'sent': 0, 'error': 0}
test_counter_lock = threading.Lock()

# Define the callback function at module level
def test_callback_func(payload, err_code, err_msg, user_data):
    """Module-level callback function for testing"""
    with test_counter_lock:
        if err_code == 0:
            test_counter['sent'] += 1
        else:
            test_counter['error'] += 1
            # Only print errors
            if err_code != 0:
                try:
                    payload_str = payload.decode('utf-8') if payload else ""
                    err_msg_str = err_msg.decode('utf-8') if err_msg else ""
                    print(f"callback: {payload_str}, {err_code}, {err_msg_str}")
                except:
                    pass

# Compile the callback into a C function pointer at module load time
# This is the key for performance - it's created once when the module loads
SendCallbackFunc = CFUNCTYPE(None, c_char_p, c_int, c_char_p, c_void_p)
TEST_CALLBACK = SendCallbackFunc(test_callback_func)


class TokenBucket:
    """Token bucket for rate limiting"""
    
    def __init__(self, capacity, refill_rate):
        self.capacity = capacity
        self.tokens = capacity
        self.refill_rate = refill_rate
        self.last_check = time.time()
        self.lock = threading.Lock()
    
    def refill_tokens(self):
        """Refill tokens based on elapsed time"""
        now = time.time()
        time_passed = now - self.last_check
        
        if time_passed > 0:
            new_tokens = int(time_passed * self.refill_rate)
            self.tokens = min(self.tokens + new_tokens, self.capacity)
            self.last_check = now
    
    def consume_token(self, tokens_needed=1):
        """Try to consume tokens"""
        with self.lock:
            self.refill_tokens()
            if self.tokens >= tokens_needed:
                self.tokens -= tokens_needed
                return True
            return False


def print_usage(prog_name):
    """Print usage information"""
    print(f"Usage: {prog_name} [options]")
    print("Options:")
    print("  -t, --to-send NUM      Number of messages to send (default: 1000000)")
    print("  -r, --rate NUM         Token bucket refill rate (default: 1000000)")
    print("  -p, --protocol PROTO   Protocol type: udp or tcp (default: tcp)")
    print("  -i, --ip IP_ADDRESS    Target IP address (default: dev.tglog.com)")
    print("  -P, --port PORT        Target port (default: 20001)")
    print("  -w, --worker NUM       Number of workers (default: 8)")
    print("  -h, --help             Show this help message")


def parse_args():
    """Parse command line arguments - matching C version"""
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('-t', '--to-send', type=int, default=1000000,
                       help='Number of messages to send')
    parser.add_argument('-r', '--rate', type=int, default=1000000,
                       help='Token bucket refill rate')
    parser.add_argument('-p', '--protocol', choices=['tcp', 'udp'], default='tcp',
                       help='Protocol type: udp or tcp')
    parser.add_argument('-i', '--ip', type=str, default='dev.tglog.com',
                       help='Target IP address')
    parser.add_argument('-P', '--port', type=int, default=20001,
                       help='Target port')
    parser.add_argument('-w', '--worker', type=int, default=8,
                       help='Number of workers')
    parser.add_argument('-h', '--help', action='store_true',
                       help='Show this help message')
    
    args = parser.parse_args()
    
    if args.help:
        print_usage(sys.argv[0])
        sys.exit(0)
    
    return args


def main():
    """Main function - equivalent to C version"""
    # Parse command line arguments
    args = parse_args()
    
    # Set variables matching C version
    to_send = args.to_send
    rate = args.rate
    protocol = args.protocol
    ip = args.ip
    port = args.port
    worker = args.worker
    
    # Create token bucket
    bucket = TokenBucket(rate + 1, rate)
    
    # Create client
    try:
        client = TglogClientPy(protocol=protocol, host=ip, port=port, workers=worker)
    except Exception as e:
        print(f"failed to create client: {e}")
        return 1
    
    # Reset the test counter
    with test_counter_lock:
        test_counter['sent'] = 0
        test_counter['error'] = 0
    
    # Start timer
    start_time = time.time()
    
    # Log message (same as C version)
    log = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n"
    
    # Send messages using the test callback
    # This should be as fast as the global callback since it's compiled at module load
    i = 0
    while True:
        # if you want to limit the rate, you can use the token bucket
        # if not bucket.consume_token(1):
        #     time.sleep(0.0005)  # 500 microseconds
        #     continue
        
        # Send with test callback - passes as ctypes._CFuncPtr
        # This avoids creating any new wrapper and uses the direct C path
        client.send(log, callback=TEST_CALLBACK)
        
        i += 1
        if i == to_send:
            break
    
    # Record send time
    end_time = time.time()
    elapsed_ms = (end_time - start_time) * 1000
    print(f"send time elapsed: {elapsed_ms:.6f} milliseconds")
    
    # Wait for all callbacks to complete (matching C version)
    while test_counter['sent'] < to_send:
        time.sleep(0.001)  # 1000 microseconds
    
    # Print sent count
    print(f"sent {test_counter['sent']} logs")
    
    # Record done time
    end_time = time.time()
    elapsed_ms = (end_time - start_time) * 1000
    print(f"done time elapsed: {elapsed_ms:.6f} milliseconds")
    
    # Get and print metrics
    time.sleep(1)
    metrics = client.get_metrics()
    if metrics:
        print(json.dumps(metrics, indent=2))
    
    # Destroy client
    client.destroy()
    
    return 0


if __name__ == "__main__":
    sys.exit(main())