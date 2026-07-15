#!/usr/bin/env python3
import sys
import os
import time

def main():
    """
    Sends a command to the toSpread application via a named pipe and prints the response.
    """
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} \"<command>\"", file=sys.stderr)
        sys.exit(1)

    command = sys.argv[1]
    input_pipe_path = "/tmp/toSpread.in"
    output_pipe_path = "/tmp/toSpread.out"

    # Wait for the pipes to be created
    timeout = 2  # seconds
    start_time = time.time()
    while time.time() - start_time < timeout:
        if os.path.exists(input_pipe_path) and os.path.exists(output_pipe_path):
            break
        time.sleep(0.1)
    else:
        print("Error: Timed out waiting for pipes to be created.", file=sys.stderr)
        if not os.path.exists(input_pipe_path):
            print(f"Input pipe not found at {input_pipe_path}", file=sys.stderr)
        if not os.path.exists(output_pipe_path):
            print(f"Output pipe not found at {output_pipe_path}", file=sys.stderr)
        sys.exit(1)

    try:
        # Open the output pipe for reading first. This will block until the server
        # opens it for writing, which it does on startup.
        with open(output_pipe_path, "r") as output_pipe:
            # Now that the connection is established, open the input pipe for writing.
            with open(input_pipe_path, "w") as input_pipe:
                # Send the command
                input_pipe.write(command + '\n')
                input_pipe.flush()

                # Read the response from the output pipe
                # The server should respond promptly. We can add a timeout here if needed.
                response = output_pipe.readline()
                print(response.strip())
                time.sleep(2) # Shortened sleep
                print("Bye")

    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()

