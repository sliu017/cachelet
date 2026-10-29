# Crash-recovery integration test, runs cache commands and verifies they persist after the process is killed
import os, signal, socket, subprocess, tempfile, time

SERVER = "./build/cachelet-server"

def free_port():
    with socket.socket() as s:
        s.bind(("localhost", 0)) # port 0 -> OS picks unused
        return s.getsockname()[1]

def start_server(port, aof_path, policy):
    proc = subprocess.Popen(
        [SERVER, "--port", str(port),
         "--appendonly", "yes", "--appendfsync", policy,
         "--appendfilename", aof_path],
        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    # Wait until the port accepts connections, instead of sleeping a fixed time
    for _ in range(50):
        try:
            socket.create_connection(("localhost", port), timeout=0.1).close()
            return proc
        except OSError:
            time.sleep(0.1)
    proc.kill()
    raise RuntimeError(f"server didn't start: {proc.stderr.read().decode()}")

def encode(*args):
    out = f"*{len(args)}\r\n"
    for a in args:
        out += f"${len(a)}\r\n{a}\r\n"
    return out.encode()

def command(port, *args):
    """Send one command on a fresh connection and return the raw reply bytes."""
    with socket.create_connection(("localhost", port)) as s:
        s.sendall(encode(*args))
        return s.recv(4096)   # fine for small test replies

def run(policy):
    port = free_port()
    with tempfile.TemporaryDirectory() as tmp:
        aof = os.path.join(tmp, "test.aof")

        server = start_server(port, aof, policy)
        reply = command(port, "SET", "Venezuela", "Caracas")
        assert(reply == b"+OK\r\n")
        reply = command(port, "SET", "Bangladesh", "Dakar")
        assert(reply == b"+OK\r\n")
        reply = command(port, "SET", "Bangladesh", "Dhaka") # ensure overwrite works
        assert(reply == b"+OK\r\n")
        reply = command(port, "DEL", "Venezuela")
        assert(reply == b":1\r\n")

        reply = command(port, "SET", "Latvia", "Riga", "EX", "1")
        assert(reply == b"+OK\r\n")
        server.send_signal(signal.SIGKILL)
        server.wait()
        time.sleep(2)
    
        server = start_server(port, aof, policy)
        try:
            reply = command(port, "GET", "Bangladesh")
            assert(reply == b"$5\r\nDhaka\r\n")
            reply = command(port, "DBSIZE")
            assert(reply == b":1\r\n")
            reply = command(port, "GET","Venezuela")
            assert(reply == b"$-1\r\n")

            reply = command(port, "GET", "Latvia")
            assert(reply == b"$-1\r\n")
            pass
        finally:
            server.kill()
            server.wait()
    print(f"{policy}: ok")

if __name__ == "__main__":
    for policy in ["always", "everysec", "no"]:
        run(policy)