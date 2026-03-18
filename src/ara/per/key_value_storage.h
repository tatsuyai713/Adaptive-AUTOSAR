/// @file src/ara/per/key_value_storage.h
/// @brief Declarations for key value storage.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef KEY_VALUE_STORAGE_H
#define KEY_VALUE_STORAGE_H

#include <cstdint>
#include <cstring>
#include <functional>
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
        ///
        /// ### Change Observation
        /// Per-key or global observers are notified synchronously when a key
        /// is created, updated, or removed. Observers are called after the
        /// internal data map is updated, before the function returns.
        class KeyValueStorage
        {
        public:
            /// @brief Callback invoked with the key name when a key is mutated.
            using KeyObserverCallback = std::function<void(const std::string &key)>;

        private:
            std::string mFilePath;
            std::map<std::string, std::vector<std::uint8_t>> mData;
            std::map<std::string, std::vector<std::uint8_t>> mCommittedData;

            std::map<std::string, KeyObserverCallback> mKeyObservers;
            KeyObserverCallback mGlobalObserver;
            std::size_t mPendingChanges{0U};
            /// @brief Storage quota in bytes (0 = unlimited).
            std::uint64_t mQuotaBytes{0U};

            void LoadFromFile();
            void SaveToFile() const;

            static std::string EncodeBase64(
                const std::vector<std::uint8_t> &data);
            static std::vector<std::uint8_t> DecodeBase64(
                const std::string &encoded);

            void notifyObservers(const std::string &key) const;

        public:
            /// @brief Constructor
            /// @param filePath   Path to the key-value storage file.
            /// @param quotaBytes Maximum storage size in bytes (0 = unlimited).
            explicit KeyValueStorage(const std::string &filePath,
                                     std::uint64_t quotaBytes = 0U);

            ~KeyValueStorage() noexcept = default;

            KeyValueStorage(const KeyValueStorage &) = delete;
            KeyValueStorage &operator=(const KeyValueStorage &) = delete;

            // ----------------------------------------------------------------
            // Read operations
            // ----------------------------------------------------------------

            /// @brief Get a value by key (trivially copyable types)
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
            core::Result<std::string> GetStringValue(
                const std::string &key) const;

            // ----------------------------------------------------------------
            // Write operations
            // ----------------------------------------------------------------

            /// @brief Set a value by key (trivially copyable types)
            template <typename T>
            typename std::enable_if<
                std::is_trivially_copyable<T>::value,
                core::Result<void>>::type
            SetValue(const std::string &key, const T &value)
            {
                std::vector<std::uint8_t> bytes(sizeof(T));
                std::memcpy(bytes.data(), &value, sizeof(T));
                mData[key] = std::move(bytes);
                ++mPendingChanges;
                notifyObservers(key);
                return core::Result<void>::FromValue();
            }

            /// @brief Set a string value by key
            core::Result<void> SetStringValue(
                const std::string &key, const std::string &value);

            /// @brief Remove a key-value pair
            core::Result<void> RemoveKey(const std::string &key);

            // ----------------------------------------------------------------
            // Query operations
            // ----------------------------------------------------------------

            /// @brief Check if a key exists
            bool HasKey(const std::string &key) const;

            /// @brief Get all keys in the storage
            core::Result<std::vector<std::string>> GetAllKeys() const;

            /// @brief Get number of pending (uncommitted) changes since last sync.
            std::size_t GetPendingChangeCount() const noexcept;

            /// @brief Get the total byte size of all stored key-value pairs (SWS_PER_00409).
            /// @returns Total byte footprint of committed data.
            std::size_t GetCurrentStorageSize() const noexcept;

            /// @brief Get the storage quota for this KVS instance (SWS_PER_00408).
            /// @returns Maximum allowed storage size (0 = no quota configured).
            std::uint64_t GetStorageQuota() const noexcept;

            // ----------------------------------------------------------------
            // Persistence operations
            // ----------------------------------------------------------------

            /// @brief Persist all pending changes to storage
            core::Result<void> SyncToStorage();

            /// @brief Discard all pending (uncommitted) changes
            void DiscardPendingChanges();

            // ----------------------------------------------------------------
            // Observer registration
            // ----------------------------------------------------------------

            /// @brief Register a per-key observer for the given key.
            /// @param key      Key to observe.
            /// @param callback Callback invoked with the key name on change.
            void SetKeyObserver(const std::string &key,
                                KeyObserverCallback callback);

            /// @brief Remove the per-key observer for the given key.
            void RemoveKeyObserver(const std::string &key);

            /// @brief Register a global observer called on every key mutation.
            /// @param callback Callback; pass nullptr to clear.
            void SetGlobalObserver(KeyObserverCallback callback);

            /// @brief Remove the global observer.
            void ClearGlobalObserver() noexcept;

            // ----------------------------------------------------------------
            // Batch operations
            // ----------------------------------------------------------------

            /// @brief Set multiple key-value pairs in a single call.
            ///
            /// All entries are applied atomically to the in-memory map.
            /// Observers are notified for each key after all entries have been
            /// applied.
            /// @param entries  Map of key -> value pairs.
            template <typename T>
            typename std::enable_if<
                std::is_trivially_copyable<T>::value,
                core::Result<void>>::type
            SetValues(const std::map<std::string, T> &entries)
            {
                std::vector<std::string> changedKeys;
                changedKeys.reserve(entries.size());
                for (const auto &kv : entries)
                {
                    std::vector<std::uint8_t> bytes(sizeof(T));
                    std::memcpy(bytes.data(), &kv.second, sizeof(T));
                    mData[kv.first] = std::move(bytes);
                    ++mPendingChanges;
                    changedKeys.push_back(kv.first);
                }
                for (const auto &key : changedKeys)
                {
                    notifyObservers(key);
                }
                return core::Result<void>::FromValue();
            }

            /// @brief Set multiple string values in a single call.
            core::Result<void> SetStringValues(
                const std::map<std::string, std::string> &entries);

            /// @brief Remove multiple keys in a single call.
            ///
            /// Keys that do not exist are silently skipped.
            /// @param keys  List of keys to remove.
            /// @returns Number of keys actually removed.
            core::Result<std::size_t> RemoveKeys(
                const std::vector<std::string> &keys);

            /// @brief Get multiple values in a single call (trivially copyable).
            ///
            /// Keys not found in storage are omitted from the result.
            template <typename T>
            typename std::enable_if<
                std::is_trivially_copyable<T>::value,
                core::Result<std::map<std::string, T>>>::type
            GetValues(const std::vector<std::string> &keys) const
            {
                std::map<std::string, T> result;
                for (const auto &key : keys)
                {
                    auto it = mData.find(key);
                    if (it != mData.end() && it->second.size() >= sizeof(T))
                    {
                        T value;
                        std::memcpy(&value, it->second.data(), sizeof(T));
                        result[key] = value;
                    }
                }
                return core::Result<std::map<std::string, T>>::FromValue(
                    std::move(result));
            }

            /// @brief Get multiple string values in a single call.
            core::Result<std::map<std::string, std::string>> GetStringValues(
                const std::vector<std::string> &keys) const;
        };
    }
}

#endif
