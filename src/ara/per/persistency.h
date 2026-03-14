/// @file src/ara/per/persistency.h
/// @brief Declarations for persistency.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef PERSISTENCY_H
#define PERSISTENCY_H

#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./shared_handle.h"
#include "./key_value_storage.h"
#include "./file_storage.h"
#include <cstdint>
#include <functional>
#include <string>

namespace ara
{
    namespace per
    {
        /// @brief Open a key-value storage per AUTOSAR AP SWS_PER_00001
        /// @param specifier Instance specifier identifying the storage
        /// @returns Shared handle to the key-value storage
        core::Result<SharedHandle<KeyValueStorage>> OpenKeyValueStorage(
            const core::InstanceSpecifier &specifier);

        /// @brief Open a file storage per AUTOSAR AP SWS_PER_00116
        /// @param specifier Instance specifier identifying the storage
        /// @returns Shared handle to the file storage
        core::Result<SharedHandle<FileStorage>> OpenFileStorage(
            const core::InstanceSpecifier &specifier);

        /// @brief Recover a key-value storage from backup
        /// @param specifier Instance specifier identifying the storage
        /// @returns Void Result on success
        core::Result<void> RecoverKeyValueStorage(
            const core::InstanceSpecifier &specifier);

        /// @brief Reset (delete all data) a key-value storage
        /// @param specifier Instance specifier identifying the storage
        /// @returns Void Result on success
        core::Result<void> ResetKeyValueStorage(
            const core::InstanceSpecifier &specifier);

        /// @brief Recover a file storage from backup (SWS_PER_00116)
        /// @param specifier Instance specifier identifying the storage
        /// @returns Void Result on success
        core::Result<void> RecoverFileStorage(
            const core::InstanceSpecifier &specifier);

        /// @brief Reset (delete all files in) a file storage (SWS_PER_00117)
        /// @param specifier Instance specifier identifying the storage
        /// @returns Void Result on success
        core::Result<void> ResetFileStorage(
            const core::InstanceSpecifier &specifier);

        /// @brief Update persistency after software update activation (SWS_PER_00456)
        /// @details Called by UCM after software update to migrate storage data to
        ///          the new schema version. Implementations should handle schema
        ///          versioning and data migration here.
        /// @param specifier Instance specifier identifying the storage
        /// @returns Void Result on success
        core::Result<void> UpdatePersistency(
            const core::InstanceSpecifier &specifier);

        /// @brief Reset all persistent data for this application (SWS_PER_00457)
        /// @returns Void Result on success
        core::Result<void> ResetPersistency();

        /// @brief Get the current persistency data version (SWS_PER_00452).
        /// @param specifier Instance specifier identifying the storage
        /// @returns Version number on success, error otherwise
        core::Result<std::uint32_t> GetCurrentPersistencyVersion(
            const core::InstanceSpecifier &specifier);

        /// @brief Callback type for data type fault notifications (SWS_PER_00350).
        using DataTypeFaultHandler = std::function<void(
            const core::InstanceSpecifier &specifier,
            const std::string &key)>;

        /// @brief Register a handler for data type / deserialization faults (SWS_PER_00350).
        /// @param handler Callback invoked when a corrupt or incompatible value is detected.
        void RegisterDataTypeFaultHandler(DataTypeFaultHandler handler);
    }
}

#endif
