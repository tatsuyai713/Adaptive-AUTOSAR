/// @file src/ara/com/sample_ptr.h
/// @brief Declarations for sample ptr.
/// @details This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_COM_SAMPLE_PTR_H
#define ARA_COM_SAMPLE_PTR_H

#include <functional>
#include <memory>
#include <utility>

namespace ara
{
    namespace com
    {
        /// @brief Smart pointer for accessing received event samples (proxy side).
        ///        Per AUTOSAR AP SWS_CM_00306, provides const access to received data.
        ///        For zero-copy paths, the custom deleter returns the buffer to the
        ///        underlying middleware (e.g. iceoryx).
        template <typename T>
        using SamplePtr = std::unique_ptr<const T>;

        /// @brief Smart pointer for skeleton-side sample allocation.
        ///        Per AUTOSAR AP SWS_CM_00308, allows in-place construction of
        ///        samples in shared memory (zero-copy) or on the heap (copy path).
        template <typename T>
        class SampleAllocateePtr
        {
        private:
            std::unique_ptr<T, std::function<void(T *)>> mPtr;

        public:
            /// @brief Creates an empty allocatee pointer.
            SampleAllocateePtr() noexcept : mPtr{nullptr} {}

            /// @brief Takes ownership of an allocated sample and deleter.
            /// @param ptr Sample unique pointer prepared by binding/runtime.
            explicit SampleAllocateePtr(
                std::unique_ptr<T, std::function<void(T *)>> ptr) noexcept
                : mPtr{std::move(ptr)}
            {
            }

            SampleAllocateePtr(const SampleAllocateePtr &) = delete;
            SampleAllocateePtr &operator=(const SampleAllocateePtr &) = delete;

            SampleAllocateePtr(SampleAllocateePtr &&other) noexcept = default;
            SampleAllocateePtr &operator=(SampleAllocateePtr &&other) noexcept = default;

            ~SampleAllocateePtr() noexcept = default;

            /// @brief Pointer-style access to the managed sample.
            T *operator->() noexcept { return mPtr.get(); }
            /// @brief Const pointer-style access to the managed sample.
            const T *operator->() const noexcept { return mPtr.get(); }
            /// @brief Dereferences the managed sample.
            T &operator*() noexcept { return *mPtr; }
            /// @brief Const dereference of the managed sample.
            const T &operator*() const noexcept { return *mPtr; }

            /// @brief Returns the raw sample pointer.
            T *Get() noexcept { return mPtr.get(); }
            /// @brief Returns the raw const sample pointer.
            const T *Get() const noexcept { return mPtr.get(); }

            /// @brief Checks whether a sample is currently owned.
            explicit operator bool() const noexcept
            {
                return mPtr != nullptr;
            }

            /// @brief Release ownership of the managed pointer.
            ///        Used internally when publishing the sample.
            T *Release() noexcept
            {
                return mPtr.release();
            }

            /// @brief Swaps ownership with another allocatee pointer.
            /// @param other Other object to swap with.
            void Swap(SampleAllocateePtr &other) noexcept
            {
                mPtr.swap(other.mPtr);
            }
        };
    }
}

#endif
