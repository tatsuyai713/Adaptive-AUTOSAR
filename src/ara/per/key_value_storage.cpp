#include "./key_value_storage.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace ara
{
    namespace per
    {
        // ── Base64 encoding/decoding ──────────────────────────

        static const char cBase64Chars[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string KeyValueStorage::EncodeBase64(
            const std::vector<std::uint8_t> &data)
        {
            std::string result;
            std::size_t i = 0U;
            std::uint8_t array3[3];
            std::uint8_t array4[4];
            std::size_t len = data.size();

            while (len--)
            {
                array3[i++] = data[data.size() - len - 1U];
                if (i == 3U)
                {
                    array4[0] = (array3[0] & 0xfcU) >> 2;
                    array4[1] =
                        ((array3[0] & 0x03U) << 4) +
                        ((array3[1] & 0xf0U) >> 4);
                    array4[2] =
                        ((array3[1] & 0x0fU) << 2) +
                        ((array3[2] & 0xc0U) >> 6);
                    array4[3] = array3[2] & 0x3fU;

                    for (i = 0U; i < 4U; ++i)
                    {
                        result += cBase64Chars[array4[i]];
                    }
                    i = 0U;
                }
            }

            if (i != 0U)
            {
                for (std::size_t j = i; j < 3U; ++j)
                {
                    array3[j] = 0U;
                }

                array4[0] = (array3[0] & 0xfcU) >> 2;
                array4[1] =
                    ((array3[0] & 0x03U) << 4) +
                    ((array3[1] & 0xf0U) >> 4);
                array4[2] =
                    ((array3[1] & 0x0fU) << 2) +
                    ((array3[2] & 0xc0U) >> 6);

                for (std::size_t j = 0U; j <= i; ++j)
                {
                    result += cBase64Chars[array4[j]];
                }

                while (i++ < 3U)
                {
                    result += '=';
                }
            }

            return result;
        }

        std::vector<std::uint8_t> KeyValueStorage::DecodeBase64(
            const std::string &encoded)
        {
            std::vector<std::uint8_t> result;
            std::size_t i = 0U;
            std::uint8_t array4[4];
            std::uint8_t array3[3];

            std::string chars(cBase64Chars);

            for (char c : encoded)
            {
                if (c == '=')
                {
                    break;
                }

                std::size_t pos = chars.find(c);
                if (pos == std::string::npos)
                {
                    continue;
                }

                array4[i++] = static_cast<std::uint8_t>(pos);
                if (i == 4U)
                {
                    array3[0] = (array4[0] << 2) + ((array4[1] & 0x30U) >> 4);
                    array3[1] =
                        ((array4[1] & 0x0fU) << 4) +
                        ((array4[2] & 0x3cU) >> 2);
                    array3[2] = ((array4[2] & 0x03U) << 6) + array4[3];

                    for (i = 0U; i < 3U; ++i)
                    {
                        result.push_back(array3[i]);
                    }
                    i = 0U;
                }
            }

            if (i != 0U)
            {
                for (std::size_t j = i; j < 4U; ++j)
                {
                    array4[j] = 0U;
                }

                array3[0] = (array4[0] << 2) + ((array4[1] & 0x30U) >> 4);
                array3[1] =
                    ((array4[1] & 0x0fU) << 4) +
                    ((array4[2] & 0x3cU) >> 2);
                array3[2] = ((array4[2] & 0x03U) << 6) + array4[3];

                for (std::size_t j = 0U; j < i - 1U; ++j)
                {
                    result.push_back(array3[j]);
                }
            }

            return result;
        }

        // ── File I/O ─────────────────────────────────────────

        void KeyValueStorage::LoadFromFile()
        {
            std::ifstream file(mFilePath);
            if (!file.is_open())
            {
                return;
            }

            mData.clear();
            std::string line;
            while (std::getline(file, line))
            {
                std::size_t eqPos = line.find('=');
                if (eqPos == std::string::npos || eqPos == 0U)
                {
                    continue;
                }

                std::string key = line.substr(0U, eqPos);
                std::string encodedValue = line.substr(eqPos + 1U);
                mData[key] = DecodeBase64(encodedValue);
            }

            mCommittedData = mData;
        }

        void KeyValueStorage::SaveToFile() const
        {
            // Atomic write: write to tmp file, then rename
            std::string tmpPath = mFilePath + ".tmp";

            {
                std::ofstream file(tmpPath);
                if (!file.is_open())
                {
                    return;
                }

                for (const auto &kv : mData)
                {
                    file << kv.first << "="
                         << EncodeBase64(kv.second) << "\n";
                }
            }

            std::rename(tmpPath.c_str(), mFilePath.c_str());
        }

        // ── Public API ───────────────────────────────────────

        KeyValueStorage::KeyValueStorage(const std::string &filePath)
            : mFilePath{filePath}
        {
            LoadFromFile();
        }

        core::Result<std::string> KeyValueStorage::GetStringValue(
            const std::string &key) const
        {
            auto it = mData.find(key);
            if (it == mData.end())
            {
                return core::Result<std::string>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            const auto &bytes = it->second;
            return core::Result<std::string>::FromValue(
                std::string(bytes.begin(), bytes.end()));
        }

        core::Result<void> KeyValueStorage::SetStringValue(
            const std::string &key, const std::string &value)
        {
            mData[key] = std::vector<std::uint8_t>(
                value.begin(), value.end());
            return core::Result<void>::FromValue();
        }

        core::Result<void> KeyValueStorage::RemoveKey(
            const std::string &key)
        {
            auto it = mData.find(key);
            if (it == mData.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(PerErrc::kKeyNotFound));
            }

            mData.erase(it);
            return core::Result<void>::FromValue();
        }

        bool KeyValueStorage::HasKey(const std::string &key) const
        {
            return mData.find(key) != mData.end();
        }

        core::Result<std::vector<std::string>>
        KeyValueStorage::GetAllKeys() const
        {
            std::vector<std::string> keys;
            keys.reserve(mData.size());
            for (const auto &kv : mData)
            {
                keys.push_back(kv.first);
            }
            return core::Result<std::vector<std::string>>::FromValue(
                std::move(keys));
        }

        core::Result<void> KeyValueStorage::SyncToStorage()
        {
            SaveToFile();
            mCommittedData = mData;
            return core::Result<void>::FromValue();
        }

        void KeyValueStorage::DiscardPendingChanges()
        {
            mData = mCommittedData;
        }
    }
}
