#!/usr/bin/env python3
"""
Apply QNX support patches to OBD-II-Emulator.

Patches:
  1. Replace BSD-style u_int32_t / u_int8_t etc. with POSIX uint*_t.
  2. Replace Linux-specific serial_communication.h / .cpp with a
     QNX-compatible version that:
       - Uses POSIX termios instead of termios2/TCGETS2/TCSETS2
       - Uses a pipe() pair instead of signalfd() for stop signalling
       - Replaces <asm/termbits.h> with <termios.h>

This script is idempotent: running it twice produces the same result.

Usage:
    python3 apply_obd_emulator_qnx_support.py <path-to-obd-ii-emulator-src>
"""

import sys
import os
import re

# Map of BSD/glibc type aliases to POSIX standard types
U_INT_REPLACEMENTS = [
    (r'\bu_int64_t\b', 'uint64_t'),
    (r'\bu_int32_t\b', 'uint32_t'),
    (r'\bu_int16_t\b', 'uint16_t'),
    (r'\bu_int8_t\b',  'uint8_t'),
]

# Idempotency marker
MARKER = "QNX_AUTOSAR_PATCHED"

# Replacement header for serial_communication.h (QNX-compatible)
SERIAL_COMM_H_QNX = """\
// {marker}
// serial_communication.h - patched for QNX
// Uses POSIX termios and pipe() for stop signalling instead of
// Linux-specific <asm/termbits.h> and signalfd().
#ifndef SERIAL_COMMUNICATION_H
#define SERIAL_COMMUNICATION_H

#include <signal.h>
#include <poll.h>
#include <termios.h>
#include <queue>
#include <future>
#include "./communication_layer.h"
#include "./packet_buffer.h"

namespace ObdEmulator
{{
    /// @brief Serial port communication layer
    class SerialCommunication : public CommunicationLayer
    {{
    private:
        static const int cErrorCode{{-1}};
        static const size_t cSingalFdIndex{{0}};   // pipe read-end fd index
        static const size_t cCommunicationFdIndex{{1}};
        static const size_t cNumberOfFileDescriptors{{2}};
        static const size_t cReadBufferSize{{30}};

        const std::string mSerialPort;
        const speed_t mBaudrate;
        const int mTimeout;

        struct pollfd mFileDescriptors[cNumberOfFileDescriptors];
        int mPipeWriteFd{{-1}};   // write end of the stop-signal pipe
        PacketBuffer mSendBuffer;
        std::promise<bool> mPromise;
        std::future<bool> mFuture;
        std::thread mPollingThread;

        bool trySetupCommunication(int &fileDescriptor) noexcept;
        bool tryReceive();
        bool trySend();
        void tryPoll();

    public:
        SerialCommunication(
            std::string serialPort, speed_t baudrate, int timeout = 1);

        ~SerialCommunication();

        bool TryStart(std::vector<uint8_t> &&configuration) override;
        bool TrySendAsync(std::vector<uint8_t> &&data) override;
        bool TryStop() override;
    }};
}}

#endif
""".format(marker=MARKER)

# Replacement implementation for serial_communication.cpp (QNX-compatible)
SERIAL_COMM_CPP_QNX = """\
// {marker}
// serial_communication.cpp - patched for QNX
// Uses POSIX termios and pipe() for stop signalling instead of
// Linux-specific signalfd() and termios2/TCGETS2/TCSETS2.
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstring>
#include "../include/obdemulator/serial_communication.h"

namespace ObdEmulator
{{
    SerialCommunication::SerialCommunication(
        std::string serialPort,
        speed_t baudrate,
        int timeout)
        : mSerialPort{{serialPort}}, mBaudrate{{baudrate}}, mTimeout{{timeout}}
    {{
        int pipefd[2];
        if (pipe(pipefd) != 0)
        {{
            throw std::runtime_error("The flow control pipe creation failed.");
        }}
        // Read end is polled; write end is used to signal stop.
        mFileDescriptors[cSingalFdIndex].fd = pipefd[0];
        mFileDescriptors[cSingalFdIndex].events = POLLIN;
        mPipeWriteFd = pipefd[1];
    }}

    bool SerialCommunication::trySetupCommunication(int &fileDescriptor) noexcept
    {{
        fileDescriptor = open(mSerialPort.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
        if (fileDescriptor <= cErrorCode) return false;

        struct termios _options;
        if (tcgetattr(fileDescriptor, &_options) != 0) return false;

        cfmakeraw(&_options);
        _options.c_cflag |= (CLOCAL | CREAD);
#ifdef CRTSCTS
        _options.c_cflag &= ~CRTSCTS;
#elif defined(IHFLOW) && defined(OHFLOW)
        // QNX uses IHFLOW/OHFLOW instead of CRTSCTS
        _options.c_cflag &= ~(IHFLOW | OHFLOW);
#endif
        _options.c_iflag &= ~(IXON | IXOFF | IXANY);

        if (cfsetspeed(&_options, mBaudrate) != 0) return false;
        if (tcsetattr(fileDescriptor, TCSANOW, &_options) != 0) return false;

        return true;
    }}

    bool SerialCommunication::tryReceive()
    {{
        int _communicationFd{{mFileDescriptors[cCommunicationFdIndex].fd}};
        std::vector<uint8_t> _readBuffer(cReadBufferSize);

        auto _numberOfReadBytes{{
            read(_communicationFd, _readBuffer.data(), cReadBufferSize)}};

        bool _result;
        if (_numberOfReadBytes == cErrorCode)
        {{
            _result = (errno == EAGAIN) || (errno == EWOULDBLOCK) || (errno == EINTR);
        }}
        else
        {{
            _readBuffer.resize(_numberOfReadBytes);

            if (Callback)
            {{
                std::vector<uint8_t> _response;
                bool _handled{{Callback(std::move(_readBuffer), _response)}};
                if (_handled)
                {{
                    mSendBuffer.TryEnqueue(std::move(_response));
                }}
            }}
            else if (AsyncCallback)
            {{
                AsyncCallback(std::move(_readBuffer));
            }}

            _result = true;
        }}

        return _result;
    }}

    bool SerialCommunication::trySend()
    {{
        int _communicationFd{{mFileDescriptors[cCommunicationFdIndex].fd}};

        while (!mSendBuffer.Empty())
        {{
            std::vector<uint8_t> _response;
            if (mSendBuffer.TryDequeue(_response))
            {{
                auto _numberOfSentBytes{{
                    write(_communicationFd, _response.data(), _response.size())}};

                if ((_numberOfSentBytes == cErrorCode) &&
                    (errno != EAGAIN) && (errno != EWOULDBLOCK) && (errno != EINTR))
                {{
                    return false;
                }}
            }}

            std::this_thread::yield();
        }}

        return true;
    }}

    void SerialCommunication::tryPoll()
    {{
        bool _result{{true}};
        bool _running{{true}};

        while (_running)
        {{
            int _polledFileDescriptors{{
                poll(mFileDescriptors, cNumberOfFileDescriptors, mTimeout)}};

            if (_polledFileDescriptors > 0)
            {{
                // Check if the stop pipe has been written to
                if (mFileDescriptors[cSingalFdIndex].revents & POLLIN)
                {{
                    char _buf;
                    read(mFileDescriptors[cSingalFdIndex].fd, &_buf, 1);
                    _running = false;
                }}
                else
                {{
                    if (mFileDescriptors[cCommunicationFdIndex].revents & POLLIN)
                    {{
                        _result &= tryReceive();
                    }}

                    if (mFileDescriptors[cCommunicationFdIndex].revents & POLLOUT)
                    {{
                        _result &= trySend();
                    }}

                    _running = _result;
                }}
            }}
            else if ((_polledFileDescriptors == cErrorCode) && (errno != EINTR))
            {{
                _result = false;
                _running = false;
            }}
        }}

        mPromise.set_value_at_thread_exit(_result);
    }}

    bool SerialCommunication::TryStart(std::vector<uint8_t> &&configuration)
    {{
        bool _result{{!mFuture.valid()}};
        if (!_result) return false;

        int _communicationFd;
        _result = trySetupCommunication(_communicationFd);
        if (!_result) return false;

        mFileDescriptors[cCommunicationFdIndex].fd = _communicationFd;
        mFileDescriptors[cCommunicationFdIndex].events = POLLIN | POLLOUT;

        _result = mSendBuffer.TryEnqueue(std::move(configuration));
        if (!_result) return false;

        mFuture = mPromise.get_future();
        mPollingThread = std::thread(&SerialCommunication::tryPoll, this);

        return true;
    }}

    bool SerialCommunication::TrySendAsync(std::vector<uint8_t> &&data)
    {{
        return mSendBuffer.TryEnqueue(std::move(data));
    }}

    bool SerialCommunication::TryStop()
    {{
        bool _result{{mFuture.valid()}};

        if (_result)
        {{
            // Write a byte to the pipe to stop the polling loop
            char _byte{{1}};
            _result = (write(mPipeWriteFd, &_byte, 1) == 1);

            if (_result)
            {{
                _result = mFuture.get();
                if (mPollingThread.joinable())
                {{
                    mPollingThread.join();
                }}

                _result &= mSendBuffer.TryClear();

                int _communicationFd{{mFileDescriptors[cCommunicationFdIndex].fd}};
                _result &= (close(_communicationFd) == 0);
            }}
        }}

        return _result;
    }}

    SerialCommunication::~SerialCommunication()
    {{
        TryStop();
        // Close the pipe fds
        close(mFileDescriptors[cSingalFdIndex].fd);
        close(mPipeWriteFd);
    }}
}}
""".format(marker=MARKER)


def patch_u_int_types(filepath: str) -> None:
    with open(filepath, 'r') as f:
        content = f.read()

    original = content
    for pattern, replacement in U_INT_REPLACEMENTS:
        content = re.sub(pattern, replacement, content)

    if content != original:
        with open(filepath, 'w') as f:
            f.write(content)
        print(f'[INFO] Patched u_int types in {filepath}')


def patch_serial_communication(src_root: str) -> None:
    """Replace serial_communication.{h,cpp} with QNX-compatible versions."""
    header_path = os.path.join(
        src_root, "include", "obdemulator", "serial_communication.h"
    )
    src_path = os.path.join(src_root, "src", "serial_communication.cpp")

    for filepath, new_content in [
        (header_path, SERIAL_COMM_H_QNX),
        (src_path, SERIAL_COMM_CPP_QNX),
    ]:
        if not os.path.exists(filepath):
            print(f"[WARN] {filepath} not found, skipping")
            continue

        with open(filepath, "r") as f:
            existing = f.read()

        if MARKER in existing:
            print(f"[INFO] {filepath} already patched, skipping")
            continue

        print(f"[INFO] Replacing {filepath} with QNX-compatible version")
        with open(filepath, "w") as f:
            f.write(new_content)


def apply_patch(src_root: str) -> None:
    # 1) Fix u_int* types in all .cpp and .h files
    for search_dir in ["src", "include"]:
        full_dir = os.path.join(src_root, search_dir)
        if not os.path.isdir(full_dir):
            continue
        for dirpath, _, filenames in os.walk(full_dir):
            for fname in filenames:
                if fname.endswith('.cpp') or fname.endswith('.h'):
                    filepath = os.path.join(dirpath, fname)
                    # Skip the serial_communication files that we'll replace wholesale
                    if "serial_communication" not in fname:
                        patch_u_int_types(filepath)

    # 2) Replace serial_communication.{h,cpp} with QNX-compatible versions
    patch_serial_communication(src_root)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <path-to-obd-ii-emulator-src>', file=sys.stderr)
        sys.exit(1)
    apply_patch(sys.argv[1])
