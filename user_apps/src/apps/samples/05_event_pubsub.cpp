/// @file user_apps/src/apps/samples/05_event_pubsub.cpp
/// @brief Sample: ProxyEvent and SkeletonEvent typed publish-subscribe.
///
/// Demonstrates:
///   - SkeletonEvent<T>::Offer / StopOffer
///   - SkeletonEvent<T>::Send(const T&)     — copy-publish
///   - SkeletonEvent<T>::Allocate / Send(SampleAllocateePtr<T>) — zero-copy
///   - ProxyEvent<T>::Subscribe / Unsubscribe
///   - ProxyEvent<T>::SetReceiveHandler     — callback when new data arrives
///   - ProxyEvent<T>::GetNewSamples         — pull and process queued samples
///   - ProxyEvent<T>::GetFreeSampleCount    — remaining queue capacity
///   - ProxyEvent<T>::SetSubscriptionStateChangeHandler
///   - Serialization of a struct via Serializer<T>
///   - In-process bindings (no real transport required)
///
/// Expected output:
///   [pubsub] subscribed, free slots: 4
///   [pubsub] receive handler called (new data available)
///   [pubsub] sample[0]: speed=80 rpm=2000
///   [pubsub] sample[1]: speed=90 rpm=2200
///   [pubsub] sample[2]: speed=100 rpm=2400
///   [pubsub] zero-copy sample: speed=120 rpm=3000
///   [pubsub] all assertions passed

#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>
#include "in_process_bindings.h"
#include "ara/com/event.h"
#include "ara/com/serialization.h"

// ── Custom data type ──────────────────────────────────────────────────────────
// Must be trivially copyable for the default Serializer<T> specialization.

struct VehicleStatus
{
    std::uint16_t speedKph;
    std::uint16_t engineRpm;
};

static_assert(
    std::is_trivially_copyable<VehicleStatus>::value,
    "VehicleStatus must be trivially copyable for default Serializer<T>");

int main()
{
    using namespace ara::com;

    // ── Create connected binding pair ─────────────────────────────────────────

    auto [proxyBind, skelBind] = sample::MakeEventPair();

    SkeletonEvent<VehicleStatus> skeletonEvent{std::move(skelBind)};
    ProxyEvent<VehicleStatus> proxyEvent{std::move(proxyBind)};

    // ── Offer the event ───────────────────────────────────────────────────────

    auto offerResult = skeletonEvent.Offer();
    assert(offerResult.HasValue());

    // ── Subscribe with a sample queue capacity of 4 ───────────────────────────

    proxyEvent.Subscribe(4U);
    assert(proxyEvent.GetSubscriptionState() == SubscriptionState::kSubscribed);
    std::cout << "[pubsub] subscribed, free slots: "
              << proxyEvent.GetFreeSampleCount() << "\n";

    // ── Attach a receive handler ──────────────────────────────────────────────

    bool receiverCalled = false;
    proxyEvent.SetReceiveHandler([&]
    {
        receiverCalled = true;
        std::cout << "[pubsub] receive handler called (new data available)\n";
    });

    // ── Skeleton sends three samples (copy path) ──────────────────────────────

    skeletonEvent.Send(VehicleStatus{80U, 2000U});
    skeletonEvent.Send(VehicleStatus{90U, 2200U});
    skeletonEvent.Send(VehicleStatus{100U, 2400U});

    assert(receiverCalled);
    assert(proxyEvent.GetFreeSampleCount() == 1U); // 4 - 3 = 1

    // ── Retrieve samples with GetNewSamples ───────────────────────────────────

    std::vector<VehicleStatus> received;
    auto getResult = proxyEvent.GetNewSamples(
        [&](SamplePtr<VehicleStatus> sample)
        {
            received.push_back(*sample);
        });

    assert(getResult.HasValue());
    assert(getResult.Value() == 3U);
    assert(received.size() == 3U);

    for (std::size_t i = 0; i < received.size(); ++i)
    {
        std::cout << "[pubsub] sample[" << i << "]:"
                  << " speed=" << received[i].speedKph
                  << " rpm=" << received[i].engineRpm << "\n";
    }

    assert(received[0].speedKph == 80U);
    assert(received[1].speedKph == 90U);
    assert(received[2].speedKph == 100U);

    // Queue is now empty; free count is back to 4.
    assert(proxyEvent.GetFreeSampleCount() == 4U);

    // ── Zero-copy path: Allocate + Send(SampleAllocateePtr) ──────────────────

    auto allocResult = skeletonEvent.Allocate();
    assert(allocResult.HasValue());

    auto ptr = std::move(allocResult).Value();
    (*ptr).speedKph = 120U;
    (*ptr).engineRpm = 3000U;

    skeletonEvent.Send(std::move(ptr));

    std::vector<VehicleStatus> zcReceived;
    proxyEvent.GetNewSamples(
        [&](SamplePtr<VehicleStatus> sample)
        {
            zcReceived.push_back(*sample);
        });

    assert(zcReceived.size() == 1U);
    assert(zcReceived[0].speedKph == 120U);
    assert(zcReceived[0].engineRpm == 3000U);
    std::cout << "[pubsub] zero-copy sample: speed=" << zcReceived[0].speedKph
              << " rpm=" << zcReceived[0].engineRpm << "\n";

    // ── Unsubscribe ───────────────────────────────────────────────────────────

    proxyEvent.Unsubscribe();
    assert(proxyEvent.GetSubscriptionState() ==
           SubscriptionState::kNotSubscribed);

    // After unsubscribe, new sends are silently dropped (no subscriber).
    skeletonEvent.Send(VehicleStatus{999U, 9999U});
    // No crash; proxy queue is empty.

    // ── StopOffer ────────────────────────────────────────────────────────────

    skeletonEvent.StopOffer();

    std::cout << "[pubsub] all assertions passed\n";
    return 0;
}
