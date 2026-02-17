/// @file src/ara/crypto/key_storage_provider.cpp
/// @brief Implementation for key storage provider.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#include "./key_storage_provider.h"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <dirent.h>

namespace ara
{
    namespace crypto
    {
        core::Result<void> KeyStorageProvider::CreateSlot(
            const KeySlotMetadata &metadata)
        {
            if (metadata.SlotId.empty())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kInvalidArgument));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            if (mSlots.find(metadata.SlotId) != mSlots.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kSlotAlreadyExists));
            }

            mSlots[metadata.SlotId] =
                std::unique_ptr<KeySlot>(new KeySlot{metadata, {}});
            return core::Result<void>::FromValue();
        }

        core::Result<void> KeyStorageProvider::DeleteSlot(
            const std::string &slotId)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mSlots.find(slotId);
            if (_it == mSlots.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kSlotNotFound));
            }

            mSlots.erase(_it);
            return core::Result<void>::FromValue();
        }

        core::Result<KeySlot *> KeyStorageProvider::GetSlot(
            const std::string &slotId)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mSlots.find(slotId);
            if (_it == mSlots.end())
            {
                return core::Result<KeySlot *>::FromError(
                    MakeErrorCode(CryptoErrc::kSlotNotFound));
            }

            return core::Result<KeySlot *>::FromValue(_it->second.get());
        }

        std::vector<std::string> KeyStorageProvider::ListSlotIds() const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            std::vector<std::string> _ids;
            _ids.reserve(mSlots.size());
            for (const auto &_pair : mSlots)
            {
                _ids.push_back(_pair.first);
            }
            return _ids;
        }

        core::Result<void> KeyStorageProvider::StoreKey(
            const std::string &slotId,
            const std::vector<std::uint8_t> &keyMaterial)
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            auto _it = mSlots.find(slotId);
            if (_it == mSlots.end())
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kSlotNotFound));
            }

            return _it->second->Update(keyMaterial);
        }

        core::Result<void> KeyStorageProvider::SaveToDirectory(
            const std::string &dirPath) const
        {
            std::lock_guard<std::mutex> _lock{mMutex};

            // Ensure directory exists
            mkdir(dirPath.c_str(), 0700);

            for (const auto &_pair : mSlots)
            {
                const auto &_id = _pair.first;
                const auto &_slot = *_pair.second;
                const auto &_meta = _slot.GetMetadata();

                std::string _filePath = dirPath + "/" + _id + ".keyslot";
                std::ofstream _ofs{_filePath, std::ios::binary};
                if (!_ofs.is_open())
                {
                    return core::Result<void>::FromError(
                        MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
                }

                // Header: type|sizeBits|exportable
                _ofs << static_cast<std::uint32_t>(_meta.Type) << "|"
                     << _meta.KeySizeBits << "|"
                     << (_meta.Exportable ? 1 : 0) << "\n";

                // Key material as hex
                if (_meta.Exportable)
                {
                    auto _result = _slot.GetKeyMaterial();
                    if (_result.HasValue())
                    {
                        const auto &_data = _result.Value();
                        for (auto _byte : _data)
                        {
                            char _hex[3];
                            snprintf(_hex, sizeof(_hex), "%02x", _byte);
                            _ofs << _hex;
                        }
                    }
                }
                _ofs << "\n";
            }

            return core::Result<void>::FromValue();
        }

        core::Result<void> KeyStorageProvider::LoadFromDirectory(
            const std::string &dirPath)
        {
            DIR *_dir = opendir(dirPath.c_str());
            if (_dir == nullptr)
            {
                return core::Result<void>::FromError(
                    MakeErrorCode(CryptoErrc::kCryptoProviderFailure));
            }

            std::lock_guard<std::mutex> _lock{mMutex};

            struct dirent *_entry;
            while ((_entry = readdir(_dir)) != nullptr)
            {
                std::string _name{_entry->d_name};
                const std::string _suffix{".keyslot"};
                if (_name.size() <= _suffix.size() ||
                    _name.compare(_name.size() - _suffix.size(),
                                  _suffix.size(), _suffix) != 0)
                {
                    continue;
                }

                std::string _slotId = _name.substr(0, _name.size() - _suffix.size());
                std::string _filePath = dirPath + "/" + _name;
                std::ifstream _ifs{_filePath, std::ios::binary};
                if (!_ifs.is_open())
                {
                    continue;
                }

                std::string _headerLine;
                if (!std::getline(_ifs, _headerLine))
                {
                    continue;
                }

                // Parse header: type|sizeBits|exportable
                std::istringstream _hss{_headerLine};
                std::uint32_t _typeVal{0U};
                std::uint32_t _sizeBits{0U};
                int _exportable{0};
                char _sep;
                _hss >> _typeVal >> _sep >> _sizeBits >> _sep >> _exportable;

                KeySlotMetadata _meta;
                _meta.SlotId = _slotId;
                _meta.Type = static_cast<KeySlotType>(_typeVal);
                _meta.KeySizeBits = _sizeBits;
                _meta.Exportable = (_exportable != 0);

                // Parse hex key material
                std::string _hexLine;
                std::getline(_ifs, _hexLine);
                std::vector<std::uint8_t> _keyMaterial;
                for (std::size_t _i = 0; _i + 1 < _hexLine.size(); _i += 2)
                {
                    unsigned int _byte{0U};
                    std::istringstream _bss{_hexLine.substr(_i, 2)};
                    _bss >> std::hex >> _byte;
                    _keyMaterial.push_back(static_cast<std::uint8_t>(_byte));
                }

                mSlots.erase(_slotId);
                mSlots[_slotId] =
                    std::unique_ptr<KeySlot>(new KeySlot{_meta, _keyMaterial});
            }

            closedir(_dir);
            return core::Result<void>::FromValue();
        }
    }
}
