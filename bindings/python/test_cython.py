#!/usr/bin/env python3
"""
Test script for tglog Cython extension - performance comparison
"""

import sys
import time
import argparse

def test_cython():
    """Test the Cython extension performance"""
    try:
        import tglog_cython
    except ImportError as e:
        print(f"Failed to import tglog_cython: {e}")
        print("\nPlease build the extension first:")
        print("  ./build_cython.sh")
        print("or")
        print("  python3 setup_cython.py build_ext --inplace")
        print("\nNote: You need to install Cython first:")
        print("  pip3 install Cython")
        return 1
    
    # Parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--to-send', type=int, default=1000000,
                       help='Number of messages to send')
    parser.add_argument('-i', '--ip', type=str, default='dev.tglog.com',
                       help='Target IP address')
    parser.add_argument('-P', '--port', type=int, default=20001,
                       help='Target port')
    args = parser.parse_args()
    
    # Create client using Cython extension
    print(f"Creating TglogClient with Cython extension...")
    client = tglog_cython.TglogClient("tcp", args.ip, args.port)
    
    # Test message - encode to bytes for Cython
    log = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n"
    log_bytes = log.encode('utf-8')
    
    # Start timer
    start_time = time.time()
    
    # Send messages one by one with callback
    print(f"Sending {args.to_send} messages...")
    
    for i in range(args.to_send):
        client.send(log_bytes, True)  # with callback
    
    # Record send time
    send_time = time.time()
    send_elapsed = (send_time - start_time) * 1000
    print(f"send time elapsed: {send_elapsed:.6f} milliseconds")
    
    # Wait for callbacks to complete (same as ext version)
    wait_start = time.time()
    while True:
        sent, error = client.get_callback_stats()
        total_callbacks = sent + error
        if total_callbacks >= args.to_send:
            break
        time.sleep(0.001)
        # Add timeout protection (10 seconds)
        if time.time() - wait_start > 10:
            print(f"Warning: Timeout waiting for callbacks. Got {total_callbacks}/{args.to_send} (sent={sent}, error={error})")
            break
    
    # Record done time
    done_time = time.time()
    done_elapsed = (done_time - start_time) * 1000
    
    # Print results
    print(f"sent {sent} logs, errors {error}")
    print(f"done time elapsed: {done_elapsed:.6f} milliseconds")
    
    # Calculate throughput
    throughput = args.to_send / (done_elapsed / 1000.0)
    print(f"Throughput: {throughput:.0f} msg/s")
    
    # Get metrics
    metrics = client.get_metrics()
    if metrics:
        import json
        print("\nMetrics:")
        print(json.dumps(metrics, indent=2))
    
    # Cleanup
    client.destroy()
    
    return 0

if __name__ == "__main__":
    sys.exit(test_cython())