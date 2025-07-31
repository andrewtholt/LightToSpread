#!/usr/bin/env python3 
import os
import pty
import subprocess
import time
import select

def interact_with_tospread_no_initial_prompt():
    print("Starting interactive toSpread session via PTY from Python (no initial prompt expected)...")

    # --- Configuration for toSpread ---
    # !!! IMPORTANT: Replace this with the actual command to launch toSpread !!!
    TOSPREAD_COMMAND = ['./toSpread']
    
    # !!! IMPORTANT: Define the prompt that toSpread displays when it's ready for input !!!
    # Example: If toSpread shows "CMD> " or "tospread ready>"
    # Add more patterns if toSpread uses different prompts.
    TOSPREAD_PROMPT_PATTERNS = ['>', ': ', 'ready>', '$', '#'] # Added common shell prompts too

    master_fd, slave_fd = pty.openpty()

    process = None # Initialize process to None
    try:
        # Start the toSpread process, connecting its stdin/stdout/stderr to the slave PTY
        process = subprocess.Popen(
            TOSPREAD_COMMAND,
            stdin=slave_fd,
            stdout=slave_fd,
            stderr=slave_fd,
            close_fds=True # Close file descriptors inherited from parent
        )

        # Close the slave FD in the parent process immediately
        os.close(slave_fd)

        # Give toSpread a very brief moment to ensure the process is running
        time.sleep(0.1) # Small delay, not waiting for output, just for process to stabilize

        # --- Example Command 1: Get version (replace with actual toSpread command) ---
        # We send the first command BEFORE trying to read any output/prompt
        command1 = "^version\n" # Assuming 'version' is a valid command for toSpread
        print(f"Sending first command: {command1.strip()}")
        os.write(master_fd, command1.encode())

        # Now, read output until a new prompt appears or a timeout occurs
        # This will capture the output of the 'version' command AND the subsequent prompt
        output1 = read_until_prompt_or_timeout(master_fd, timeout=2, prompt_patterns=TOSPREAD_PROMPT_PATTERNS)
        print(f"\n--- Output of '{command1.strip()}' (via PTY) ---\n{output1.strip()}\n------------------------------------------------\n")

        # --- Example Command 2: Load data (replace with actual toSpread command) ---
        command2 = "^dump\n" # Assuming 'load' command for a file
        print(f"Sending command: {command2.strip()}")
        os.write(master_fd, command2.encode())

        output2 = read_until_prompt_or_timeout(master_fd, timeout=3, prompt_patterns=TOSPREAD_PROMPT_PATTERNS) # Give more time for file operations
        print(f"\n--- Output of '{command2.strip()}' (via PTY) ---\n{output2.strip()}\n------------------------------------------------\n")

        # --- Example Command 3: Perform a calculation (replace with actual toSpread command) ---
        command3 = "^get CONNECTED\n"
        print(f"Sending command: {command3.strip()}")
        os.write(master_fd, command3.encode())

        output3 = read_until_prompt_or_timeout(master_fd, timeout=2, prompt_patterns=TOSPREAD_PROMPT_PATTERNS)
        print(f"\n--- Output of '{command3.strip()}' (via PTY) ---\n{output3.strip()}\n------------------------------------------------\n")

        # --- Exit the toSpread session ---
        # !!! IMPORTANT: Replace 'quit' with the actual exit command for toSpread if different !!!
        exit_command = "^exit\n"
        print(f"Sending command: {exit_command.strip()}")
        os.write(master_fd, exit_command.encode())

        # Wait for the process to terminate
        process.wait()
        print(f"\ntoSpread session exited with return code: {process.returncode}")

    except Exception as e:
        print(f"An error occurred: {e}")
    finally:
        # Ensure all file descriptors are closed
        if 'master_fd' in locals() and master_fd is not None:
            os.close(master_fd)
        if process and process.poll() is None: # If process is still running
            process.terminate()
            print("toSpread process terminated.")

def read_until_prompt_or_timeout(fd, timeout=1, prompt_patterns=[]):
    """
    Reads from a file descriptor until a defined prompt is detected
    or a timeout occurs.
    """
    output = b""
    start_time = time.time()
    last_line_decoded = ""

    while True:
        # Use select to wait for data with a short timeout
        rlist, _, _ = select.select([fd], [], [], 0.1) # Check every 0.1 seconds
        if rlist:
            try:
                data = os.read(fd, 4096) # Read up to 4KB
                if not data: # EOF - process might have exited
                    break
                output += data
                
                # Decode only the last part to check for prompt, avoid decoding large outputs repeatedly
                try:
                    full_decoded_output = output.decode(errors='ignore')
                    # Get the last non-empty line
                    lines = [line.strip() for line in full_decoded_output.split('\n') if line.strip()]
                    if lines:
                        last_line_decoded = lines[-1]
                    else:
                        last_line_decoded = "" # No meaningful lines yet
                except UnicodeDecodeError:
                    # If decoding fails, continue, might be binary data or mid-character
                    pass

                # Check for prompt patterns
                for pattern in prompt_patterns:
                    # Check if the prompt is at the very end of the last line
                    if last_line_decoded.endswith(pattern):
                        return output.decode(errors='ignore')
                    # Or if the prompt is the entire last line (common for simple prompts like "$")
                    if last_line_decoded == pattern:
                        return output.decode(errors='ignore')

            except OSError as e:
                if e.errno == 5: # Input/output error (e.g., FD closed)
                    break
                raise
        elif time.time() - start_time > timeout:
            break # Timeout reached

    return output.decode(errors='ignore') # Return whatever was read within the timeout

if __name__ == "__main__":
    interact_with_tospread_no_initial_prompt()
