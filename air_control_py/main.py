import ctypes
import mmap
import os
import signal
import subprocess
import threading
import time

_libc = ctypes.CDLL(None, use_errno=True)

TOTAL_TAKEOFFS = 20
STRIPS = 5
shm_data = []

# TODO1: Size of shared memory for 3 integers (current process pid, radio, ground) use ctypes.sizeof()
# SHM_LENGTH=

# Global variables and locks
planes = 0  # planes waiting
takeoffs = 0  # local takeoffs (per thread)
total_takeoffs = 0  # total takeoffs
# Locks (mutexes)
state_lock = threading.Lock()
runway1_lock = threading.Lock()
runway2_lock = threading.Lock()

# Shared memory name used by the C programs in this project
SH_MEMORY_NAME = b"/SharedMemory"
# size for three integers
SHM_LENGTH = ctypes.sizeof(ctypes.c_int) * 3



def create_shared_memory():
    """Create shared memory segment for PID exchange"""
    # Use POSIX shm_open via libc to create a shared memory segment
    name = SH_MEMORY_NAME
    # set argtypes/ret for safety
    _libc.shm_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_int]
    _libc.shm_open.restype = ctypes.c_int
    _libc.ftruncate.argtypes = [ctypes.c_int, ctypes.c_size_t]
    _libc.ftruncate.restype = ctypes.c_int

    # temporarily set umask so that created shm has proper perms
    old_umask = os.umask(0)
    try:
        fd = _libc.shm_open(name, os.O_CREAT | os.O_RDWR, 0o666)
        if fd < 0:
            err = ctypes.get_errno()
            raise OSError(err, "shm_open failed")
        # set size
        if _libc.ftruncate(fd, SHM_LENGTH) != 0:
            err = ctypes.get_errno()
            # close fd before raising
            try:
                os.close(fd)
            except Exception:
                pass
            raise OSError(err, "ftruncate failed")
    finally:
        os.umask(old_umask)

    # map the shared memory
    mm = mmap.mmap(fd, SHM_LENGTH, flags=mmap.MAP_SHARED, prot=mmap.PROT_READ | mmap.PROT_WRITE)
    # create an int view to access three ints directly
    data = memoryview(mm).cast('i')
    return fd, mm, data



def HandleUSR2(signum, frame):
    """Handle external signal indicating arrival of 5 new planes.
    Complete function to update waiting planes"""
    global planes
    # increment planes by 5 in a thread-safe way
    with state_lock:
        planes += 5


def TakeOffFunction(agent_id: int):
    """Function executed by each THREAD to control takeoffs.
    Complete using runway1_lock and runway2_lock and state_lock to synchronize"""
    global planes, takeoffs, total_takeoffs

    # TODO: implement the logic to control a takeoff thread
    # Each thread repeatedly tries to acquire a runway and process a takeoff
    global shm_data
    while True:
        # quick check whether we're done
        with state_lock:
            if total_takeoffs >= TOTAL_TAKEOFFS:
                break

        acquired_runway = None
        # try runway1
        if runway1_lock.acquire(blocking=False):
            acquired_runway = 1
        elif runway2_lock.acquire(blocking=False):
            acquired_runway = 2
        else:
            # no runway available, retry shortly
            time.sleep(0.001)
            continue

        # we hold a runway now
        try:
            radio_pid = None
            with state_lock:
                # re-check termination condition
                if total_takeoffs >= TOTAL_TAKEOFFS:
                    break

                if planes > 0:
                    planes -= 1
                    takeoffs += 1
                    total_takeoffs += 1
                    # read radio pid from shared memory if available
                    try:
                        radio_pid = int(shm_data[1])
                    except Exception:
                        radio_pid = None

                    # if block of 5 takeoffs completed, notify radio
                    if takeoffs >= 5:
                        if radio_pid:
                            try:
                                os.kill(radio_pid, signal.SIGUSR1)
                            except Exception:
                                pass
                        takeoffs = 0
                else:
                    # no planes to take off, continue loop
                    pass

            # simulate takeoff time regardless whether we processed a plane or not
            time.sleep(1)

            # after sleeping, check if overall goal reached and notify radio to terminate
            with state_lock:
                done = total_takeoffs >= TOTAL_TAKEOFFS
            if done:
                try:
                    radio_pid = int(shm_data[1])
                    os.kill(radio_pid, signal.SIGTERM)
                except Exception:
                    pass
                break
        finally:
            # release the runway we acquired
            if acquired_runway == 1:
                try:
                    runway1_lock.release()
                except RuntimeError:
                    pass
            elif acquired_runway == 2:
                try:
                    runway2_lock.release()
                except RuntimeError:
                    pass


def launch_radio():
    """unblock the SIGUSR2 signal so the child receives it"""
    def _unblock_sigusr2():
        signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGUSR2})

    # TODO 8: Launch the external 'radio' process using subprocess.Popen()
    # process = 
    # return process
    # Assume the 'radio' executable is available on the PATH or in the current
    # working directory as './radio'. Pass the shared memory name as argument.
    proc = subprocess.Popen(["./radio", SH_MEMORY_NAME.decode('utf-8')], preexec_fn=_unblock_sigusr2)
    return proc


def main():
    global shm_data

    # register handler for SIGUSR2
    signal.signal(signal.SIGUSR2, HandleUSR2)

    # create shared memory and save our pid in index 0
    fd, mm, data = create_shared_memory()
    shm_data = data
    data[0] = os.getpid()

    # launch radio and store its pid in shared memory index 1
    radio_process = launch_radio()
    # wait a tiny bit for the child to start and register its pid
    time.sleep(0.05)
    try:
        data[1] = int(radio_process.pid)
    except Exception:
        data[1] = 0

    # create and start STRIPS threads
    threads = []
    for i in range(STRIPS):
        t = threading.Thread(target=TakeOffFunction, args=(i,))
        t.start()
        threads.append(t)

    # wait for threads to finish
    for t in threads:
        t.join()

    # cleanup: inform radio (again) and release shared memory
    try:
        pid = int(shm_data[1])
        if pid:
            try:
                os.kill(pid, signal.SIGTERM)
            except Exception:
                pass
    except Exception:
        pass

    # close mapping and file descriptor and unlink shared memory
    try:
        mm.close()
    except Exception:
        pass
    try:
        os.close(fd)
    except Exception:
        pass
    try:
        _libc.shm_unlink(SH_MEMORY_NAME)
    except Exception:
        pass



if __name__ == "__main__":
    main()