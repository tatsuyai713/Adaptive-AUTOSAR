// This source is scanned by autosar-generate-comm-manifest during build.
// It intentionally mirrors app-level endpoint usage declarations.

namespace sample
{
namespace transport
{
struct VehicleStatusFrame;
} // namespace transport
} // namespace sample

void declare_switchable_pubsub_usage()
{
    // The generator detects create_publisher/create_subscription patterns.
    // This file is not linked into runtime binaries.
    auto pub = create_publisher<sample::transport::VehicleStatusFrame>(
        "/sample/switchable_status",
        10);
    auto sub = create_subscription<sample::transport::VehicleStatusFrame>(
        "/sample/switchable_status",
        10,
        nullptr);

    (void)pub;
    (void)sub;
}
