/// @file src/ara/per/read_accessor.cpp
/// @brief Implementation for read accessor.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./read_accessor.h"
#include <sys/stat.h>

namespace ara
{
    namespace per
    {
        ReadAccessor::ReadAccessor(const std::string &filePath)
            : mStream{filePath, std::ios::binary},
              mFilePath{filePath}
        {
        }

        ReadAccessor::~ReadAccessor() noexcept
        {
            if (mStream.is_open())
            {
                mStream.close();
            }
        }

        ReadAccessor::ReadAccessor(ReadAccessor &&other) noexcept
            : mStream{std::move(other.mStream)},
              mFilePath{std::move(other.mFilePath)}
        {
        }

        ReadAccessor &ReadAccessor::operator=(ReadAccessor &&other) noexcept
        {
            if (this != &other)
            {
                if (mStream.is_open())
                {
                    mStream.close();
                }
                mStream = std::move(other.mStream);
                mFilePath = std::move(other.mFilePath);
            }
            return *this;
        }

        core::Result<std::size_t> ReadAccessor::Read(
            std::uint8_t *buffer, std::size_t count)
        {
            if (!mStream.is_open() || !mStream.good())
            {
                return core::Result<std::size_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            mStream.read(reinterpret_cast<char *>(buffer),
                         static_cast<std::streamsize>(count));

            if (mStream.bad())
            {
                return core::Result<std::size_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<std::size_t>::FromValue(
                static_cast<std::size_t>(mStream.gcount()));
        }

        core::Result<std::uint64_t> ReadAccessor::GetSize() const
        {
            struct stat st;
            if (::stat(mFilePath.c_str(), &st) != 0)
            {
                return core::Result<std::uint64_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }
            return core::Result<std::uint64_t>::FromValue(
                static_cast<std::uint64_t>(st.st_size));
        }

        core::Result<void> ReadAccessor::Peek(std::uint8_t &byte)
        {
            if (!mStream.is_open() || !mStream.good())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            int ch = mStream.peek();
            if (ch == std::char_traits<char>::eof())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            byte = static_cast<std::uint8_t>(ch);
            return core::Result<void>::FromValue();
        }

        bool ReadAccessor::IsValid() const noexcept
        {
            return mStream.is_open() && mStream.good();
        }
    }
}
