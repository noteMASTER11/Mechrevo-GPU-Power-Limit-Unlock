"""Defer ordinary termination through child restoration and evidence persistence.
SIGKILL, crashes and power loss remain outside this software guarantee.
"""
from contextlib import contextmanager
import signal
@contextmanager
def blocked_signals():
    previous=signal.pthread_sigmask(signal.SIG_BLOCK,{signal.SIGINT,signal.SIGTERM,signal.SIGHUP})
    try:
        yield
    finally:
        signal.pthread_sigmask(signal.SIG_SETMASK,previous)
