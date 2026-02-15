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
            SampleAllocateePtr() noexcept : mPtr{nullptr} {}

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

            T *operator->() noexcept { return mPtr.get(); }
            const T *operator->() const noexcept { return mPtr.get(); }
            T &operator*() noexcept { return *mPtr; }
            const T &operator*() const noexcept { return *mPtr; }

            T *Get() noexcept { return mPtr.get(); }
            const T *Get() const noexcept { return mPtr.get(); }

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

            /// @brief Swap with another SampleAllocateePtr
            void Swap(SampleAllocateePtr &other) noexcept
            {
                mPtr.swap(other.mPtr);
            }
        };
    }
}

#endif
