/// @file user_apps/src/apps/samples/02_fire_and_forget.cpp
/// @brief Sample: fire-and-forget one-way method calls.
///
/// Demonstrates:
///   - ProxyFireAndForgetMethod<Args...>    — send args, never wait for a reply
///   - SkeletonFireAndForgetMethod<Args...> — handle incoming args, no response
///   - Difference from void-return ProxyMethod: the caller never suspends
///   - In-process binding (no real transport required)
///
/// AUTOSAR AP reference: SWS_CM_00196
///
/// Expected output:
///   [faf] notified: event_id=1001 severity=2 text=overheat
///   [faf] notified: event_id=1002 severity=1 text=low_battery
///   [faf] total notifications received: 2
///   [faf] all assertions passed

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include "in_process_bindings.h"
#include "ara/com/method.h"
#include "ara/com/serialization.h"

int main()
{
    using namespace ara::com;

    // ── Setup: create one shared channel ─────────────────────────────────────

    auto methodPair = sample::MakeMethodPair();
    auto proxyBind = std::move(methodPair.first);
    auto skelBind  = std::move(methodPair.second);

    // ── Skeleton side: register a void handler ────────────────────────────────
    //
    //   SkeletonFireAndForgetMethod<EventId, Severity, Message>
    //   The handler receives typed arguments but returns nothing.
    //   The binding sends an empty success payload to maintain transport framing,
    //   but the caller (proxy) discards it.

    struct Notification
    {
        std::uint32_t eventId;
        std::uint8_t severity;
        std::string text;
    };

    std::vector<Notification> received;

    SkeletonFireAndForgetMethod<std::uint32_t, std::uint8_t, std::string> skeleton{
        std::move(skelBind)};

    auto regResult = skeleton.SetHandler(
        [&](const std::uint32_t &eventId,
            const std::uint8_t &severity,
            const std::string &text)
        {
            received.push_back({eventId, severity, text});
            std::cout << "[faf] notified: event_id=" << eventId
                      << " severity=" << static_cast<int>(severity)
                      << " text=" << text << "\n";
        });

    assert(regResult.HasValue());

    // ── Proxy side: fire events without waiting for any response ──────────────
    //
    //   ProxyFireAndForgetMethod<EventId, Severity, Message>
    //   operator() returns void — there is no Future to wait on.

    ProxyFireAndForgetMethod<std::uint32_t, std::uint8_t, std::string> proxy{
        std::move(proxyBind)};

    proxy(1001U, std::uint8_t{2}, std::string{"overheat"});
    proxy(1002U, std::uint8_t{1}, std::string{"low_battery"});

    // ── Verify ────────────────────────────────────────────────────────────────

    assert(received.size() == 2U);
    assert(received[0].eventId == 1001U);
    assert(received[0].severity == 2U);
    assert(received[0].text == "overheat");
    assert(received[1].eventId == 1002U);
    assert(received[1].text == "low_battery");

    std::cout << "[faf] total notifications received: " << received.size() << "\n";

    // ── UnsetHandler — removes the registered lambda ──────────────────────────

    skeleton.UnsetHandler();

    // After unset, incoming requests return an error internally (no handler).
    // The proxy side still does not block — it simply discards the error response.
    proxy(9999U, std::uint8_t{0}, std::string{"ignored"});
    // 'received' still has 2 elements (the third call had no handler).
    assert(received.size() == 2U);

    std::cout << "[faf] all assertions passed\n";
    return 0;
}
