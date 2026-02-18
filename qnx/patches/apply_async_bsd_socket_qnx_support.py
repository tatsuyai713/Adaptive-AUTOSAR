#!/usr/bin/env python3
"""
Apply QNX support patches to upstream Async-BSD-Socket-Lib.

Replaces the Linux epoll-based Poller with a POSIX poll()-based implementation
that is compatible with QNX (and any POSIX-compliant OS without epoll).

This script is idempotent: running it twice produces the same result.

Usage:
    python3 apply_async_bsd_socket_qnx_support.py <path-to-async-bsd-socket-lib-src>
"""

import sys
import os

# poll()-based Poller replacement for QNX / non-Linux POSIX systems.
POLLER_CPP_POLL = r"""#include <poll.h>
#include <cerrno>
#include <stdexcept>
#include <vector>
#include <map>
#include "asyncbsdsocket/poller.h"

namespace AsyncBsdSocketLib
{
    Poller::Poller()
        : mFileDescriptor(-1), mEventCounter(0)
    {
    }

    Poller::~Poller() noexcept
    {
    }

    bool Poller::tryModifyAsSenderReceiver(int connectionDescriptor)
    {
        (void)connectionDescriptor;
        return true;
    }

    bool Poller::TryAddListener(
        TcpListener *tcpListener, std::function<void()> callback)
    {
        int _socketDescriptor = tcpListener->Descriptor();
        std::lock_guard<std::mutex> _lock(mMutex);
        mListeners[_socketDescriptor] = callback;
        ++mEventCounter;
        return true;
    }

    bool Poller::TryAddSender(
        Communicator *communicator, std::function<void()> callback)
    {
        int _connectionDescriptor = communicator->Connection();
        std::lock_guard<std::mutex> _lock(mMutex);
        mSenders[_connectionDescriptor] = callback;
        ++mEventCounter;
        return true;
    }

    bool Poller::TryAddReceiver(
        Communicator *communicator, std::function<void()> callback)
    {
        int _connectionDescriptor = communicator->Connection();
        std::lock_guard<std::mutex> _lock(mMutex);
        mReceivers[_connectionDescriptor] = callback;
        ++mEventCounter;
        return true;
    }

    bool Poller::TryRemoveListener(TcpListener *tcpListener)
    {
        int _socketDescriptor = tcpListener->Descriptor();
        std::lock_guard<std::mutex> _lock(mMutex);
        auto it = mListeners.find(_socketDescriptor);
        if (it == mListeners.end())
            return false;
        mListeners.erase(it);
        --mEventCounter;
        return true;
    }

    bool Poller::TryRemoveSender(Communicator *communicator)
    {
        int _connectionDescriptor = communicator->Connection();
        std::lock_guard<std::mutex> _lock(mMutex);
        auto it = mSenders.find(_connectionDescriptor);
        if (it == mSenders.end())
            return false;
        mSenders.erase(it);
        --mEventCounter;
        return true;
    }

    bool Poller::TryRemoveReceiver(Communicator *communicator)
    {
        int _connectionDescriptor = communicator->Connection();
        std::lock_guard<std::mutex> _lock(mMutex);
        auto it = mReceivers.find(_connectionDescriptor);
        if (it == mReceivers.end())
            return false;
        mReceivers.erase(it);
        --mEventCounter;
        return true;
    }

    bool Poller::TryPoll(int timeout) const
    {
        // Build fd event map from all registered fds.
        // Note: no mutex in this const method (matching upstream epoll design where
        // TryPoll also does not hold mMutex; callers must not concurrently modify).
        std::map<int, short> fdEvents;
        for (const auto &kv : mListeners)
            fdEvents[kv.first] |= POLLIN;
        for (const auto &kv : mReceivers)
            fdEvents[kv.first] |= POLLIN;
        for (const auto &kv : mSenders)
            fdEvents[kv.first] |= POLLOUT;

        if (fdEvents.empty())
            return true;

        std::vector<struct pollfd> fds;
        fds.reserve(fdEvents.size());
        for (const auto &kv : fdEvents)
        {
            struct pollfd pfd;
            pfd.fd = kv.first;
            pfd.events = kv.second;
            pfd.revents = 0;
            fds.push_back(pfd);
        }

        int _fdNo = poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);

        if (_fdNo == -1)
            return false;
        if (_fdNo == 0)
            return true;

        // Collect callbacks to fire then invoke outside the iteration
        std::vector<std::function<void()>> toFire;
        for (const auto &pfd : fds)
        {
            if (pfd.revents == 0)
                continue;

            const int _fd = pfd.fd;

            if (pfd.revents & POLLIN)
            {
                auto it = mListeners.find(_fd);
                if (it != mListeners.end())
                    toFire.push_back(it->second);

                it = mReceivers.find(_fd);
                if (it != mReceivers.end())
                    toFire.push_back(it->second);
            }

            if (pfd.revents & POLLOUT)
            {
                auto it = mSenders.find(_fd);
                if (it != mSenders.end())
                    toFire.push_back(it->second);
            }
        }

        for (const auto &cb : toFire)
            cb();

        return true;
    }
}
"""


def apply_patch(src_root):
    poller_cpp = os.path.join(src_root, "src", "poller.cpp")

    if not os.path.isfile(poller_cpp):
        print(f"[WARN] poller.cpp not found at {poller_cpp}, skipping.", file=sys.stderr)
        return

    # Idempotency check: already patched if epoll is absent
    with open(poller_cpp, "r") as f:
        current = f.read()

    if "epoll" not in current:
        print(f"[INFO] {poller_cpp} already patched (no epoll found), skipping.")
        return

    print(f"[INFO] Applying QNX poll() patch to {poller_cpp}")
    with open(poller_cpp, "w") as f:
        f.write(POLLER_CPP_POLL)
    print(f"[INFO] QNX poll() patch applied to {poller_cpp}")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path-to-async-bsd-socket-lib-src>", file=sys.stderr)
        sys.exit(1)
    apply_patch(sys.argv[1])
