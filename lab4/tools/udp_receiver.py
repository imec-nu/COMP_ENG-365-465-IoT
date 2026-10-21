"""Task 2: receive ten consecutive UDP packets and average nine intervals.

Run with --esp-ip set to the board's Wi-Fi IPv4 address from Serial Monitor.
Socket setup, datagram parsing, and sender/session filtering are supplied.
"""
import argparse
from dataclasses import dataclass
import ipaddress
import re
import socket
import time

UINT32_MAX = (1 << 32) - 1
PACKET_RE = re.compile(rb"L4 ([0-9a-f]{8}) ([0-9]{1,10})")


@dataclass(frozen=True)
class Packet:
    session: str
    sequence: int


def parse_packet(payload: bytes):
    """Supplied parser: check the L4 format and unsigned 32-bit sequence."""
    match = PACKET_RE.fullmatch(payload)
    if not match:
        return None
    sequence = int(match.group(2))
    if sequence > UINT32_MAX:
        return None
    return Packet(match.group(1).decode("ascii"), sequence)


class ConsecutiveRun:
    def __init__(self):
        self.timestamps_ns = []
        self.last_sequence = None
        self.first_sequence = None
        self.restarts = 0

    def add(self, sequence: int, received_ns: int) -> bool:
        # TODO 2.2: collect receive times for ten consecutive sequence values.
        # For the first packet, set self.first_sequence to sequence.
        # For later packets, compare sequence with (self.last_sequence + 1)
        # modulo 2**32, so the largest unsigned 32-bit value wraps to zero.
        # A gap, duplicate, or reordered packet starts a new run: clear
        # self.timestamps_ns, set self.first_sequence to the current sequence,
        # and increment self.restarts. The current packet is packet 1.
        # For every packet, append received_ns to self.timestamps_ns and assign
        # sequence to self.last_sequence. Return True at exactly ten timestamps,
        # otherwise False. The caller stops receiving when True is returned.
        raise NotImplementedError("Complete TODO 2.2 in ConsecutiveRun.add().")

    def mean_interval_ms(self) -> float:
        # TODO 2.3: require exactly ten values in self.timestamps_ns;
        # otherwise raise ValueError. These are PC monotonic times in ns.
        # Ten arrivals span nine intervals. Subtract the first timestamp from
        # the last and divide by nine to get the mean interval in nanoseconds.
        # Divide by 1_000_000 to return the result in milliseconds.
        raise NotImplementedError("Complete TODO 2.3 in mean_interval_ms().")


class SourceLock:
    """Supplied filter for one board IP, UDP source port, and boot session."""
    def __init__(self, expected_ip):
        self.expected_ip = expected_ip
        self.key = None

    def accepts(self, address, packet):
        if address[0] != self.expected_ip:
            return False
        key = (address[0], address[1], packet.session)
        if self.key is None:
            self.key = key
            print(f"Locked to {key[0]}:{key[1]}, session {key[2]}.")
        return key == self.key


def ipv4(text):
    try:
        address = ipaddress.IPv4Address(text)
        if address.is_loopback or address.is_unspecified or address.is_multicast or int(address) == UINT32_MAX:
            raise ValueError
        return str(address)
    except ValueError as exc:
        raise argparse.ArgumentTypeError("use the board's unicast LAN IPv4 from Serial Monitor") from exc


def receive(args):
    run = ConsecutiveRun()
    source = SourceLock(args.esp_ip)
    deadline_ns = time.monotonic_ns() + int(args.timeout * 1_000_000_000)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(("0.0.0.0", args.port))
        sock.settimeout(0.5)
        print(f"Listening on UDP port {args.port} for {args.esp_ip} (up to {args.timeout:g} s).")
        print("A board restart changes its session; restart this receiver too.")
        while time.monotonic_ns() < deadline_ns:
            try:
                payload, address = sock.recvfrom(1024)
                received_ns = time.monotonic_ns()  # Timestamp immediately after recvfrom.
            except socket.timeout:
                continue
            packet = parse_packet(payload)
            if packet is None or not source.accepts(address, packet):
                continue
            previous_restarts = run.restarts
            complete = run.add(packet.sequence, received_ns)
            if run.restarts != previous_restarts:
                print(f"Continuity broken; restarting at sequence {packet.sequence}.")
            print(f"seq={packet.sequence}: {len(run.timestamps_ns)}/10")
            if complete:
                print(f"Ten consecutive packets: {run.first_sequence} through {run.last_sequence}.")
                print(f"Mean inter-arrival time over 9 intervals: {run.mean_interval_ms():.3f} ms")
                return
    raise RuntimeError("No complete run. Check TODO 2.1/2.2, IPs, same LAN, and firewall/client isolation.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--esp-ip", type=ipv4, required=True, help="board's Wi-Fi IPv4 from Serial Monitor")
    parser.add_argument("--port", type=int, default=4210)
    parser.add_argument("--timeout", type=float, default=60.0)
    args = parser.parse_args()
    if not 1 <= args.port <= 65535 or not 0 < args.timeout <= 3600:
        parser.error("port must be 1..65535 and timeout must be >0 and <=3600 seconds")
    try:
        receive(args)
    except KeyboardInterrupt:
        print("Stopped.")
    except (OSError, RuntimeError, NotImplementedError, ValueError) as exc:
        parser.exit(1, f"UDP receiver: {exc}\n")


if __name__ == "__main__":
    main()
