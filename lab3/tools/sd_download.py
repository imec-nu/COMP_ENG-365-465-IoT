#!/usr/bin/env python3
"""Download SD files from the companion XIAO ESP32S3 USB serial sketch."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import errno
import os
from pathlib import Path, PurePosixPath
import re
import sys
import tempfile
import time
from typing import Optional
import zlib

MAX_LINE = 1024
MAX_CHUNK = 1024
MAX_FILE_SIZE = 0xFFFFFFFF
MAX_PATH_BYTES = 240


class ExportError(Exception):
    """A protocol, transfer, or destination validation failure."""


@dataclass(frozen=True)
class RemoteFile:
    path: str
    size: int


def is_hidden_path(remote_path: str) -> bool:
    """Include files inside dot-prefixed directories in the hidden filter."""
    return any(part.startswith('.') for part in remote_path.split('/') if part)


def safe_relative_path(remote_path: str) -> PurePosixPath:
    """Map an absolute SD file path to a portable, non-traversing local path."""
    if not remote_path.startswith('/') or remote_path.startswith('//'):
        raise ExportError('An SD file path must start with exactly one slash.')
    if len(remote_path.encode('utf-8')) > MAX_PATH_BYTES:
        raise ExportError('SD path exceeds the 240-byte protocol limit.')
    parts = remote_path[1:].split('/')
    reserved = {'CON', 'PRN', 'AUX', 'NUL'}
    reserved.update(f'{prefix}{n}' for prefix in ('COM', 'LPT') for n in range(1, 10))
    for part in parts:
        if not part or part in ('.', '..'):
            raise ExportError('Empty, dot, and parent-directory path components are not allowed.')
        if any(ord(c) < 32 or ord(c) == 127 or c in '\\:*?"<>|' for c in part):
            raise ExportError(f'Filename cannot be safely exported: {remote_path!r}')
        if part.endswith((' ', '.')) or part.split('.')[0].upper() in reserved:
            raise ExportError(f'Filename is not portable to Windows: {remote_path!r}')
    return PurePosixPath(*parts)


def destination_for(root: Path, remote_path: str) -> Path:
    relative = safe_relative_path(remote_path)
    base = root.resolve()
    destination = base.joinpath(*relative.parts)
    try:
        destination.resolve().relative_to(base)
    except ValueError as exc:
        raise ExportError(f'Destination escapes the output folder: {destination}') from exc
    # Reject symlink components even when they currently point inside the tree.
    current = base
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            raise ExportError(f'Refusing a symlink destination: {current}')
    return destination


def _decimal(value: str) -> int:
    if not re.fullmatch(r'[0-9]{1,10}', value):
        raise ExportError(f'Invalid byte count: {value!r}')
    number = int(value)
    if number > MAX_FILE_SIZE:
        raise ExportError('File exceeds the FAT32 size limit.')
    return number


def _crc(value: str) -> int:
    if not re.fullmatch(r'[0-9A-Fa-f]{8}', value):
        raise ExportError(f'Invalid CRC32: {value!r}')
    return int(value, 16)


class ExportClient:
    def __init__(self, serial_port, timeout: float = 10.0):
        if timeout <= 0:
            raise ValueError('timeout must be positive')
        self.serial = serial_port
        self.timeout = timeout
        self.usable = True

    def _send(self, command: str, timeout: Optional[float] = None) -> None:
        message = ('SDX1 ' + command + '\n').encode('ascii')
        if len(message) > MAX_LINE:
            raise ExportError('Command is too long.')
        deadline = time.monotonic() + (self.timeout if timeout is None else timeout)
        offset = 0
        while offset < len(message):
            if time.monotonic() >= deadline:
                raise ExportError('Timed out writing to the USB port.')
            written = self.serial.write(message[offset:])
            if written is None or written < 0 or written > len(message) - offset:
                raise ExportError('Serial write returned an invalid byte count.')
            offset += written
            if not written:
                time.sleep(0.001)

    def _read_exact(self, count: int, deadline: Optional[float] = None) -> bytes:
        if deadline is None:
            deadline = time.monotonic() + self.timeout
        result = bytearray()
        while len(result) < count:
            if time.monotonic() >= deadline:
                raise ExportError('Transfer timed out. Check USB; reset the board before retrying.')
            block = self.serial.read(count - len(result))
            if len(block) > count - len(result):
                raise ExportError('Serial read exceeded the requested byte count.')
            if block:
                result.extend(block)
            else:
                time.sleep(0.001)
        return bytes(result)

    def _line(self, deadline: Optional[float] = None) -> str:
        if deadline is None:
            deadline = time.monotonic() + self.timeout
        raw = bytearray()
        while len(raw) < MAX_LINE:
            byte = self._read_exact(1, deadline)
            if byte == b'\n':
                try:
                    return raw.rstrip(b'\r').decode('ascii')
                except UnicodeDecodeError as exc:
                    raise ExportError('Invalid protocol text. Reset the board and retry.') from exc
            raw.extend(byte)
        raise ExportError('Protocol line is too long. Check that the exporter sketch is running.')

    def _response(self) -> list[str]:
        parts = self._line().split()
        if len(parts) >= 2 and parts[:2] == ['SDX1', 'ERR']:
            code = ' '.join(parts[2:]) or 'UNKNOWN'
            raise ExportError(f'Board reported {code}. Check the SD card and reset before retrying.')
        if len(parts) < 2 or parts[0] != 'SDX1':
            raise ExportError('Unexpected output. Upload sd_usb_export and close Serial Monitor.')
        return parts

    def _require_usable(self) -> None:
        if not self.usable:
            raise ExportError('The previous transfer failed. Reconnect and reset before retrying.')

    def ping(self) -> None:
        self._require_usable()
        self._send('PING')
        deadline = time.monotonic() + self.timeout
        while True:
            line = self._line(deadline)
            if line == 'SDX1 READY 1':
                return
            if line.startswith('SDX1 ERR '):
                raise ExportError(f'Board reported {line[9:]}. Check the SD card and reset.')
            if line.startswith('SDX1 '):
                raise ExportError('Exporter protocol mismatch. Upload the matching sketch.')
            # ESP32 boot messages may precede the first reply.

    def list_files(self) -> list[RemoteFile]:
        self._require_usable()
        self._send('LIST')
        files = []
        seen = set()
        while True:
            parts = self._response()
            if parts == ['SDX1', 'END']:
                return sorted(files, key=lambda entry: entry.path)
            if len(parts) != 4 or parts[1] != 'FILE':
                raise ExportError('Invalid file-list response.')
            size = _decimal(parts[2])
            encoded = parts[3]
            if len(encoded) % 2 or not re.fullmatch(r'[0-9a-fA-F]+', encoded):
                raise ExportError('Invalid encoded filename.')
            try:
                path = bytes.fromhex(encoded).decode('utf-8')
            except UnicodeDecodeError as exc:
                raise ExportError('The SD filename is not valid UTF-8.') from exc
            safe_relative_path(path)
            key = path.casefold()
            if key in seen:
                raise ExportError(f'Duplicate or case-conflicting filename: {path}')
            seen.add(key)
            files.append(RemoteFile(path, size))
            if len(files) > 100000:
                raise ExportError('File list exceeds 100,000 entries.')

    def download(self, remote_path: str, destination: Path,
                 expected_size: Optional[int] = None, progress=None) -> int:
        self._require_usable()
        safe_relative_path(remote_path)
        destination = Path(destination)
        if destination.exists() or destination.is_symlink():
            raise FileExistsError(f'Output already exists; choose a new folder or filename: {destination}')
        if expected_size is not None and not 0 <= expected_size <= MAX_FILE_SIZE:
            raise ExportError('Invalid expected file size.')
        destination.parent.mkdir(parents=True, exist_ok=True)
        fd, temporary_name = tempfile.mkstemp(prefix='.sd-export-', suffix='.part', dir=destination.parent)
        temporary = Path(temporary_name)
        requested = False
        try:
            with os.fdopen(fd, 'wb') as output:
                requested = True
                self._send('GET ' + remote_path.encode('utf-8').hex())
                header = self._response()
                if len(header) != 3 or header[1] != 'DATA':
                    raise ExportError('Expected a file-size header.')
                size = _decimal(header[2])
                if expected_size is not None and size != expected_size:
                    raise ExportError('File size differs from the listing; list files again.')
                received = 0
                whole_crc = 0
                while received < size:
                    header = self._response()
                    if len(header) != 4 or header[1] != 'CHUNK':
                        raise ExportError('Expected an audio/image data chunk.')
                    amount = _decimal(header[2])
                    chunk_crc = _crc(header[3])
                    if not 1 <= amount <= min(MAX_CHUNK, size - received):
                        raise ExportError('Invalid chunk length.')
                    data = self._read_exact(amount)
                    if zlib.crc32(data) & 0xFFFFFFFF != chunk_crc:
                        raise ExportError('CRC32 mismatch: a data chunk was corrupted.')
                    if output.write(data) != amount:
                        raise ExportError('Could not write the full chunk to the computer.')
                    received += amount
                    whole_crc = zlib.crc32(data, whole_crc) & 0xFFFFFFFF
                    self._send('ACK')
                    if progress:
                        progress(received, size)
                footer = self._response()
                if len(footer) != 4 or footer[1] != 'DONE':
                    raise ExportError('Missing transfer-complete message.')
                if _decimal(footer[2]) != received or _crc(footer[3]) != whole_crc:
                    raise ExportError('Final file length or CRC32 did not match.')
                output.flush()
                os.fsync(output.fileno())
            # A hard link publishes a complete file without replacing a file
            # that another process may have created during this transfer.
            try:
                os.link(temporary, destination)
            except FileExistsError:
                raise
            except OSError as exc:
                raise ExportError('Cannot publish the file here. Use a folder on the computer\'s local disk.') from exc
            return received
        except BaseException:
            if requested:
                self.usable = False
                try:
                    self._send('CANCEL', timeout=min(self.timeout, 1.0))
                except Exception:
                    pass
            raise
        finally:
            temporary.unlink(missing_ok=True)


def _arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='USB port, for example /dev/cu.usbmodem101 or COM5')
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--timeout', type=float, default=10.0, help='Protocol timeout in seconds (default: 10)')
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('ports', help='List computer serial ports')
    listing = sub.add_parser('list', help='List non-hidden files on the SD card')
    listing.add_argument('--include-hidden', action='store_true',
                         help='Also list dot-prefixed files and files inside dot-prefixed folders')
    get = sub.add_parser('get', help='Download one file')
    get.add_argument('remote_path', help='Absolute SD path shown by list')
    get.add_argument('--output', type=Path, required=True, help='New local filename')
    pull = sub.add_parser('pull', help='Download non-hidden files while preserving SD folders')
    pull.add_argument('--output', type=Path, default=Path('sd_download'), help='Destination folder (default: sd_download)')
    pull.add_argument('--directory', default='/', help='Only download this SD directory, for example /rawtxt')
    pull.add_argument('--include-hidden', action='store_true',
                      help='Also download dot-prefixed files and files inside dot-prefixed folders')
    args = parser.parse_args(argv)
    if args.command != 'ports' and not args.port:
        parser.error('--port is required; run the ports command to list available ports')
    if args.timeout <= 0 or args.baud <= 0:
        parser.error('--timeout and --baud must be positive')
    return args


def main(argv=None) -> int:
    args = _arguments(argv)
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print('Install the dependency with: python3 -m pip install pyserial', file=sys.stderr)
        return 1
    if args.command == 'ports':
        ports = sorted(list_ports.comports(), key=lambda port: port.device)
        for port in ports:
            print(f'{port.device}\t{port.description}')
        if not ports:
            print('No serial ports found. Connect the XIAO using a USB data cable.')
        return 0
    connection = serial.Serial(port=None, baudrate=args.baud, timeout=0.2,
                               write_timeout=args.timeout, xonxoff=False,
                               rtscts=False, dsrdtr=False)
    opened = False
    try:
        connection.port = args.port
        connection.dtr = True
        connection.rts = False
        if os.name == 'posix':
            connection.exclusive = True
        connection.open()
        opened = True
        time.sleep(1.5)
        connection.reset_input_buffer()
        client = ExportClient(connection, timeout=args.timeout)
        client.ping()
        if args.command == 'list':
            entries = client.list_files()
            if not args.include_hidden:
                entries = [entry for entry in entries if not is_hidden_path(entry.path)]
            for entry in entries:
                print(f'{entry.size:>12} bytes  {entry.path}')
            print(f'{len(entries)} file(s)')
            return 0
        if args.command == 'get':
            jobs = [(args.remote_path, args.output, None)]
        else:
            directory = args.directory.rstrip('/') or '/'
            if directory != '/':
                safe_relative_path(directory)
            entries = client.list_files()
            selected = [entry for entry in entries
                        if (directory == '/' or entry.path.startswith(directory + '/'))
                        and (args.include_hidden or not is_hidden_path(entry.path))]
            jobs = [(entry.path, destination_for(args.output, entry.path), entry.size) for entry in selected]
            if not jobs:
                kind = 'files' if args.include_hidden else 'non-hidden files'
                print(f'No {kind} found in {directory}.')
                return 0
        # Check all output paths before the first transfer.
        for _, destination, _ in jobs:
            if destination.exists() or destination.is_symlink():
                raise FileExistsError(f'Output already exists; choose a new --output folder: {destination}')
        for index, (remote, destination, expected_size) in enumerate(jobs, 1):
            print(f'[{index}/{len(jobs)}] {remote} -> {destination}', flush=True)
            last_update = [0.0]
            def progress(received, total):
                now = time.monotonic()
                if now - last_update[0] >= 0.5 or received == total:
                    print(f'\r  {received:,} / {total:,} bytes', end='', file=sys.stderr, flush=True)
                    last_update[0] = now
            count = client.download(remote, destination, expected_size, progress=progress)
            print(file=sys.stderr)
            print(f'  Saved {count:,} bytes; CRC32 verified.', flush=True)
        return 0
    except KeyboardInterrupt:
        print('\nCancelled. Reset the board before starting a new transfer.', file=sys.stderr)
        return 130
    except (ExportError, OSError, serial.SerialException, ValueError) as exc:
        print(f'Error: {exc}', file=sys.stderr)
        if not opened:
            code = getattr(exc, 'errno', None)
            if code == errno.ENOENT:
                print('That port does not exist. Run: python3 sd_download.py ports', file=sys.stderr)
                print('Use the current port shown there; it may change after uploading.', file=sys.stderr)
            elif code in (errno.EACCES, errno.EPERM, errno.EBUSY, errno.EAGAIN):
                print('Port busy or access denied. Close Serial Monitor/Plotter and other serial programs.', file=sys.stderr)
            else:
                print('Could not open the port. Run the ports command and check the USB connection.', file=sys.stderr)
        elif isinstance(exc, serial.SerialException):
            print('USB communication failed. Check the cable and reset the board before retrying.', file=sys.stderr)
        return 1
    finally:
        connection.close()


if __name__ == '__main__':
    raise SystemExit(main())
