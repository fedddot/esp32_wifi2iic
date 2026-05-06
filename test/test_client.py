#!/usr/bin/env python3
"""Test client: serializes WiFi I2C relay requests to protobuf and sends via HTTP.

Usage:
    python3 test_client.py <url> write <i2c_address> <timeout_ms> <hex_data>
    python3 test_client.py <url> read  <i2c_address> <timeout_ms> <length>
    python3 test_client.py <url> write-receive <i2c_address> <timeout_ms> <hex_write_data> <read_length>

Examples:
    python3 test_client.py http://192.168.4.1/iic write 0x50 100 deadbeef
    python3 test_client.py http://192.168.4.1/iic read  0x50 100 4
    python3 test_client.py http://192.168.4.1/iic/write_read write-receive 0x50 100 deadbeef 4

Dependencies:
    pip install protobuf requests

Generate the Python bindings with:
    python3 -m grpc_tools.protoc -I src --python_out=. src/service.proto
"""

import argparse
import sys
import time

try:
    import requests
except ImportError:
    sys.exit("Missing dependency: pip install requests")

try:
    import service_pb2
except ImportError:
    sys.exit(
        "service_pb2.py not found. Generate it with:\n"
        "  python3 -m grpc_tools.protoc -I src --python_out=. src/service.proto"
    )


def build_write_request(i2c_address: int, timeout_ms: int, data: bytes) -> service_pb2.WifiI2CRelayWriteRequest:
    req = service_pb2.WifiI2CRelayWriteRequest()
    req.address = i2c_address
    req.timeout_ms = timeout_ms
    req.data = data
    return req


def build_read_request(i2c_address: int, timeout_ms: int, length: int) -> service_pb2.WifiI2CRelayReadRequest:
    req = service_pb2.WifiI2CRelayReadRequest()
    req.address = i2c_address
    req.timeout_ms = timeout_ms
    req.length = length
    return req


def build_write_receive_request(
    i2c_address: int,
    timeout_ms: int,
    write_data: bytes,
    read_length: int,
) -> service_pb2.WifiI2CRelayWriteReceiveRequest:
    req = service_pb2.WifiI2CRelayWriteReceiveRequest()
    req.address = i2c_address
    req.timeout_ms = timeout_ms
    req.write_data = write_data
    req.read_length = read_length
    return req


def print_write_response(raw: bytes) -> None:
    resp = service_pb2.WifiI2CRelayWriteResponse()
    resp.ParseFromString(raw)
    result_name = service_pb2.Result.Name(resp.result)
    print(f"Write response: {result_name}")


def print_read_response(raw: bytes) -> None:
    resp = service_pb2.WifiI2CRelayReadResponse()
    resp.ParseFromString(raw)
    result_name = service_pb2.Result.Name(resp.result)
    print(f"Read response: {result_name}, data: {resp.data.hex()}")


def print_write_receive_response(raw: bytes) -> None:
    resp = service_pb2.WifiI2CRelayWriteReceiveResponse()
    resp.ParseFromString(raw)
    result_name = service_pb2.Result.Name(resp.result)
    print(f"WriteReceive response: {result_name}, data: {resp.data.hex()}")


def main():
    parser = argparse.ArgumentParser(description="Send WiFi I2C relay request via protobuf HTTP")
    parser.add_argument("url", help="Target URL, e.g. http://192.168.4.1/iic")

    subparsers = parser.add_subparsers(dest="command", required=True)

    write_parser = subparsers.add_parser("write", help="Send a write request")
    write_parser.add_argument("i2c_address", type=lambda x: int(x, 0), help="I2C device address (e.g. 0x50)")
    write_parser.add_argument("timeout_ms", type=int, help="Timeout in milliseconds")
    write_parser.add_argument("data", help="Data bytes as a hex string (e.g. deadbeef)")

    read_parser = subparsers.add_parser("read", help="Send a read request")
    read_parser.add_argument("i2c_address", type=lambda x: int(x, 0), help="I2C device address (e.g. 0x50)")
    read_parser.add_argument("timeout_ms", type=int, help="Timeout in milliseconds")
    read_parser.add_argument("length", type=int, help="Number of bytes to read")

    write_receive_parser = subparsers.add_parser("write-receive", help="Send a write-receive request")
    write_receive_parser.add_argument("i2c_address", type=lambda x: int(x, 0), help="I2C device address (e.g. 0x50)")
    write_receive_parser.add_argument("timeout_ms", type=int, help="Timeout in milliseconds")
    write_receive_parser.add_argument("write_data", help="Data bytes to write as a hex string (e.g. deadbeef)")
    write_receive_parser.add_argument("read_length", type=int, help="Number of bytes to read back")

    args = parser.parse_args()

    if args.command == "write":
        request = build_write_request(args.i2c_address, args.timeout_ms + 2000, bytes.fromhex(args.data))
    elif args.command == "read":
        request = build_read_request(args.i2c_address, args.timeout_ms + 2000, args.length)
    else:
        request = build_write_receive_request(
            args.i2c_address,
            args.timeout_ms + 2000,
            bytes.fromhex(args.write_data),
            args.read_length,
        )

    payload = request.SerializeToString()
    print(f"Sending {len(payload)} bytes to {args.url} ...")

    with requests.Session() as session:
        session.headers.update({"Content-Type": "application/x-protobuf"})
        for i in range(30):
            print(f"Sending request {i+1}/30")
            t_start = time.perf_counter()
            if args.command in {"read", "write-receive"}:
                response = session.get(args.url, data=payload, timeout=10)
            else:
                response = session.post(args.url, data=payload, timeout=10)
            elapsed_ms = (time.perf_counter() - t_start) * 1000
            response.raise_for_status()
            if args.command == "read":
                print_read_response(response.content)
            elif args.command == "write-receive":
                print_write_receive_response(response.content)
            else:
                print_write_response(response.content)
            print(f"  Round-trip: {elapsed_ms:.1f} ms")


if __name__ == "__main__":
    main()
