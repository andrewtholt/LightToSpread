#!/usr/bin/env python3

import os
import sys
import pty
import select
import termios
import tty

def run_interactive_program(program="/bin/bash"):
    """
    Simple function to run an interactive program under PTY control
    """
    # Save original terminal settings
    old_settings = termios.tcgetattr(sys.stdin)
    
    try:
        # Fork with PTY
        pid, master_fd = pty.fork()
        
        if pid == 0:
            # Child process - run the target program
            os.execvp(program, [program])
        else:
            # Parent process - handle I/O
            print(f"Running {program} with PID {pid}")
            
            # Set terminal to raw mode
            tty.setraw(sys.stdin)
            
            try:
                while True:
                    # Wait for input from either stdin or the child process
                    ready, _, _ = select.select([sys.stdin, master_fd], [], [])
                    
                    if sys.stdin in ready:
                        # Forward user input to child
                        data = os.read(sys.stdin.fileno(), 1024)
                        if data:
                            os.write(master_fd, data)
                    
                    if master_fd in ready:
                        # Forward child output to user
                        try:
                            data = os.read(master_fd, 1024)
                            if data:
                                os.write(sys.stdout.fileno(), data)
                            else:
                                break  # Child closed connection
                        except OSError:
                            break  # Child process died
                            
            except KeyboardInterrupt:
                print("\nExiting...")
            
            # Wait for child to exit
            os.waitpid(pid, 0)
            os.close(master_fd)
            
    finally:
        # Restore terminal settings
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)

if __name__ == "__main__":
    program = sys.argv[1] if len(sys.argv) > 1 else "/bin/bash"
    run_interactive_program(program)
