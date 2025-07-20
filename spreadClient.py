#!/usr/bin/env python3
import socket
import sys
import threading
import select

def read_from_socket(sock):
    """Read data from socket and print to stdout"""
    try:
        while True:
            data = sock.recv(1024)
            if not data:
                break
            sys.stdout.write(data.decode('utf-8'))
            sys.stdout.flush()
    except Exception as e:
        print(f"Error reading from socket: {e}", file=sys.stderr)
    finally:
        sock.close()

def main():
    if len(sys.argv) != 3:
        print("Usage: python3 socket_client.py <host> <port>", file=sys.stderr)
        sys.exit(1)
    
    host = sys.argv[1]
    try:
        port = int(sys.argv[2])
    except ValueError:
        print("Error: Port must be a number", file=sys.stderr)
        sys.exit(1)
    
    # Create socket
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((host, port))
        print(f"Connected to {host}:{port}", file=sys.stderr)
    except Exception as e:
        print(f"Error connecting to {host}:{port}: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Start thread to read from socket
    reader_thread = threading.Thread(target=read_from_socket, args=(sock,), daemon=True)
    reader_thread.start()
    
    # Read from stdin and send to socket
    try:
        while True:
            # Use select to check if stdin has data (non-blocking on Unix)
            if select.select([sys.stdin], [], [], 0)[0]:
                line = sys.stdin.readline()
                if not line:  # EOF
                    break
                sock.send(line.encode('utf-8'))
            else:
                # Small sleep to prevent busy waiting
                import time
                time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nDisconnecting...", file=sys.stderr)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
    finally:
        sock.close()

if __name__ == "__main__":
    main()
