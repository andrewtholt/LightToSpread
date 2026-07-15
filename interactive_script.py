#!/usr/bin/env python3

# interactive_script.py
import sys
import time

print("Interactive script started. Type 'quit' to exit.")
sys.stdout.flush() # Ensure this initial message is sent immediately

while True:
    sys.stdout.write("Enter something: ")
    sys.stdout.flush() # Ensure the prompt is shown

    line = sys.stdin.readline().strip()

    if not line: # EOF or empty line on immediate close
        print("\nEOF received. Exiting.")
        break
    elif line.lower() == 'quit':
        print("Quitting as requested.")
        break
    else:
        print(f"You entered: {line}")
        # Simulate some processing time
        time.sleep(0.1)
    sys.stdout.flush() # Ensure output is sent immediately
