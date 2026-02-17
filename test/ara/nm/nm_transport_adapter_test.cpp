#include <gtest/gtest.h>
#include "../../../src/ara/nm/nm_transport_adapter.h"

namespace ara
{
    namespace nm
    {
        namespace
        {
            /// Mock transport adapter for testing.
            class MockNmTransportAdapter : public NmTransportAdapter
            {
            public:
                core::Result<void> SendNmPdu(
                    const std::string &channelName,
                    const std::vector<std::uint8_t> &pduData) override
                {
                    mLastChannel = channelName;
                    mLastPdu = pduData;
                    ++mSendCount;

                    // Loopback: if handler is registered, deliver back
                    if (mHandler)
                    {
                        mHandler(channelName, pduData);
                    }

                    return core::Result<void>::FromValue();
                }

                core::Result<void> RegisterReceiveHandler(
                    NmPduHandler handler) override
                {
                    mHandler = std::move(handler);
                    return core::Result<void>::FromValue();
                }

                core::Result<void> Start() override
                {
                    mRunning = true;
                    return core::Result<void>::FromValue();
                }

                void Stop() override
                {
                    mRunning = false;
                }

                bool mRunning{false};
                int mSendCount{0};
                std::string mLastChannel;
                std::vector<std::uint8_t> mLastPdu;
                NmPduHandler mHandler;
            };
        }

        TEST(NmTransportAdapterTest, MockStartStop)
        {
            MockNmTransportAdapter _adapter;
            EXPECT_FALSE(_adapter.mRunning);

            auto _startResult = _adapter.Start();
            EXPECT_TRUE(_startResult.HasValue());
            EXPECT_TRUE(_adapter.mRunning);

            _adapter.Stop();
            EXPECT_FALSE(_adapter.mRunning);
        }

        TEST(NmTransportAdapterTest, SendPdu)
        {
            MockNmTransportAdapter _adapter;
            _adapter.Start();

            std::vector<std::uint8_t> _pdu{0x01, 0x02, 0x03};
            auto _result = _adapter.SendNmPdu("ch1", _pdu);
            EXPECT_TRUE(_result.HasValue());
            EXPECT_EQ(_adapter.mSendCount, 1);
            EXPECT_EQ(_adapter.mLastChannel, "ch1");
            EXPECT_EQ(_adapter.mLastPdu, _pdu);
        }

        TEST(NmTransportAdapterTest, LoopbackReceive)
        {
            MockNmTransportAdapter _adapter;
            _adapter.Start();

            std::string _receivedChannel;
            std::vector<std::uint8_t> _receivedPdu;

            _adapter.RegisterReceiveHandler(
                [&_receivedChannel, &_receivedPdu](
                    const std::string &ch,
                    const std::vector<std::uint8_t> &data)
                {
                    _receivedChannel = ch;
                    _receivedPdu = data;
                });

            std::vector<std::uint8_t> _pdu{0xAA, 0xBB};
            _adapter.SendNmPdu("ch2", _pdu);

            EXPECT_EQ(_receivedChannel, "ch2");
            EXPECT_EQ(_receivedPdu, _pdu);
        }
    }
}
