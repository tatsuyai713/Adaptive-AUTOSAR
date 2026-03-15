/// @file test/ara/com/someip/someip_extended_test.cpp
/// @brief Unit tests for SOME/IP extended features:
///        Magic Cookie, TP Reassembly Manager, Session Handler,
///        and fire-and-forget RPC server support.

#include <gtest/gtest.h>
#include <thread>
#include <ara/com/someip/magic_cookie.h>
#include <ara/com/someip/tp_reassembly_manager.h>
#include <ara/com/someip/session_handler.h>

using namespace ara::com::someip;

namespace ara_com_someip_extended_test
{
    // ── Magic Cookie Tests ─────────────────────────────────────

    TEST(MagicCookieTest, ClientCookieSize)
    {
        auto cookie = GenerateClientMagicCookie();
        EXPECT_EQ(cookie.size(), 16U);
    }

    TEST(MagicCookieTest, ServerCookieSize)
    {
        auto cookie = GenerateServerMagicCookie();
        EXPECT_EQ(cookie.size(), 16U);
    }

    TEST(MagicCookieTest, ClientCookieMessageId)
    {
        auto cookie = GenerateClientMagicCookie();
        EXPECT_EQ(cookie[0], 0xDE);
        EXPECT_EQ(cookie[1], 0xAD);
        EXPECT_EQ(cookie[2], 0x00);
        EXPECT_EQ(cookie[3], 0x00);
    }

    TEST(MagicCookieTest, ServerCookieMessageId)
    {
        auto cookie = GenerateServerMagicCookie();
        EXPECT_EQ(cookie[0], 0xDE);
        EXPECT_EQ(cookie[1], 0xAD);
        EXPECT_EQ(cookie[2], 0x80);
        EXPECT_EQ(cookie[3], 0x00);
    }

    TEST(MagicCookieTest, IsClientMagicCookie)
    {
        auto cookie = GenerateClientMagicCookie();
        EXPECT_TRUE(IsClientMagicCookie(cookie.data(), cookie.size()));
        EXPECT_FALSE(IsServerMagicCookie(cookie.data(), cookie.size()));
    }

    TEST(MagicCookieTest, IsServerMagicCookie)
    {
        auto cookie = GenerateServerMagicCookie();
        EXPECT_TRUE(IsServerMagicCookie(cookie.data(), cookie.size()));
        EXPECT_FALSE(IsClientMagicCookie(cookie.data(), cookie.size()));
    }

    TEST(MagicCookieTest, IsMagicCookieBoth)
    {
        auto client = GenerateClientMagicCookie();
        auto server = GenerateServerMagicCookie();
        EXPECT_TRUE(IsMagicCookie(client.data(), client.size()));
        EXPECT_TRUE(IsMagicCookie(server.data(), server.size()));
    }

    TEST(MagicCookieTest, NotMagicCookieShortBuffer)
    {
        std::vector<uint8_t> data(10, 0x00);
        EXPECT_FALSE(IsMagicCookie(data.data(), data.size()));
    }

    TEST(MagicCookieTest, NotMagicCookieRandomData)
    {
        std::vector<uint8_t> data(16, 0xAB);
        EXPECT_FALSE(IsMagicCookie(data.data(), data.size()));
    }

    TEST(MagicCookieTest, FindMagicCookieAtStart)
    {
        auto cookie = GenerateClientMagicCookie();
        EXPECT_EQ(FindMagicCookie(cookie.data(), cookie.size()), 0U);
    }

    TEST(MagicCookieTest, FindMagicCookieAtOffset)
    {
        std::vector<uint8_t> stream(5, 0x00);
        auto cookie = GenerateServerMagicCookie();
        stream.insert(stream.end(), cookie.begin(), cookie.end());

        EXPECT_EQ(FindMagicCookie(stream.data(), stream.size()), 5U);
    }

    TEST(MagicCookieTest, FindMagicCookieNotFound)
    {
        std::vector<uint8_t> data(32, 0xCC);
        EXPECT_EQ(FindMagicCookie(data.data(), data.size()), data.size());
    }

    TEST(MagicCookieTest, MessageType0xFF)
    {
        auto cookie = GenerateClientMagicCookie();
        // Byte 14 is the message type
        EXPECT_EQ(cookie[14], 0xFF);
    }

    // ── TP Reassembly Manager Tests ─────────────────────────────

    TEST(TpReassemblyManagerTest, InitiallyEmpty)
    {
        TpReassemblyManager mgr;
        EXPECT_EQ(mgr.ActiveStreamCount(), 0U);
    }

    TEST(TpReassemblyManagerTest, FeedCreatesStream)
    {
        TpReassemblyManager mgr;
        TpStreamKey key{0x12340001, 0x0010};
        std::vector<uint8_t> data(16, 0xAA);

        mgr.FeedSegment(key, 0, true, data);
        EXPECT_EQ(mgr.ActiveStreamCount(), 1U);
    }

    TEST(TpReassemblyManagerTest, TwoStreamsIndependent)
    {
        TpReassemblyManager mgr;
        TpStreamKey key1{0x12340001, 0x0010};
        TpStreamKey key2{0x12340002, 0x0020};
        std::vector<uint8_t> data(16, 0xBB);

        mgr.FeedSegment(key1, 0, true, data);
        mgr.FeedSegment(key2, 0, true, data);
        EXPECT_EQ(mgr.ActiveStreamCount(), 2U);
    }

    TEST(TpReassemblyManagerTest, CompleteStreamCallsCallback)
    {
        TpReassemblyManager mgr;
        TpStreamKey key{0x00010001, 0x0001};

        std::vector<uint8_t> data(32, 0xCC);
        bool callbackInvoked = false;
        std::vector<uint8_t> receivedPayload;

        auto callback = [&](const TpStreamKey &k,
                            std::vector<uint8_t> payload)
        {
            callbackInvoked = true;
            receivedPayload = std::move(payload);
        };

        // Single segment, moreSegments=false → complete
        mgr.FeedSegment(key, 0, false, data, callback);

        EXPECT_TRUE(callbackInvoked);
        EXPECT_EQ(receivedPayload, data);
        // Stream should be removed after completion
        EXPECT_EQ(mgr.ActiveStreamCount(), 0U);
    }

    TEST(TpReassemblyManagerTest, MultiSegmentReassembly)
    {
        TpReassemblyManager mgr;
        TpStreamKey key{0x00010001, 0x0001};

        std::vector<uint8_t> seg1(16, 0x11);
        std::vector<uint8_t> seg2(16, 0x22);

        bool callbackInvoked = false;
        std::vector<uint8_t> result;

        auto callback = [&](const TpStreamKey &, std::vector<uint8_t> p)
        {
            callbackInvoked = true;
            result = std::move(p);
        };

        mgr.FeedSegment(key, 0, true, seg1);
        EXPECT_FALSE(callbackInvoked);

        mgr.FeedSegment(key, 16, false, seg2, callback);
        EXPECT_TRUE(callbackInvoked);
        EXPECT_EQ(result.size(), 32U);
    }

    TEST(TpReassemblyManagerTest, RemoveStream)
    {
        TpReassemblyManager mgr;
        TpStreamKey key{0x00020001, 0x0001};
        std::vector<uint8_t> data(16, 0xDD);

        mgr.FeedSegment(key, 0, true, data);
        EXPECT_EQ(mgr.ActiveStreamCount(), 1U);

        mgr.RemoveStream(key);
        EXPECT_EQ(mgr.ActiveStreamCount(), 0U);
    }

    TEST(TpReassemblyManagerTest, ClearAllStreams)
    {
        TpReassemblyManager mgr;
        TpStreamKey key1{0x00010001, 0x0001};
        TpStreamKey key2{0x00010002, 0x0002};
        std::vector<uint8_t> data(16, 0xEE);

        mgr.FeedSegment(key1, 0, true, data);
        mgr.FeedSegment(key2, 0, true, data);
        EXPECT_EQ(mgr.ActiveStreamCount(), 2U);

        mgr.Clear();
        EXPECT_EQ(mgr.ActiveStreamCount(), 0U);
    }

    TEST(TpReassemblyManagerTest, HasSegmentAt)
    {
        TpReassemblyManager mgr;
        TpStreamKey key{0x00010001, 0x0001};
        std::vector<uint8_t> data(16, 0xFF);

        EXPECT_FALSE(mgr.HasSegmentAt(key, 0));

        mgr.FeedSegment(key, 0, true, data);
        EXPECT_TRUE(mgr.HasSegmentAt(key, 0));
    }

    TEST(TpReassemblyManagerTest, StreamKeyEquality)
    {
        TpStreamKey key1{0x1234, 0x0001};
        TpStreamKey key2{0x1234, 0x0001};
        TpStreamKey key3{0x1234, 0x0002};

        EXPECT_TRUE(key1 == key2);
        EXPECT_FALSE(key1 == key3);
    }

    TEST(TpReassemblyManagerTest, StreamKeyOrdering)
    {
        TpStreamKey key1{0x0001, 0x0001};
        TpStreamKey key2{0x0002, 0x0001};
        TpStreamKey key3{0x0001, 0x0002};

        EXPECT_TRUE(key1 < key2);
        EXPECT_TRUE(key1 < key3);
    }

    // ── Session Handler Tests ─────────────────────────────────

    TEST(SessionHandlerTest, ActiveModeDefault)
    {
        SessionHandler handler;
        EXPECT_EQ(handler.Policy(), SessionHandlingPolicy::kActive);
    }

    TEST(SessionHandlerTest, ActiveModeIncrement)
    {
        SessionHandler handler(SessionHandlingPolicy::kActive);
        auto id1 = handler.NextSessionId();
        auto id2 = handler.NextSessionId();
        EXPECT_NE(id1, 0U);
        EXPECT_NE(id2, 0U);
        EXPECT_NE(id1, id2);
    }

    TEST(SessionHandlerTest, InactiveModeAlwaysZero)
    {
        SessionHandler handler(SessionHandlingPolicy::kInactive);
        EXPECT_EQ(handler.NextSessionId(), 0U);
        EXPECT_EQ(handler.NextSessionId(), 0U);
        EXPECT_EQ(handler.NextSessionId(), 0U);
    }

    TEST(SessionHandlerTest, RebootFlagInitiallyTrue)
    {
        SessionHandler handler;
        EXPECT_TRUE(handler.RebootFlag());
    }

    TEST(SessionHandlerTest, ClearRebootFlag)
    {
        SessionHandler handler;
        handler.ClearRebootFlag();
        EXPECT_FALSE(handler.RebootFlag());
    }

    TEST(SessionHandlerTest, ResetRestoresRebootFlag)
    {
        SessionHandler handler;
        handler.ClearRebootFlag();
        handler.Reset();
        EXPECT_TRUE(handler.RebootFlag());
    }

    TEST(SessionHandlerTest, ResetResetsSessionId)
    {
        SessionHandler handler(SessionHandlingPolicy::kActive);
        handler.NextSessionId();
        handler.NextSessionId();
        handler.Reset();
        auto id = handler.NextSessionId();
        EXPECT_EQ(id, 1U);
    }

    TEST(SessionHandlerTest, ActiveModeStartsAt1)
    {
        SessionHandler handler(SessionHandlingPolicy::kActive);
        EXPECT_EQ(handler.NextSessionId(), 1U);
    }

    TEST(SessionHandlerTest, PolicyAccessor)
    {
        SessionHandler active(SessionHandlingPolicy::kActive);
        SessionHandler inactive(SessionHandlingPolicy::kInactive);
        EXPECT_EQ(active.Policy(), SessionHandlingPolicy::kActive);
        EXPECT_EQ(inactive.Policy(), SessionHandlingPolicy::kInactive);
    }

    TEST(SessionHandlerTest, ConcurrentAccess)
    {
        SessionHandler handler(SessionHandlingPolicy::kActive);
        std::vector<std::uint16_t> ids1;
        std::vector<std::uint16_t> ids2;

        auto generate = [&handler](std::vector<std::uint16_t> &out, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                out.push_back(handler.NextSessionId());
            }
        };

        std::thread t1([&] { generate(ids1, 100); });
        std::thread t2([&] { generate(ids2, 100); });
        t1.join();
        t2.join();

        // All 200 IDs should be unique and non-zero
        std::set<std::uint16_t> allIds;
        for (auto id : ids1) allIds.insert(id);
        for (auto id : ids2) allIds.insert(id);
        EXPECT_EQ(allIds.size(), 200U);
        EXPECT_EQ(allIds.count(0U), 0U);
    }
}
