import cffi
import os

class SpreadClient:
    def __init__(self, library_path='/usr/local/lib/libConnectToSpread.so'):
        print(library_path)
        print(os.path.exists(library_path))

        if not os.path.exists(library_path):
            raise FileNotFoundError(f"Library not found at {library_path}")

        self.ffi = cffi.FFI()
        self._define_c_signatures()
        self.lib = self.ffi.dlopen(library_path)

        # Connect to Spread on initialization
        self.lib.connectToSpread()

    def _define_c_signatures(self):
        self.ffi.cdef("""
            /* From connectToSpread.h */
            void connectToSpread(void);
            void spreadDisconnect(void);
            void spreadJoin(char *);
            void spreadLeave(char *);
            int spreadPoll();

            void toSpread(char *, char *);
            void toSpreadCounted(char *, char *, int);
            int fromSpread(char *, char *);

            void setBoolean(char *, int);
            int getBoolean(char *);

            void redisHset(char *, char *, char *);
            
            /* For handling string returns and buffers */
            char *strsave (const char *);
            void getGroup(char *);
        """)

    def __del__(self):
        # Ensure disconnection on object cleanup
        if hasattr(self, 'lib'):
            self.lib.spreadDisconnect()

    def join_group(self, group_name):
        group_name_c = self.ffi.new("char[]", group_name.encode('utf-8'))
        self.lib.spreadJoin(group_name_c)

    def leave_group(self, group_name):
        group_name_c = self.ffi.new("char[]", group_name.encode('utf-8'))
        self.lib.spreadLeave(group_name_c)

    def poll(self):
        return self.lib.spreadPoll()

    def send_message(self, group_name, message):
        group_name_c = self.ffi.new("char[]", group_name.encode('utf-8'))
        message_c = self.ffi.new("char[]", message.encode('utf-8'))
        self.lib.toSpread(group_name_c, message_c)

    def receive_message(self, max_group_len=128, max_msg_len=4096):
        group_buffer = self.ffi.new(f"char[{max_group_len}]")
        msg_buffer = self.ffi.new(f"char[{max_msg_len}]")
        
        bytes_read = self.lib.fromSpread(group_buffer, msg_buffer)
        
        if bytes_read > 0:
            group = self.ffi.string(group_buffer).decode('utf-8')
            message = self.ffi.string(msg_buffer).decode('utf-8')
            return group, message
        return None, None

    def set_boolean(self, name, value):
        name_c = self.ffi.new("char[]", name.encode('utf-8'))
        self.lib.setBoolean(name_c, int(value))

    def get_boolean(self, name):
        name_c = self.ffi.new("char[]", name.encode('utf-8'))
        return bool(self.lib.getBoolean(name_c))

    def redis_hset(self, key, field, value):
        key_c = self.ffi.new("char[]", key.encode('utf-8'))
        field_c = self.ffi.new("char[]", field.encode('utf-8'))
        value_c = self.ffi.new("char[]", value.encode('utf-8'))
        self.lib.redisHset(key_c, field_c, value_c)

if __name__ == '__main__':
    # Example Usage:
    # 1. Make sure 'libConnectToSpread.so' is in the same directory
    #    or provide the correct path.
    # 2. Ensure the Spread daemon is running.

    try:
        client = SpreadClient()
        print("Successfully connected to Spread.")

        my_group = "test_group"
        client.join_group(my_group)
        print(f"Joined group: {my_group}")

        client.send_message(my_group, "Hello from Python CFFI!")
        print("Sent a message.")

        print("Polling for messages...")
        while True:
            group, message = client.receive_message()
            if group:
                print(f"Received message from group '{group}': {message}")
                break
            # Add a small delay to prevent busy-waiting
            import time
            time.sleep(0.1)

        client.leave_group(my_group)
        print(f"Left group: {my_group}")

    except FileNotFoundError as e:
        print(f"Error: {e}")
        print("Please ensure 'libConnectToSpread.so' is compiled and accessible.")
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
