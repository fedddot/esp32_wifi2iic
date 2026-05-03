#!/usr/bin/env python3
"""Test client: serializes WiFi I2C relay requests to protobuf and sends via HTTP POST.

Usage:
    python3 test_client.py <url> write <i2c_address> <timeout_ms> <hex_data>
    python3 test_client.py <url> read  <i2c_address> <timeout_ms> <length>

Examples:
    python3 test_client.py http://192.168.4.1/relay write 0x50 100 deadbeef
    python3 test_client.py http://192.168.4.1/relay read  0x50 100 4

Dependencies:
    pip install protobuf requests

Generate the Python bindings with:
    python3 -m grpc_tools.protoc -I src --python_out=. src/wifi_iic_relay.proto
"""

import argparse
import sys
import time

try:
    import requests
except ImportError:
    sys.exit("Missing dependency: pip install requests")

try:
    import wifi_iic_relay_pb2
except ImportError:
    sys.exit(
        "wifi_iic_relay_pb2.py not found. Generate it with:\n"
        "  python3 -m grpc_tools.protoc -I src --python_out=. src/wifi_iic_relay.proto"
    )


def build_write_request(i2c_address: int, timeout_ms: int, data: bytes) -> wifi_iic_relay_pb2.WifiI2CRelayRequest:
    req = wifi_iic_relay_pb2.WifiI2CRelayRequest()
    req.write_request.address = i2c_address
    req.write_request.timeout_ms = timeout_ms
    req.write_request.data = data
    return req


def build_read_request(i2c_address: int, timeout_ms: int, length: int) -> wifi_iic_relay_pb2.WifiI2CRelayRequest:
    req = wifi_iic_relay_pb2.WifiI2CRelayRequest()
    req.read_request.address = i2c_address
    req.read_request.timeout_ms = timeout_ms
    req.read_request.length = length
    return req


def print_response(raw: bytes) -> None:
    resp = wifi_iic_relay_pb2.WifiI2CRelayResponse()
    resp.ParseFromString(raw)
    which = resp.WhichOneof("response")
    if which == "write_response":
        result_name = wifi_iic_relay_pb2.Result.Name(resp.write_response.result)
        print(f"Write response: {result_name}")
    elif which == "read_response":
        result_name = wifi_iic_relay_pb2.Result.Name(resp.read_response.result)
        print(f"Read response: {result_name}, data: {resp.read_response.data.hex()}")
    elif which == "undefined_response":
        result_name = wifi_iic_relay_pb2.Result.Name(resp.undefined_response)
        print(f"Undefined response: {result_name}")
    else:
        print("Empty or unrecognised response")


def main():
    parser = argparse.ArgumentParser(description="Send WiFi I2C relay request via protobuf HTTP POST")
    parser.add_argument("url", help="Target URL, e.g. http://192.168.4.1/relay")

    subparsers = parser.add_subparsers(dest="command", required=True)

    write_parser = subparsers.add_parser("write", help="Send a write request")
    write_parser.add_argument("i2c_address", type=lambda x: int(x, 0), help="I2C device address (e.g. 0x50)")
    write_parser.add_argument("timeout_ms", type=int, help="Timeout in milliseconds")
    write_parser.add_argument("data", help="Data bytes as a hex string (e.g. deadbeef)")

    read_parser = subparsers.add_parser("read", help="Send a read request")
    read_parser.add_argument("i2c_address", type=lambda x: int(x, 0), help="I2C device address (e.g. 0x50)")
    read_parser.add_argument("timeout_ms", type=int, help="Timeout in milliseconds")
    read_parser.add_argument("length", type=int, help="Number of bytes to read")

    args = parser.parse_args()

    if args.command == "write":
        request = build_write_request(args.i2c_address, args.timeout_ms, bytes.fromhex(args.data))
    else:
        request = build_read_request(args.i2c_address, args.timeout_ms, args.length)

    payload = request.SerializeToString()
    print(f"Sending {len(payload)} bytes to {args.url} ...")

    with requests.Session() as session:
        session.headers.update({"Content-Type": "application/x-protobuf"})
        for i in range(30):
            print(f"Sending request {i+1}/30")
            t_start = time.perf_counter()
            if args.command == "read":
                response = session.get(args.url, data=payload, timeout=10)
            else:
                response = session.post(args.url, data=payload, timeout=10)
            elapsed_ms = (time.perf_counter() - t_start) * 1000
            response.raise_for_status()
            print_response(response.content)
            print(f"  Round-trip: {elapsed_ms:.1f} ms")


if __name__ == "__main__":
    main()
