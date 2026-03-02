/// @file qnx/patches/poller_posix.cpp
/// @brief POSIX poll()-based implementation of AsyncBsdSocketLib::Poller.
/// @details Replaces the Linux epoll-based poller.cpp when building for
///          platforms that lack sys/epoll.h (e.g. QNX Neutrino).
///          The public API is identical to the epoll version.

#include <unistd.h>
#include <poll.h>
#include <cerrno>
#include <stdexcept>
#include <vector>
#include "asyncbsdsocket/poller.h"

namespace AsyncBsdSocketLib
{
    Poller::Poller()
    {
        // poll() does not require a special file descriptor.
        // We set mFileDescriptor to -1 as a sentinel.
        mFileDescriptor = -1;
        mEventCounter = 0;
    }

    Poller::~Poller() noexcept
    {
        // Nothing to close — poll() has no kernel object.
    }

    bool Poller::tryModifyAsSenderReceiver(int connectionDescriptor)
    {
        // With poll() there is no kernel structure to modify.
        // The pollfd array is rebuilt each time in TryPoll().
        return true;
    }

    bool Poller::TryAddListener(
        TcpListener *tcpListener, std::function<void()> callback)
    {
        int _socketDescriptor = tcpListener->Descriptor();

        {
            std::lock_guard<std::mutex> _lock(mMutex);
            if (mListeners.find(_socketDescriptor) != mListeners.end())
            {
                return false; // already added
            }
            mListeners[_socketDescriptor] = callback;
            ++mEventCounter;
        }

        return true;
    }

    bool Poller::TryAddSender(
        Communicator *communicator, std::function<void()> callback)
    {
        int _connectionDescriptor = communicator->Connection();

        std::lock_guard<std::mutex> _lock(mMutex);
        mSenders[_connectionDescriptor] = callback;

        if (mReceivers.find(_connectionDescriptor) == mReceivers.end())
        {
            // Brand-new fd — count it
            ++mEventCounter;
        }

        return true;
    }

    bool Poller::TryAddReceiver(
        Communicator *communicator, std::function<void()> callback)
    {
        int _connectionDescriptor = communicator->Connection();

        std::lock_guard<std::mutex> _lock(mMutex);
        mReceivers[_connectionDescriptor] = callback;

        if (mSenders.find(_connectionDescriptor) == mSenders.end())
        {
            // Brand-new fd — count it
            ++mEventCounter;
        }

        return true;
    }

    bool Poller::TryRemoveListener(TcpListener *tcpListener)
    {
        int _socketDescriptor = tcpListener->Descriptor();

        std::lock_guard<std::mutex> _lock(mMutex);
        auto _itr = mListeners.find(_socketDescriptor);
        if (_itr == mListeners.end())
        {
            return false;
        }
        mListeners.erase(_itr);
        --mEventCounter;

        return true;
    }

    bool Poller::TryRemoveSender(Communicator *communicator)
    {
        int _connectionDescriptor = communicator->Connection();

        std::lock_guard<std::mutex> _lock(mMutex);
        auto _itr = mSenders.find(_connectionDescriptor);
        if (_itr == mSenders.end())
        {
            return false;
        }
        mSenders.erase(_itr);

        if (mReceivers.find(_connectionDescriptor) == mReceivers.end())
        {
            --mEventCounter;
        }

        return true;
    }

    bool Poller::TryRemoveReceiver(Communicator *communicator)
    {
        int _connectionDescriptor = communicator->Connection();

        std::lock_guard<std::mutex> _lock(mMutex);
        auto _itr = mReceivers.find(_connectionDescriptor);
        if (_itr == mReceivers.end())
        {
            return false;
        }
        mReceivers.erase(_itr);

        if (mSenders.find(_connectionDescriptor) == mSenders.end())
        {
            --mEventCounter;
        }

        return true;
    }

    bool Poller::TryPoll(int timeout) const
    {
        // Collect all unique fds and build the pollfd array.
        // Because TryPoll is const we cannot lock mMutex (it is not mutable);
        // the original epoll implementation also accesses maps without locking
        // in TryPoll, so we follow the same contract.

        std::map<int, short> fdEventMap;

        for (const auto &pair : mListeners)
        {
            fdEventMap[pair.first] |= POLLIN;
        }
        for (const auto &pair : mReceivers)
        {
            fdEventMap[pair.first] |= POLLIN;
        }
        for (const auto &pair : mSenders)
        {
            fdEventMap[pair.first] |= POLLOUT;
        }

        if (fdEventMap.empty())
        {
            return true; // nothing to poll
        }

        std::vector<struct pollfd> pfds;
        pfds.reserve(fdEventMap.size());
        for (const auto &pair : fdEventMap)
        {
            struct pollfd pfd;
            pfd.fd = pair.first;
            pfd.events = pair.second;
            pfd.revents = 0;
            pfds.push_back(pfd);
        }

        int _result = ::poll(pfds.data(),
                             static_cast<nfds_t>(pfds.size()),
                             timeout);
        if (_result == -1)
        {
            return false;
        }

        for (const auto &pfd : pfds)
        {
            if (pfd.revents == 0)
            {
                continue;
            }

            int _fd = pfd.fd;

            auto _listenerItr = mListeners.find(_fd);
            if (_listenerItr != mListeners.end() &&
                (pfd.revents & POLLIN))
            {
                (_listenerItr->second)();
            }

            auto _senderItr = mSenders.find(_fd);
            if (_senderItr != mSenders.end() &&
                (pfd.revents & POLLOUT))
            {
                (_senderItr->second)();
            }

            auto _receiverItr = mReceivers.find(_fd);
            if (_receiverItr != mReceivers.end() &&
                (pfd.revents & POLLIN))
            {
                (_receiverItr->second)();
            }
        }

        return true;
    }
}
