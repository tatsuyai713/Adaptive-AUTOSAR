#ifndef KEY_VALUE_STORAGE_H
#define KEY_VALUE_STORAGE_H

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <vector>
#include "../core/result.h"
#include "./per_error_domain.h"
#include "./shared_handle.h"

namespace ara
{
    namespace per
    {
        /// @brief Key-value persistent storage per AUTOSAR AP SWS_PER
        ///        Stores typed values associated with string keys.
        ///        Changes are buffered in memory until SyncToStorage() is called.
        class KeyValueStorage
        {
        private:
            std::string mFilePath;
            std::map<std::string, std::vector<std::uint8_t>> mData;
            std::map<std::string, std::vector<std::uint8_t>> mCommittedData;

            void LoadFromFile();
            void SaveToFile() const;

            static std::string EncodeBase64(
                const std::vector<std::uint8_t> &data);
            static std::vector<std::uint8_t> DecodeBase64(
                const std::string &encoded);

        public:
            /// @brief Constructor
            /// @param filePath Path to the key-value storage file
            explicit KeyValueStorage(const std::string &filePath);

            ~KeyValueStorage() noexcept = default;

            KeyValueStorage(const KeyValueStorage &) = delete;
            KeyValueStorage &operator=(const KeyValueStorage &) = delete;

            /// @brief Get a value by key (trivially copyable types)
            /// @tparam T Value type (must be trivially copyable)
            /// @param key Key string
            /// @returns The stored value, or error if key not found
            template <typename T>
            typename std::enable_if<
                std::is_trivially_copyable<T>::value,
                core::Result<T>>::type
            GetValue(const std::string &key) const
            {
                auto it = mData.find(key);
                if (it == mData.end())
                {
                    return core::Result<T>::FromError(
                        MakeErrorCode(PerErrc::kKeyNotFound));
                }

                const auto &bytes = it->second;
                if (bytes.size() < sizeof(T))
                {
                    return core::Result<T>::FromError(
                        MakeErrorCode(PerErrc::kIntegrityCorrupted));
                }

                T value;
                std::memcpy(&value, bytes.data(), sizeof(T));
                return core::Result<T>::FromValue(std::move(value));
            }

            /// @brief Get a string value by key
            /// @param key Key string
            /// @returns The stored string, or error
            core::Result<std::string> GetStringValue(
                const std::string &key) const;

            /// @brief Set a value by key (trivially copyable types)
            /// @tparam T Value type (must be trivially copyable)
            /// @param key Key string
            /// @param value Value to store
            /// @returns Void Result on success
            template <typename T>
            typename std::enable_if<
                std::is_trivially_copyable<T>::value,
                core::Result<void>>::type
            SetValue(const std::string &key, const T &value)
            {
                std::vector<std::uint8_t> bytes(sizeof(T));
                std::memcpy(bytes.data(), &value, sizeof(T));
                mData[key] = std::move(bytes);
                return core::Result<void>::FromValue();
            }

            /// @brief Set a string value by key
            /// @param key Key string
            /// @param value String value to store
            /// @returns Void Result on success
            core::Result<void> SetStringValue(
                const std::string &key, const std::string &value);

            /// @brief Remove a key-value pair
            /// @param key Key string to remove
            /// @returns Void Result on success, error if key not found
            core::Result<void> RemoveKey(const std::string &key);

            /// @brief Check if a key exists
            /// @param key Key string
            /// @returns True if the key exists
            bool HasKey(const std::string &key) const;

            /// @brief Get all keys in the storage
            /// @returns Vector of key strings
            core::Result<std::vector<std::string>> GetAllKeys() const;

            /// @brief Persist all pending changes to storage
            /// @returns Void Result on success
            core::Result<void> SyncToStorage();

            /// @brief Discard all pending (uncommitted) changes
            void DiscardPendingChanges();
        };
    }
}

#endif
