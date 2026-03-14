/// @file user_apps/src/apps/samples/01_typed_method_rpc.cpp
/// @brief Sample: typed request-reply RPC using ProxyMethod and SkeletonMethod.
///
/// Demonstrates:
///   - SkeletonMethod<R(Args...)>  — register a typed handler on the server side
///   - ProxyMethod<R(Args...)>     — call the remote method from the client side
///   - Multi-argument serialization (two uint32_t args)
///   - ara::core::Future / Promise for async result retrieval
///   - Error propagation through the Future chain
///
/// Expected output:
///   [rpc] add(10, 32) = 42
///   [rpc] add(0, 0) = 0
///   [rpc] divide(100, 4) = 25
///   [rpc] divide by zero returns error: ...
///   [rpc] skeleton received log: "hello from proxy"
///   [rpc] all assertions passed

#include <cassert>
#include <iostream>
#include <utility>
#include "in_process_bindings.h"
#include "ara/com/method.h"
#include "ara/com/serialization.h"
#include "ara/core/promise.h"

int main()
{
    using namespace ara;
    using namespace ara::com;

    // ── Example 1: integer addition ──────────────────────────────────────────

    auto addPair      = sample::MakeMethodPair();
    auto proxyAddBind = std::move(addPair.first);
    auto skelAddBind  = std::move(addPair.second);

    SkeletonMethod<std::uint32_t(std::uint32_t, std::uint32_t)> skelAdd{
        std::move(skelAddBind)};

    auto setResult = skelAdd.SetHandler(
        [](const std::uint32_t &a, const std::uint32_t &b)
            -> core::Future<std::uint32_t>
        {
            core::Promise<std::uint32_t> p;
            p.set_value(a + b);
            return p.get_future();
        });
    assert(setResult.HasValue());

    ProxyMethod<std::uint32_t(std::uint32_t, std::uint32_t)> proxyAdd{
        std::move(proxyAddBind)};

    {
        auto result = proxyAdd(10U, 32U).GetResult();
        assert(result.HasValue() && result.Value() == 42U);
        std::cout << "[rpc] add(10, 32) = " << result.Value() << "\n";
    }
    {
        auto result = proxyAdd(0U, 0U).GetResult();
        assert(result.HasValue() && result.Value() == 0U);
        std::cout << "[rpc] add(0, 0) = " << result.Value() << "\n";
    }

    // ── Example 2: division with error on zero denominator ───────────────────

    auto divPair      = sample::MakeMethodPair();
    auto proxyDivBind = std::move(divPair.first);
    auto skelDivBind  = std::move(divPair.second);

    SkeletonMethod<std::uint32_t(std::uint32_t, std::uint32_t)> skelDiv{
        std::move(skelDivBind)};
    skelDiv.SetHandler(
        [](const std::uint32_t &n, const std::uint32_t &d)
            -> core::Future<std::uint32_t>
        {
            core::Promise<std::uint32_t> p;
            if (d == 0U)
                p.SetError(MakeErrorCode(ComErrc::kFieldValueIsNotValid));
            else
                p.set_value(n / d);
            return p.get_future();
        });

    ProxyMethod<std::uint32_t(std::uint32_t, std::uint32_t)> proxyDiv{
        std::move(proxyDivBind)};

    {
        auto result = proxyDiv(100U, 4U).GetResult();
        assert(result.HasValue() && result.Value() == 25U);
        std::cout << "[rpc] divide(100, 4) = " << result.Value() << "\n";
    }
    {
        auto result = proxyDiv(100U, 0U).GetResult();
        assert(!result.HasValue());
        std::cout << "[rpc] divide by zero returns error: "
                  << result.Error().Message() << "\n";
    }

    // ── Example 3: void-return method ────────────────────────────────────────

    auto logPair      = sample::MakeMethodPair();
    auto proxyLogBind = std::move(logPair.first);
    auto skelLogBind  = std::move(logPair.second);

    std::string lastMsg;
    SkeletonMethod<void(std::string)> skelLog{std::move(skelLogBind)};
    skelLog.SetHandler(
        [&](const std::string &msg) -> core::Future<void>
        {
            lastMsg = msg;
            core::Promise<void> p;
            p.set_value();
            return p.get_future();
        });

    ProxyMethod<void(std::string)> proxyLog{std::move(proxyLogBind)};
    assert(proxyLog(std::string{"hello from proxy"}).GetResult().HasValue());
    assert(lastMsg == "hello from proxy");
    std::cout << "[rpc] skeleton received log: \"" << lastMsg << "\"\n";

    std::cout << "[rpc] all assertions passed\n";
    return 0;
}
