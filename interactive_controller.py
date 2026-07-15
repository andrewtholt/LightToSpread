#!/usr/bin/env python3

import subprocess
import threading
import queue
import time
import sys

# Define the command to run our interactive script
command = ['./toSpread']

# Queues for inter-thread communication
stdout_queue = queue.Queue()
stderr_queue = queue.Queue()

# Event to signal reader threads to stop
stop_event = threading.Event()

def enqueue_output(stream, q):
    """
    Function to read from a stream and put lines/prompts into a queue.
    Looks for the 'toSpread>' prompt.
    """
    buffer = b''
    prompt = b'toSpread>'
    for char_byte in iter(lambda: stream.read(1), b''):
        if stop_event.is_set():
            break
        buffer += char_byte
        if char_byte == b'\n':
            q.put(buffer)
            buffer = b''
        elif buffer.endswith(prompt):
            q.put(buffer)
            buffer = b''
    if buffer:
        q.put(buffer)
    stream.close()

def callClient(process, data_to_send):
    """
    Sends a string to the subprocess and returns the response.
    If a prompt is not received, it sends a newline to trigger it.
    """
    response = ""
    if process.stdin.writable():
        process.stdin.write((data_to_send + '\n').encode('utf-8', errors='replace'))
        process.stdin.flush()
    else:
        return "Error: Subprocess stdin is not writable."

    # Read the response until the next prompt
    prompt = b'toSpread>'
    while True:
        try:
            line_bytes = stdout_queue.get(timeout=10).strip()
            line_str = line_bytes.decode('utf-8', errors='replace')
            
            if line_bytes.endswith(prompt):
                # Add the line to the response, but without the prompt itself
                response += line_str[:-len(prompt)]
                break
            else:
                response += line_str + "\n"
        except queue.Empty:
            # The process might be waiting for a newline to show the prompt
            print("\nTimeout waiting for prompt. Sending newline to trigger it.", file=sys.stderr)
            if process.stdin.writable():
                process.stdin.write(b'\n')
                process.stdin.flush()
            else:
                response += "\nError: Subprocess stdin is not writable."
                break
            
            # Try reading one more time after sending newline
            try:
                line_bytes = stdout_queue.get(timeout=4).strip()
                line_str = line_bytes.decode('utf-8', errors='replace')
                if line_bytes.endswith(prompt):
                    response += line_str[:-len(prompt)]
                else:
                    response += line_str
                break
            except queue.Empty:
                response += "\nSubprocess did not respond with a prompt after newline."
                break

    return response.strip()

def main():
    process = None
    try:
        # Start the subprocess
        process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            bufsize=0
        )
        print(f"Subprocess '{' '.join(command)}' started with PID: {process.pid}")

        # Start reader threads
        stdout_thread = threading.Thread(target=enqueue_output, args=(process.stdout, stdout_queue))
        stderr_thread = threading.Thread(target=enqueue_output, args=(process.stderr, stderr_queue))
        stdout_thread.daemon = True
        stderr_thread.daemon = True
        stdout_thread.start()
        stderr_thread.start()

        # Per user instruction: let it time out, then send \n
        try:
            # Try to read initial output, expecting a timeout
            stdout_queue.get(timeout=4)
        except queue.Empty:
            if process.stdin.writable():
                process.stdin.write(b'\n')
                process.stdin.flush()

        # Read until we see the prompt
        initial_response = ""
        try:
            while True:
                line_bytes = stdout_queue.get(timeout=4)
                line_str = line_bytes.decode('utf-8', 'replace')
                initial_response += line_str
                if line_bytes.endswith(b'toSpread>'):
                    break
        except queue.Empty:
            print("Did not get 'toSpread>' prompt.")
            return

        print(initial_response, end='')
        
        # Main loop for interaction
        while True:
            try:
                user_input = input()
            except EOFError:
                print("\nEOF received, exiting.")
                break

            if user_input.lower() == 'quit':
                print("Exiting.")
                break

            response = callClient(process, user_input)
            print(response)
            print("toSpread>", end='', flush=True)


    except FileNotFoundError:
        print(f"Error: Command '{command[0]}' not found.")
    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        # Final cleanup
        if process and process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                process.kill()
        
        stop_event.set()
        if 'stdout_thread' in locals() and stdout_thread.is_alive():
            stdout_thread.join(timeout=2)
        if 'stderr_thread' in locals() and stderr_thread.is_alive():
            stderr_thread.join(timeout=2)

        print("\nInteractive session ended.")

if __name__ == "__main__":
    main()
