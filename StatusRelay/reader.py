import os
import time

pipe_path = "/tmp/mydata_pipe"

# Open and read from pipe
fd = os.open(pipe_path, os.O_RDONLY)

try:
    while True:
        data = os.read(fd, 1024)
        if data:
            print("Received:", data.decode())
        else:
            print("\n")
            time.sleep(1)
            
except KeyboardInterrupt:
    print("Stopping...")
finally:
    os.close(fd)