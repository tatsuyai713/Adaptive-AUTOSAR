/// @file src/ara/per/read_write_accessor.cpp
/// @brief Implementation for read write accessor.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./read_write_accessor.h"
#include <sys/stat.h>
#include <unistd.h>

namespace ara
{
    namespace per
    {
        ReadWriteAccessor::ReadWriteAccessor(const std::string &filePath)
            : mStream{filePath,
                      std::ios::binary | std::ios::in | std::ios::out},
              mFilePath{filePath}
        {
            if (!mStream.is_open())
            {
                // File may not exist; create it
                std::ofstream create(filePath, std::ios::binary);
                create.close();
                mStream.open(
                    filePath,
                    std::ios::binary | std::ios::in | std::ios::out);
            }
        }

        ReadWriteAccessor::~ReadWriteAccessor() noexcept
        {
            if (mStream.is_open())
            {
                mStream.close();
            }
        }

        ReadWriteAccessor::ReadWriteAccessor(
            ReadWriteAccessor &&other) noexcept
            : mStream{std::move(other.mStream)},
              mFilePath{std::move(other.mFilePath)}
        {
        }

        ReadWriteAccessor &ReadWriteAccessor::operator=(
            ReadWriteAccessor &&other) noexcept
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

        core::Result<std::size_t> ReadWriteAccessor::Read(
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

        core::Result<std::size_t> ReadWriteAccessor::Write(
            const std::uint8_t *data, std::size_t count)
        {
            if (!mStream.is_open())
            {
                return core::Result<std::size_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            auto posBefore = mStream.tellp();
            mStream.write(reinterpret_cast<const char *>(data),
                          static_cast<std::streamsize>(count));

            if (mStream.bad())
            {
                return core::Result<std::size_t>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            auto posAfter = mStream.tellp();
            return core::Result<std::size_t>::FromValue(
                static_cast<std::size_t>(posAfter - posBefore));
        }

        core::Result<void> ReadWriteAccessor::Sync()
        {
            if (!mStream.is_open())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            mStream.flush();
            if (mStream.bad())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<void>::FromValue();
        }

        core::Result<std::uint64_t> ReadWriteAccessor::GetSize() const
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

        core::Result<void> ReadWriteAccessor::SetFileSize(std::uint64_t size)
        {
            // Flush first to ensure consistency
            if (mStream.is_open())
            {
                mStream.flush();
            }

            if (::truncate(mFilePath.c_str(),
                           static_cast<off_t>(size)) != 0)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kPhysicalStorageFailure));
            }

            return core::Result<void>::FromValue();
        }

        bool ReadWriteAccessor::IsValid() const noexcept
        {
            return mStream.is_open() && mStream.good();
        }
    }
}
