#!/usr/bin/env python3
"""
Python CFFI interface for libConnectToSpread.so library
Example usage of Spread group communication system
"""

import cffi
import os
import sys
from contextlib import contextmanager

class SpreadClient:
    def __init__(self, library_path="/usr/local/lib/libConnectToSpread.so"):
        """Initialize CFFI interface to libConnectToSpread"""
        self.ffi = cffi.FFI()
        
        # Define the C interface based on the header file
        self.ffi.cdef("""
            // Constants
            #define SINK 1
            #define SOURCE 2
            
            // Forward declare the global structure (we don't know its contents)
            typedef struct globalDefinitions globalDefinitions;
            
            // Structure definitions
            struct spreadServerStatus {
                char server[64];
                int status;  // -1 unknown, 0 OK, 1 None fail
            };
            
            // External variables - declare but may not be accessible
            extern struct spreadServerStatus servers[5];
            extern struct globalDefinitions global;
            
            // Function declarations
            void dumpSymbols(void);
            void saveSymbols(void);
            
            int cmdInterp(int, char *);
            char *strsave(const char *);
            void getGroup(char *);
            
            int fromSpread(char *, char *);
            void toSpread(char *, char *);
            void toSpreadCounted(char *, char *, int);
            
            void setBoolean(char *, int);
            int getBoolean(char *);
            
            void toOut(char *);
            void toError(char *);
            void fromIn(char *);
            
            void dumpGlobals(void);
            void printDebug(char *);
            
            void connectToSpread(void);
            void spreadJoin(char *);
            void spreadLeave(char *);
            void spreadDisconnect(void);
            int spreadPoll(void);
            
            void redisHset(char *, char *, char *);
            
            // Memory management
            void free(void *);
        """)
        
        # Try multiple ways to load the library
        self.lib = None
        library_paths = [
            library_path,
            "libConnectToSpread.so",
            "./libConnectToSpread.so",
            "ConnectToSpread"  # Try without lib prefix and .so suffix
        ]
        
        for path in library_paths:
            try:
                print(f"Trying to load: {path}")
                self.lib = self.ffi.dlopen(path)
                print(f"Successfully loaded library from: {path}")
                break
            except OSError as e:
                print(f"Failed to load {path}: {e}")
                continue
        
        if self.lib is None:
            print("ERROR: Could not load libConnectToSpread.so from any location")
            print("\nTroubleshooting steps:")
            print("1. Check if the library exists:")
            print("   ls -la /usr/local/lib/libConnectToSpread.so")
            print("2. Check library dependencies:")
            print("   ldd /usr/local/lib/libConnectToSpread.so")
            print("3. Check for undefined symbols:")
            print("   nm -D /usr/local/lib/libConnectToSpread.so | grep -E '(global|UNDEF)'")
            print("4. Make sure library path is in LD_LIBRARY_PATH:")
            print("   export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH")
            sys.exit(1)
    
    def connect(self):
        """Connect to Spread daemon"""
        try:
            self.lib.connectToSpread()
            print("Connected to Spread")
        except Exception as e:
            print(f"Connection failed: {e}")
            raise
    
    def disconnect(self):
        """Disconnect from Spread daemon"""
        try:
            self.lib.spreadDisconnect()
            print("Disconnected from Spread")
        except Exception as e:
            print(f"Disconnect failed: {e}")
    
    def join_group(self, group_name):
        """Join a Spread group"""
        try:
            group_bytes = group_name.encode('utf-8')
            self.lib.spreadJoin(group_bytes)
            print(f"Joined group: {group_name}")
        except Exception as e:
            print(f"Failed to join group {group_name}: {e}")
    
    def leave_group(self, group_name):
        """Leave a Spread group"""
        try:
            group_bytes = group_name.encode('utf-8')
            self.lib.spreadLeave(group_bytes)
            print(f"Left group: {group_name}")
        except Exception as e:
            print(f"Failed to leave group {group_name}: {e}")
    
    def send_message(self, group_name, message):
        """Send a message to a Spread group"""
        try:
            group_bytes = group_name.encode('utf-8')
            message_bytes = message.encode('utf-8')
            self.lib.toSpread(group_bytes, message_bytes)
            print(f"Sent message to {group_name}: {message}")
        except Exception as e:
            print(f"Failed to send message: {e}")
    
    def send_counted_message(self, group_name, data, length):
        """Send binary/counted message to a Spread group"""
        try:
            group_bytes = group_name.encode('utf-8')
            if isinstance(data, str):
                data_bytes = data.encode('utf-8')
            else:
                data_bytes = data
            self.lib.toSpreadCounted(group_bytes, data_bytes, length)
            print(f"Sent counted message to {group_name} (length: {length})")
        except Exception as e:
            print(f"Failed to send counted message: {e}")
    
    def receive_message(self):
        """Receive a message from Spread"""
        try:
            # Allocate buffers for group and message
            group_buffer = self.ffi.new("char[256]")
            message_buffer = self.ffi.new("char[1024]")
            
            result = self.lib.fromSpread(group_buffer, message_buffer)
            
            if result > 0:
                group = self.ffi.string(group_buffer).decode('utf-8')
                message = self.ffi.string(message_buffer).decode('utf-8')
                print(f"Received from {group}: {message}")
                return group, message
            else:
                return None, None
        except Exception as e:
            print(f"Failed to receive message: {e}")
            return None, None
    
    def poll(self):
        """Poll for incoming messages"""
        try:
            return self.lib.spreadPoll()
        except Exception as e:
            print(f"Poll failed: {e}")
            return -1
    
    def set_boolean_var(self, var_name, value):
        """Set a boolean variable"""
        try:
            var_bytes = var_name.encode('utf-8')
            self.lib.setBoolean(var_bytes, int(bool(value)))
            print(f"Set {var_name} = {bool(value)}")
        except Exception as e:
            print(f"Failed to set boolean {var_name}: {e}")
    
    def get_boolean_var(self, var_name):
        """Get a boolean variable"""
        try:
            var_bytes = var_name.encode('utf-8')
            result = self.lib.getBoolean(var_bytes)
            print(f"Got {var_name} = {bool(result)}")
            return bool(result)
        except Exception as e:
            print(f"Failed to get boolean {var_name}: {e}")
            return False
    
    def debug_output(self, message):
        """Send debug output"""
        try:
            message_bytes = message.encode('utf-8')
            self.lib.printDebug(message_bytes)
        except Exception as e:
            print(f"Debug output failed: {e}")
    
    def output_message(self, message):
        """Send message to output"""
        try:
            message_bytes = message.encode('utf-8')
            self.lib.toOut(message_bytes)
        except Exception as e:
            print(f"Output failed: {e}")
    
    def error_message(self, message):
        """Send error message"""
        try:
            message_bytes = message.encode('utf-8')
            self.lib.toError(message_bytes)
        except Exception as e:
            print(f"Error output failed: {e}")
    
    def dump_globals(self):
        """Dump global variables for debugging"""
        try:
            self.lib.dumpGlobals()
        except Exception as e:
            print(f"Dump globals failed: {e}")
    
    def get_server_status(self):
        """Get status of Spread servers"""
        try:
            servers = []
            # Try to access the servers array - this might fail if undefined
            for i in range(5):
                server = self.lib.servers[i]
                server_name = self.ffi.string(server.server).decode('utf-8')
                status = server.status
                status_text = {-1: "unknown", 0: "OK", 1: "failed"}.get(status, "invalid")
                servers.append({
                    'name': server_name,
                    'status': status,
                    'status_text': status_text
                })
            return servers
        except Exception as e:
            print(f"Failed to get server status (external variable may not be accessible): {e}")
            return []
    
    def redis_hset(self, key, field, value):
        """Set Redis hash field"""
        try:
            key_bytes = key.encode('utf-8')
            field_bytes = field.encode('utf-8')
            value_bytes = value.encode('utf-8')
            self.lib.redisHset(key_bytes, field_bytes, value_bytes)
            print(f"Redis HSET {key} {field} {value}")
        except Exception as e:
            print(f"Redis HSET failed: {e}")

@contextmanager
def spread_client(library_path="/usr/local/lib/libConnectToSpread.so"):
    """Context manager for SpreadClient"""
    client = SpreadClient(library_path)
    try:
        yield client
    finally:
        try:
            client.disconnect()
        except:
            pass

def check_library_dependencies(library_path="/usr/local/lib/libConnectToSpread.so"):
    """Helper function to diagnose library issues"""
    print(f"\n=== Library Diagnostics for {library_path} ===")
    
    # Check if file exists
    if os.path.exists(library_path):
        print(f"✓ Library file exists: {library_path}")
        
        # Get file info
        stat = os.stat(library_path)
        print(f"  Size: {stat.st_size} bytes")
        print(f"  Permissions: {oct(stat.st_mode)[-3:]}")
    else:
        print(f"✗ Library file not found: {library_path}")
        return False
    
    # Try to run ldd to check dependencies
    try:
        import subprocess
        result = subprocess.run(['ldd', library_path], capture_output=True, text=True)
        if result.returncode == 0:
            print("\n Dependencies (ldd output):")
            for line in result.stdout.strip().split('\n'):
                if 'not found' in line:
                    print(f"  ✗ {line}")
                else:
                    print(f"  ✓ {line}")
        else:
            print(f"✗ ldd failed: {result.stderr}")
    except Exception as e:
        print(f"Could not run ldd: {e}")
    
    # Try to check for undefined symbols
    try:
        result = subprocess.run(['nm', '-D', library_path], capture_output=True, text=True)
        if result.returncode == 0:
            undefined_symbols = []
            for line in result.stdout.strip().split('\n'):
                if ' U ' in line:  # Undefined symbol
                    symbol = line.split()[-1]
                    undefined_symbols.append(symbol)
            
            if undefined_symbols:
                print(f"\n Undefined symbols (need to be provided by your program):")
                for symbol in undefined_symbols[:10]:  # Show first 10
                    print(f"  - {symbol}")
                if len(undefined_symbols) > 10:
                    print(f"  ... and {len(undefined_symbols) - 10} more")
            else:
                print("\n✓ No undefined symbols found")
        else:
            print(f"Could not run nm: {result.stderr}")
    except Exception as e:
        print(f"Could not run nm: {e}")
    
    return True

def main():
    """Example usage of the SpreadClient"""
    
    # First, run diagnostics
    library_path = "/usr/local/lib/libConnectToSpread.so"
    
    # Check if we should run diagnostics
    if len(sys.argv) > 1 and sys.argv[1] == "--diagnose":
        check_library_dependencies(library_path)
        return
    
    print("=== Basic Spread Client Example ===")
    print("(Run with --diagnose flag to check library dependencies)")
    
    # Example 1: Basic connection and messaging
    try:
        with spread_client(library_path) as client:
            print("Library loaded successfully!")
            
            # Test basic functions that shouldn't depend on external symbols
            print("\nTesting basic functions...")
            
            # Test string operations
            try:
                client.debug_output("Testing debug output")
                client.output_message("Testing standard output")
                client.error_message("Testing error output")
                print("✓ Output functions work")
            except Exception as e:
                print(f"✗ Output functions failed: {e}")
            
            # Test boolean operations
            try:
                client.set_boolean_var("test_var", True)
                result = client.get_boolean_var("test_var")
                print(f"✓ Boolean operations work (set True, got {result})")
            except Exception as e:
                print(f"✗ Boolean operations failed: {e}")
            
            # Test Spread connection (this might fail if Spread daemon isn't running)
            try:
                client.connect()
                print("✓ Connected to Spread daemon")
                
                # Test group operations
                client.join_group("test_group")
                print("✓ Joined test group")
                
                # Test messaging
                client.send_message("test_group", "Hello, Spread!")
                print("✓ Sent message")
                
                # Test polling
                poll_result = client.poll()
                print(f"✓ Poll result: {poll_result}")
                
                # Try to receive (might not have messages)
                group, message = client.receive_message()
                if group and message:
                    print(f"✓ Received: [{group}] {message}")
                else:
                    print("ℹ No messages received (normal if no other clients)")
                
                client.leave_group("test_group")
                print("✓ Left test group")
                
            except Exception as e:
                print(f"✗ Spread operations failed: {e}")
                print("  This might be normal if Spread daemon is not running")
            
            # Test Redis operations (might fail if Redis not configured)
            try:
                client.redis_hset("test:key", "field", "value")
                print("✓ Redis operation completed")
            except Exception as e:
                print(f"✗ Redis operation failed: {e}")
                print("  This might be normal if Redis is not configured")
            
            # Test server status (might fail due to undefined symbols)
            try:
                servers = client.get_server_status()
                if servers:
                    print("✓ Server status retrieved")
                    for server in servers:
                        if server['name']:
                            print(f"  {server['name']}: {server['status_text']}")
                else:
                    print("ℹ No server status available")
            except Exception as e:
                print(f"✗ Server status failed: {e}")
            
            # Test dump functions
            try:
                client.dump_globals()
                print("✓ Dump globals completed")
            except Exception as e:
                print(f"✗ Dump globals failed: {e}")
            
    except Exception as e:
        print(f"Error in main: {e}")
        print("\nIf you're getting library loading errors, try:")
        print("1. Run with --diagnose flag to check dependencies")
        print("2. Make sure libConnectToSpread.so is properly compiled")
        print("3. Check that all required symbols are defined")
        print("4. Ensure Spread daemon is running if testing Spread functions")

    print("\n=== Advanced Example: Message Loop ===")
    
    # Example 2: Message processing loop
    def message_processor():
        """Example message processing function"""
        with spread_client() as client:
            client.connect()
            client.join_group("processing_group")
            
            print("Starting message processing loop (press Ctrl+C to stop)")
            try:
                while True:
                    # Poll for messages
                    if client.poll() > 0:
                        group, message = client.receive_message()
                        if group and message:
                            # Process the message
                            response = f"Processed: {message}"
                            client.send_message(f"{group}_response", response)
                    
                    # Small delay to prevent busy waiting
                    import time
                    time.sleep(0.01)
                    
            except KeyboardInterrupt:
                print("\nMessage processing stopped")
                client.leave_group("processing_group")
    
    # Uncomment to run message processor
    # message_processor()

if __name__ == "__main__":
    main()
