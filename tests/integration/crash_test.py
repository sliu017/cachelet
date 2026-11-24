# Crash-recovery integration test, runs cache commands and verifies they persist after the process is killed
import os, signal, socket, subprocess, tempfile, time

SERVER = "./build/cachelet-server"

FSYNC_POLICIES = ["always", "everysec", "no"]
EVICTION_POLICIES = ["lru", "fifo", "sieve", "s3fifov0", "approx-lru"]

# large enough that run() and run_compaction() never evict, and s3fifo gets a sane small queue
ROOMY = 1000


def free_port():
    with socket.socket() as s:
        s.bind(("localhost", 0)) # port 0 -> OS picks unused
        return s.getsockname()[1]

def start_server(port, aof_path, fsync_policy, eviction_policy="lru", extra_args=()):
    proc = subprocess.Popen(
        [SERVER, "--port", str(port),
         "--appendonly", "yes", "--appendfsync", fsync_policy,
         "--appendfilename", aof_path,
         "--policy", eviction_policy, *extra_args],
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


def run(fsync_policy, eviction_policy):
    # basic operations, no eviction conducted
    port = free_port()
    flags = ("--maxentries", str(ROOMY))
    with tempfile.TemporaryDirectory() as tmp:
        aof = os.path.join(tmp, "test.aof")

        server = start_server(port, aof, fsync_policy, eviction_policy, flags)
        try:
            assert command(port, "SET", "Venezuela", "Caracas") == b"+OK\r\n"
            assert command(port, "SET", "Bangladesh", "Dakar") == b"+OK\r\n"
            assert command(port, "SET", "Bangladesh", "Dhaka") == b"+OK\r\n" # ensure overwrite works
            assert command(port, "DEL", "Venezuela") == b":1\r\n"
            assert command(port, "SET", "Latvia", "Riga", "PX", "200") == b"+OK\r\n"
        except BaseException:
            server.kill()
            server.wait()
            raise
        server.send_signal(signal.SIGKILL)
        server.wait()
        time.sleep(0.3) # let Latvia's TTL pass

        server = start_server(port, aof, fsync_policy, eviction_policy, flags)
        try:
            reply = command(port, "GET", "Bangladesh")
            assert reply == b"$5\r\nDhaka\r\n", reply
            reply = command(port, "DBSIZE")
            assert reply == b":1\r\n", reply
            reply = command(port, "GET", "Venezuela")
            assert reply == b"$-1\r\n", reply
            reply = command(port, "GET", "Latvia")
            assert reply == b"$-1\r\n", reply
        finally:
            server.kill()
            server.wait()
    print(f"basic ({fsync_policy}, {eviction_policy}): ok")


def run_eviction_lru(fsync_policy):
    # LRU-specific test
    port = free_port()
    flags = ("--maxentries", "2")
    with tempfile.TemporaryDirectory() as tmp:
        aof = os.path.join(tmp, "test.aof")

        server = start_server(port, aof, fsync_policy, "lru", flags)
        try:
            assert command(port, "SET", "Tunisia", "Tunis") == b"+OK\r\n"
            assert command(port, "SET", "Mozambique", "Maputo") == b"+OK\r\n"
            # GETting Tunisia makes it most recently used, so Mozambique is LRU
            assert command(port, "GET", "Tunisia") == b"$5\r\nTunis\r\n"
            # SET then evicts Mozambique
            assert command(port, "SET", "India", "New Delhi") == b"+OK\r\n"
        except BaseException:
            server.kill()
            server.wait()
            raise
        server.send_signal(signal.SIGKILL)
        server.wait()

        server = start_server(port, aof, fsync_policy, "lru", flags)
        try:
            reply = command(port, "GET", "Tunisia")
            assert reply == b"$5\r\nTunis\r\n", reply
            reply = command(port, "GET", "Mozambique")
            assert reply == b"$-1\r\n", reply
            reply = command(port, "GET", "India")
            assert reply == b"$9\r\nNew Delhi\r\n", reply
            reply = command(port, "DBSIZE")
            assert reply == b":2\r\n", reply
        finally:
            server.kill()
            server.wait()
    print(f"eviction exact ({fsync_policy}, lru): ok")


def run_eviction_contents(fsync_policy, eviction_policy):
    # simply checks that the contents after the crash are equivalent to the contents beforehand
    port = free_port()
    capacity = 10 # s3fifo needs >= 10 for a non-empty small queue
    flags = ("--maxentries", str(capacity))
    keys = [f"key{i}" for i in range(25)]
    with tempfile.TemporaryDirectory() as tmp:
        aof = os.path.join(tmp, "test.aof")

        server = start_server(port, aof, fsync_policy, eviction_policy, flags)
        try:
            for i, k in enumerate(keys):
                assert command(port, "SET", k, f"v{i}") == b"+OK\r\n"
                if i % 3 == 0:   # some hits, so recency/frequency policies have something to work with
                    command(port, "GET", keys[i // 2])
            # snapshot contents before the crash (GET doesn't change contents or the AOF)
            before = {k: command(port, "GET", k) for k in keys}
            resident = [k for k, r in before.items() if r != b"$-1\r\n"]
            assert len(resident) == capacity, f"expected a full cache, got {len(resident)} keys"
        except BaseException:
            server.kill()
            server.wait()
            raise
        server.send_signal(signal.SIGKILL)
        server.wait()

        server = start_server(port, aof, fsync_policy, eviction_policy, flags)
        try:
            # check DBSIZE first: the GETs below don't change contents, but keep the order obvious
            reply = command(port, "DBSIZE")
            assert reply == f":{capacity}\r\n".encode(), reply
            for k in keys:
                reply = command(port, "GET", k)
                assert reply == before[k], f"{k}: before {before[k]!r}, after {reply!r}"
        finally:
            server.kill()
            server.wait()
    print(f"eviction contents ({fsync_policy}, {eviction_policy}): ok")


def run_compaction(fsync_policy, eviction_policy):
    """The log is compacted once it passes the min size, and the compacted log replays correctly."""
    port = free_port()
    flags = ("--aofrewriteminsize", "1024", "--maxentries", str(ROOMY))
    n = 200
    with tempfile.TemporaryDirectory() as tmp:
        aof = os.path.join(tmp, "test.aof")

        server = start_server(port, aof, fsync_policy, eviction_policy, flags)
        try:
            # keys that compaction must carry over (or drop) correctly
            assert command(port, "SET", "Chile", "Santiago", "EX", "60") == b"+OK\r\n"
            assert command(port, "SET", "Peru", "Lima") == b"+OK\r\n"
            assert command(port, "DEL", "Peru") == b":1\r\n"

            # many redundant commands
            for i in range(n):
                assert command(port, "SET", "counter", str(i)) == b"+OK\r\n"

            # without compaction the log would hold every one of those SETs
            uncompacted = n * len(encode("SET", "counter", str(n - 1)))
            size = os.path.getsize(aof)
            assert size < uncompacted // 2, f"log not compacted: {size} bytes (uncompacted ~{uncompacted})"
            assert not os.path.exists(aof + ".tmp"), "leftover temp file after rewrite"
        except BaseException:
            server.kill()
            server.wait()
            print(server.stderr.read().decode())
            raise

        server.send_signal(signal.SIGKILL)
        server.wait()

        server = start_server(port, aof, fsync_policy, eviction_policy, flags)
        try:
            reply = command(port, "GET", "counter")
            expected = str(n - 1).encode()
            assert reply == b"$" + str(len(expected)).encode() + b"\r\n" + expected + b"\r\n", reply
            reply = command(port, "GET", "Chile")
            assert reply == b"$8\r\nSantiago\r\n", reply
            reply = command(port, "GET", "Peru")
            assert reply == b"$-1\r\n", reply
            reply = command(port, "DBSIZE")
            assert reply == b":2\r\n", reply
        finally:
            server.kill()
            server.wait()
    print(f"compaction ({fsync_policy}, {eviction_policy}): ok")


if __name__ == "__main__":
    for fsync_policy in FSYNC_POLICIES:
        run(fsync_policy, "lru")
    for eviction_policy in EVICTION_POLICIES:
        run("everysec", eviction_policy)
        run_eviction_contents("everysec", eviction_policy)
        run_compaction("everysec", eviction_policy)
    run_eviction_lru("everysec")