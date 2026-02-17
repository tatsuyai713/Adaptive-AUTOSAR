/// @file src/ara/core/variant.h
/// @brief ara::core::Variant — AUTOSAR AP SWS_CORE type-safe discriminated union.
/// @details Provides ara::core::Variant<Types...> as a C++14-compatible type-safe
///          union, conforming to the AUTOSAR Adaptive Platform Core specification.
///
///          For C++17 environments, std::variant is equivalent. This implementation
///          provides a compatible subset for C++14.
///
///          Supported operations:
///          - Construction from any alternative type
///          - ara::core::Get<T>(variant) — extract value (throws if wrong type)
///          - ara::core::GetIf<T>(variant) — extract pointer (nullptr if wrong type)
///          - ara::core::HoldsAlternative<T>(variant) — type query
///          - ara::core::Visit(visitor, variant) — visitor pattern
///          - In-place construction with ara::core::InPlaceType<T>
///
///          This file is part of the Adaptive AUTOSAR educational implementation.

#ifndef ARA_CORE_VARIANT_H
#define ARA_CORE_VARIANT_H

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <new>

namespace ara
{
    namespace core
    {
        // -----------------------------------------------------------------------
        // Helper: in-place construction tag
        // -----------------------------------------------------------------------
        template <typename T>
        struct InPlaceType {};

        // -----------------------------------------------------------------------
        // Helper: index of T in type pack
        // -----------------------------------------------------------------------
        namespace detail
        {
            template <typename T, typename... Types>
            struct TypeIndex;

            template <typename T, typename First, typename... Rest>
            struct TypeIndex<T, First, Rest...>
            {
                static constexpr std::size_t value =
                    std::is_same<T, First>::value
                        ? 0
                        : 1 + TypeIndex<T, Rest...>::value;
            };

            template <typename T>
            struct TypeIndex<T>
            {
                // T not found — large sentinel value will cause compile error
                static constexpr std::size_t value = static_cast<std::size_t>(-1);
            };

            // Max size/align of a type pack
            template <typename... Types>
            struct MaxSizeOf;

            template <typename T, typename... Rest>
            struct MaxSizeOf<T, Rest...>
            {
                static constexpr std::size_t value =
                    sizeof(T) > MaxSizeOf<Rest...>::value
                        ? sizeof(T)
                        : MaxSizeOf<Rest...>::value;
            };

            template <>
            struct MaxSizeOf<>
            {
                static constexpr std::size_t value = 0;
            };

            template <typename... Types>
            struct MaxAlignOf;

            template <typename T, typename... Rest>
            struct MaxAlignOf<T, Rest...>
            {
                static constexpr std::size_t value =
                    alignof(T) > MaxAlignOf<Rest...>::value
                        ? alignof(T)
                        : MaxAlignOf<Rest...>::value;
            };

            template <>
            struct MaxAlignOf<>
            {
                static constexpr std::size_t value = 1;
            };

            // Invoke destructor by index
            template <std::size_t Idx, typename... Types>
            struct Destroyer;

            template <std::size_t Idx, typename First, typename... Rest>
            struct Destroyer<Idx, First, Rest...>
            {
                static void destroy(std::size_t typeIndex, void *storage) noexcept
                {
                    if (typeIndex == Idx)
                    {
                        reinterpret_cast<First *>(storage)->~First();
                    }
                    else
                    {
                        Destroyer<Idx + 1, Rest...>::destroy(typeIndex, storage);
                    }
                }
            };

            template <std::size_t Idx>
            struct Destroyer<Idx>
            {
                static void destroy(std::size_t, void *) noexcept {}
            };

            // Copy by index
            template <std::size_t Idx, typename... Types>
            struct Copier;

            template <std::size_t Idx, typename First, typename... Rest>
            struct Copier<Idx, First, Rest...>
            {
                static void copy(std::size_t typeIndex,
                                 const void *src, void *dst)
                {
                    if (typeIndex == Idx)
                    {
                        new (dst) First(*reinterpret_cast<const First *>(src));
                    }
                    else
                    {
                        Copier<Idx + 1, Rest...>::copy(typeIndex, src, dst);
                    }
                }

                static void move(std::size_t typeIndex, void *src, void *dst)
                {
                    if (typeIndex == Idx)
                    {
                        new (dst) First(std::move(*reinterpret_cast<First *>(src)));
                    }
                    else
                    {
                        Copier<Idx + 1, Rest...>::move(typeIndex, src, dst);
                    }
                }
            };

            template <std::size_t Idx>
            struct Copier<Idx>
            {
                static void copy(std::size_t, const void *, void *) {}
                static void move(std::size_t, void *, void *) {}
            };

            // Visitor dispatch by index
            template <std::size_t Idx, typename Visitor, typename... Types>
            struct Visitor;

            template <std::size_t Idx, typename VisitorT, typename First, typename... Rest>
            struct Visitor<Idx, VisitorT, First, Rest...>
            {
                static void visit(std::size_t typeIndex, VisitorT &&visitor, void *storage)
                {
                    if (typeIndex == Idx)
                    {
                        visitor(*reinterpret_cast<First *>(storage));
                    }
                    else
                    {
                        Visitor<Idx + 1, VisitorT, Rest...>::visit(
                            typeIndex, std::forward<VisitorT>(visitor), storage);
                    }
                }
            };

            template <std::size_t Idx, typename VisitorT>
            struct Visitor<Idx, VisitorT>
            {
                static void visit(std::size_t, VisitorT &&, void *) {}
            };

        } // namespace detail

        // -----------------------------------------------------------------------
        // Sentinel value for "valueless" state
        // -----------------------------------------------------------------------
        static constexpr std::size_t variant_npos = static_cast<std::size_t>(-1);

        // -----------------------------------------------------------------------
        // Variant<Types...>
        // -----------------------------------------------------------------------
        /// @brief Type-safe discriminated union (AUTOSAR SWS_CORE_01601).
        /// @tparam Types Alternative types. Must be copy-constructible.
        /// @details Stores exactly one value of one of the alternative types.
        ///          The active type is tracked by an index.
        ///
        ///          @code
        ///          ara::core::Variant<int, float, ara::core::String> v;
        ///          v = 42;
        ///          if (ara::core::HoldsAlternative<int>(v)) {
        ///              int x = ara::core::Get<int>(v); // 42
        ///          }
        ///
        ///          v = ara::core::String("hello");
        ///          ara::core::Visit([](auto& val) { /* ... */ }, v);
        ///          @endcode
        template <typename... Types>
        class Variant
        {
        public:
            static constexpr std::size_t cTypeCount = sizeof...(Types);

            // Default constructor — constructs the first alternative
            Variant() : mTypeIndex{0}
            {
                using FirstType = typename std::tuple_element<0, std::tuple<Types...>>::type;
                new (&mStorage) FirstType();
            }

            // Copy constructor
            Variant(const Variant &other) : mTypeIndex{other.mTypeIndex}
            {
                if (mTypeIndex != variant_npos)
                {
                    detail::Copier<0, Types...>::copy(mTypeIndex,
                                                     &other.mStorage, &mStorage);
                }
            }

            // Move constructor
            Variant(Variant &&other) noexcept : mTypeIndex{other.mTypeIndex}
            {
                if (mTypeIndex != variant_npos)
                {
                    detail::Copier<0, Types...>::move(mTypeIndex,
                                                     &other.mStorage, &mStorage);
                }
                other.mTypeIndex = variant_npos;
            }

            // Construct from a value of one of the alternative types
            template <typename T,
                      typename = typename std::enable_if<
                          (detail::TypeIndex<typename std::decay<T>::type, Types...>::value
                           < sizeof...(Types))>::type>
            Variant(T &&value)
                : mTypeIndex{detail::TypeIndex<typename std::decay<T>::type, Types...>::value}
            {
                using DecayT = typename std::decay<T>::type;
                new (&mStorage) DecayT(std::forward<T>(value));
            }

            // In-place construction
            template <typename T, typename... Args>
            explicit Variant(InPlaceType<T>, Args &&... args)
                : mTypeIndex{detail::TypeIndex<T, Types...>::value}
            {
                new (&mStorage) T(std::forward<Args>(args)...);
            }

            // Destructor
            ~Variant()
            {
                destroyActive();
            }

            // Copy assignment
            Variant &operator=(const Variant &other)
            {
                if (this != &other)
                {
                    destroyActive();
                    mTypeIndex = other.mTypeIndex;
                    if (mTypeIndex != variant_npos)
                    {
                        detail::Copier<0, Types...>::copy(mTypeIndex,
                                                         &other.mStorage, &mStorage);
                    }
                }
                return *this;
            }

            // Move assignment
            Variant &operator=(Variant &&other) noexcept
            {
                if (this != &other)
                {
                    destroyActive();
                    mTypeIndex = other.mTypeIndex;
                    if (mTypeIndex != variant_npos)
                    {
                        detail::Copier<0, Types...>::move(mTypeIndex,
                                                         &other.mStorage, &mStorage);
                    }
                    other.mTypeIndex = variant_npos;
                }
                return *this;
            }

            // Value assignment
            template <typename T,
                      typename = typename std::enable_if<
                          (detail::TypeIndex<typename std::decay<T>::type, Types...>::value
                           < sizeof...(Types))>::type>
            Variant &operator=(T &&value)
            {
                using DecayT = typename std::decay<T>::type;
                destroyActive();
                mTypeIndex = detail::TypeIndex<DecayT, Types...>::value;
                new (&mStorage) DecayT(std::forward<T>(value));
                return *this;
            }

            /// @brief Return the index of the active alternative.
            /// @returns 0-based index, or variant_npos if valueless.
            std::size_t index() const noexcept { return mTypeIndex; }

            /// @brief Return true if the variant has no value (moved-from state).
            bool valueless_by_exception() const noexcept
            {
                return mTypeIndex == variant_npos;
            }

            // Internal access (used by Get/GetIf/HoldsAlternative/Visit friends)
            void *storage() noexcept { return &mStorage; }
            const void *storage() const noexcept { return &mStorage; }

        private:
            using StorageType = typename std::aligned_storage<
                detail::MaxSizeOf<Types...>::value,
                detail::MaxAlignOf<Types...>::value>::type;

            StorageType mStorage;
            std::size_t mTypeIndex{variant_npos};

            void destroyActive() noexcept
            {
                if (mTypeIndex != variant_npos)
                {
                    detail::Destroyer<0, Types...>::destroy(mTypeIndex, &mStorage);
                }
            }
        };

        // -----------------------------------------------------------------------
        // HoldsAlternative<T>(variant)
        // -----------------------------------------------------------------------
        /// @brief Check if a Variant holds the alternative type T.
        template <typename T, typename... Types>
        bool HoldsAlternative(const Variant<Types...> &v) noexcept
        {
            return v.index() == detail::TypeIndex<T, Types...>::value;
        }

        // -----------------------------------------------------------------------
        // Get<T>(variant) — throws std::bad_cast if wrong type
        // -----------------------------------------------------------------------
        /// @brief Extract the value of type T from the variant.
        /// @throws std::bad_cast if the variant does not hold T.
        template <typename T, typename... Types>
        T &Get(Variant<Types...> &v)
        {
            if (!HoldsAlternative<T>(v))
            {
                throw std::bad_cast();
            }
            return *reinterpret_cast<T *>(v.storage());
        }

        template <typename T, typename... Types>
        const T &Get(const Variant<Types...> &v)
        {
            if (!HoldsAlternative<T>(v))
            {
                throw std::bad_cast();
            }
            return *reinterpret_cast<const T *>(v.storage());
        }

        template <typename T, typename... Types>
        T &&Get(Variant<Types...> &&v)
        {
            if (!HoldsAlternative<T>(v))
            {
                throw std::bad_cast();
            }
            return std::move(*reinterpret_cast<T *>(v.storage()));
        }

        // -----------------------------------------------------------------------
        // GetIf<T>(variant*) — returns nullptr if wrong type
        // -----------------------------------------------------------------------
        /// @brief Return a pointer to the value of type T, or nullptr if wrong type.
        template <typename T, typename... Types>
        T *GetIf(Variant<Types...> *v) noexcept
        {
            if (v == nullptr || !HoldsAlternative<T>(*v)) return nullptr;
            return reinterpret_cast<T *>(v->storage());
        }

        template <typename T, typename... Types>
        const T *GetIf(const Variant<Types...> *v) noexcept
        {
            if (v == nullptr || !HoldsAlternative<T>(*v)) return nullptr;
            return reinterpret_cast<const T *>(v->storage());
        }

        // -----------------------------------------------------------------------
        // Visit(visitor, variant) — visitor pattern
        // -----------------------------------------------------------------------
        /// @brief Apply a visitor callable to the active alternative.
        /// @param visitor Callable accepting any of the alternative types (overload set / generic lambda).
        /// @param v The variant to visit.
        template <typename Visitor, typename... Types>
        void Visit(Visitor &&visitor, Variant<Types...> &v)
        {
            detail::Visitor<0, Visitor, Types...>::visit(
                v.index(), std::forward<Visitor>(visitor), v.storage());
        }

        template <typename Visitor, typename... Types>
        void Visit(Visitor &&visitor, const Variant<Types...> &v)
        {
            // const_cast is safe here: visitor receives const ref via type dispatch
            detail::Visitor<0, Visitor, Types...>::visit(
                v.index(), std::forward<Visitor>(visitor),
                const_cast<void *>(v.storage()));
        }

    } // namespace core
} // namespace ara

#endif // ARA_CORE_VARIANT_H
