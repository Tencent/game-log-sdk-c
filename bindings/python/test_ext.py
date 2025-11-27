#!/usr/bin/env python3
"""
Test script for tglog C extension - performance comparison
"""

import sys
import time
import argparse

def test_c_extension():
    """Test the C extension performance"""
    try:
        import tglog_ext
    except ImportError as e:
        print(f"Failed to import tglog_ext: {e}")
        print("\nPlease build the extension first:")
        print("  ./build_ext.sh")
        print("or")
        print("  python3 setup_ext.py build_ext --inplace")
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
    
    # Create client using C extension
    print(f"Creating TglogClient with C extension...")
    client = tglog_ext.TglogClient("tcp", args.ip, args.port)
    
    # Test message
    log = "CarryOutItemFlow|/cnprod-a/36/settlementsvr/nanjing/11.176.45.170/1|2025-07-14 13:39:23|wx39308a0a1bd83d22|1|36|18182882159345025898|18182882159345025898|2340077323481036542|11090000053|1|648520772929162293|0|4|0.100|109|1|0|||0|0|||648530419497318933|2340077323481036542||\n"
    
    # Start timer
    start_time = time.time()
    
    # Send messages with callback
    print(f"Sending {args.to_send} messages...")
    for i in range(args.to_send):
        client.send(log, use_callback=True)
    
    # Record send time
    send_time = time.time()
    send_elapsed = (send_time - start_time) * 1000
    print(f"send time elapsed: {send_elapsed:.6f} milliseconds")
    
    # Wait for callbacks to complete
    while True:
        stats = client.get_stats()
        total_callbacks = stats['sent'] + stats['error']
        if total_callbacks >= args.to_send:
            break
        time.sleep(0.001)
    
    # Record done time
    done_time = time.time()
    done_elapsed = (done_time - start_time) * 1000
    
    # Print results
    print(f"sent {stats['sent']} logs, errors {stats['error']}")
    print(f"done time elapsed: {done_elapsed:.6f} milliseconds")
    
    # Calculate throughput
    throughput = args.to_send / (done_elapsed / 1000.0)
    print(f"Throughput: {throughput:.0f} msg/s")
    
    # Get metrics
    metrics_json = client.get_metrics()
    if metrics_json:
        import json
        print("\nMetrics:")
        # Parse and pretty-print the JSON string
        metrics = json.loads(metrics_json)
        print(json.dumps(metrics, indent=2))
    
    # Cleanup
    del client
    
    return 0

if __name__ == "__main__":
    sys.exit(test_c_extension())