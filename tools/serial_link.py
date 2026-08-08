"""
LightGuide Edge - shared serial plumbing
COM683 CW2 | Vishnu Vekariya | Ulster University

Both operator-driven tools (online_trial.py, calibrate.py) open the port once and
then wait on the operator for minutes at a time. That combination has one sharp
edge, found the hard way on 8 August, and it lives here so neither tool can grow
its own half-correct version of it.

THE FAILURE
-----------
The sketches free-run their CSV at ~10 Hz whether or not the host is listening.
If the port is open and nothing reads it, the OS receive buffer fills - about
98 KB, roughly four minutes - and back-pressure reaches the device. The sketch
then blocks inside Serial.print, and a blocked sketch services nothing: not the
D7 button, not incoming commands. The next write from the host fails with

    WriteFile failed (PermissionError(13, 'The device does not recognize the
    command.', None, 22))

which reads like a dead board or a bad cable and is neither. The tell is that the
board also stops responding to its own buttons.

So anything that holds the port across an operator prompt must keep draining it.
"""

from __future__ import annotations

import threading
import time

import serial

BAUD = 115200


def open_port(port: str, baud: int = BAUD) -> serial.Serial:
    ser = serial.Serial(port, baud, timeout=2)
    ser.dtr = True          # the Nano's native USB needs DTR asserted (RUN.md)
    time.sleep(2.0)         # the board resets when the port opens
    ser.reset_input_buffer()
    return ser


class Drainer:
    """Keeps the receive buffer empty while we are waiting on the operator.

    Reads and discards on a daemon thread, and parks on request so the caller
    can own the port for a write or a timed read without racing it for bytes.
    """

    def __init__(self, ser: serial.Serial) -> None:
        self.ser = ser
        self._draining = threading.Event()
        self._parked = threading.Event()
        self._stop = threading.Event()
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def _loop(self) -> None:
        while not self._stop.is_set():
            if not self._draining.is_set():
                self._parked.set()
                time.sleep(0.05)
                continue
            self._parked.clear()
            try:
                waiting = self.ser.in_waiting
                if waiting:
                    self.ser.read(waiting)
                else:
                    time.sleep(0.05)
            except Exception:
                # The port is going away, or the main thread is mid-teardown.
                # Draining is best-effort; the caller reports real errors.
                time.sleep(0.1)

    def resume(self) -> None:
        self._parked.clear()
        self._draining.set()

    def pause(self) -> None:
        """Stop draining and wait until the thread is genuinely idle, so the
        caller has the port to itself."""
        self._draining.clear()
        self._parked.wait(timeout=1.0)

    def stop(self) -> None:
        self._stop.set()
        self._draining.clear()
        self._thread.join(timeout=1.0)

    # Used around the operator prompts: drain while they are working, park the
    # moment they hand control back.
    def prompt(self, message: str) -> str:
        self.resume()
        try:
            return input(message)
        finally:
            self.pause()
