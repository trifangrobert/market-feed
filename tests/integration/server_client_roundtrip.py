#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import time

SOCK = "/tmp/demo.sock"

PASS_MARKERS = [
    "client: connected to server",
    "NEW ACK:",
    "CANCEL ACK:",
    "client: workflow completed successfully",
]

def wait_for_socket(path: str, timeout_s: float = 5.0) -> bool:
    t0 = time.time()
    while time.time() - t0 < timeout_s:
        if os.path.exists(path):
            return True
        time.sleep(0.05)
    return False

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--server", required=True)
    ap.add_argument("--client", required=True)
    args = ap.parse_args()

    # Ensure stale socket is gone
    try:
        os.unlink(SOCK)
    except FileNotFoundError:
        pass

    # Start server
    srv = subprocess.Popen([args.server], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)

    # Wait for socket to appear
    if not wait_for_socket(SOCK, 5.0):
        srv_out = srv.communicate(timeout=1)[0] if srv.poll() is not None else "<server running, no output>"
        print("[FAIL] server did not create socket in time")
        print("--- server output ---\n" + srv_out)
        srv.terminate()
        try:
            srv.wait(timeout=2)
        except subprocess.TimeoutExpired:
            srv.kill()
        return 1

    # Run client
    cli = subprocess.run([args.client], capture_output=True, text=True)

    # Stop server (give it a moment to flush)
    try:
        srv.wait(timeout=2)
    except subprocess.TimeoutExpired:
        srv.terminate()
        try:
            srv.wait(timeout=2)
        except subprocess.TimeoutExpired:
            srv.kill()

    server_output = srv.stdout.read() if srv.stdout else ""

    ok = (cli.returncode == 0)
    for marker in PASS_MARKERS:
        if marker not in cli.stdout:
            ok = False
            print(f"[FAIL] missing marker in client output: {marker}")

    print("--- client stdout ---\n" + cli.stdout)
    print("--- client stderr ---\n" + cli.stderr)
    print("--- server output ---\n" + server_output)

    if not ok:
        print("[FAIL] integration roundtrip failed")
        return 1

    print("[OK] integration roundtrip passed")
    return 0

if __name__ == "__main__":
    sys.exit(main())