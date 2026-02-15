#ifndef PERSISTENCY_H
#define PERSISTENCY_H

#include "../core/instance_specifier.h"
#include "../core/result.h"
#include "./shared_handle.h"
#include "./key_value_storage.h"
#include "./file_storage.h"

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
    }
}

#endif
