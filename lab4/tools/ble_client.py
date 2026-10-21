"""Task 1: receive HELLO messages on TX and send matching ACKs on RX.

Complete TODOs 1.4 and 1.5 below. The board measures RTT from sending HELLO
until its matching ACK arrives.
"""
import argparse
import asyncio
import re

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
TX_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
RX_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a9"
HELLO_RE = re.compile(rb"HELLO ([0-9]{1,10})")
UINT32_MAX = (1 << 32) - 1


async def reply_to_hello(client, payload: bytes) -> bool:
    # TODO 1.4: check the full message with HELLO_RE.fullmatch(payload)
    # and read the sequence as an integer. Return False for a malformed
    # message or a sequence above UINT32_MAX.
    # Build "ACK <sequence>" with the same number and encode it as ASCII,
    # without a newline or NUL. Await client.write_gatt_char() on RX_UUID
    # with response=True, then return True when the write completes.
    raise NotImplementedError("Complete TODO 1.4 in reply_to_hello().")


async def subscribe(client, callback):
    # TODO 1.5: call client.start_notify() with TX_UUID and callback,
    # and await the call to enable delivery of TX notifications.
    raise NotImplementedError("Complete TODO 1.5 in subscribe().")


async def find_board(args, scanner):
    """Find the board by name or OS identifier; names may arrive in scan responses."""
    for attempt in range(1, 4):
        print(f"Scanning ({attempt}/3)...")
        found = await scanner.discover(timeout=4.0, return_adv=True,
                                       service_uuids=[SERVICE_UUID])
        matches = []
        for device, advertisement in found.values():
            if args.name and advertisement.local_name == args.name:
                matches.append(device)
            elif args.address and device.address.lower() == args.address.lower():
                matches.append(device)
        if len(matches) > 1:
            raise RuntimeError("Several boards have this name; give yours a unique BOARD_NAME.")
        if matches:
            return matches[0]
    raise RuntimeError("Board not found. Check its name, Serial output, and Bluetooth permissions.")


async def run(args):
    try:
        from bleak import BleakClient, BleakScanner
    except ImportError as exc:
        raise RuntimeError("Install the supplied requirements.txt with your Python interpreter.") from exc
    device = await find_board(args, BleakScanner)
    queue = asyncio.Queue(maxsize=32)
    disconnected = asyncio.Event()
    overflow = asyncio.Event()
    replies = 0

    def on_disconnect(_client):
        disconnected.set()

    def on_notify(_characteristic, data):
        # Queue each notification. The loop below awaits replies in order.
        try:
            queue.put_nowait(bytes(data))
        except asyncio.QueueFull:
            overflow.set()

    print(f"Connecting to {device.name} ({device.address})...")
    async with BleakClient(device, disconnected_callback=on_disconnect, timeout=20.0) as client:
        await subscribe(client, on_notify)
        print("Subscribed. Waiting for HELLO messages...")
        deadline = asyncio.get_running_loop().time() + 40.0
        while asyncio.get_running_loop().time() < deadline:
            if overflow.is_set():
                raise RuntimeError("Notification queue overflow; reconnect and run again.")
            if disconnected.is_set():
                raise RuntimeError("Board disconnected. Re-run this command for a fresh run.")
            try:
                payload = await asyncio.wait_for(queue.get(), timeout=0.5)
            except asyncio.TimeoutError:
                continue
            if payload == b"DONE":
                print(f"Board completed its 10 matched exchanges; PC sent {replies} ACKs.")
                print("Record the mean RTT printed by the board's Serial Monitor.")
                return
            print(f"Board → PC: {payload.decode('ascii', errors='replace')}")
            if await reply_to_hello(client, payload):
                replies += 1
            else:
                print("Ignoring a malformed HELLO.")
        raise RuntimeError("No DONE within 40 s. Check TODO 1.2/1.3/1.4 and board Serial output.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--name", help="exact unique BOARD_NAME from config.h")
    target.add_argument("--address", help="BLE identifier from a scan; macOS uses a UUID")
    args = parser.parse_args()
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        print("Stopped.")
    except Exception as exc:
        parser.exit(1, f"BLE client: {exc}\n")


if __name__ == "__main__":
    main()
