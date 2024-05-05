// DeliveryTestService.h

#ifndef _DELIVERY_TEST_SERVICE_h
#define _DELIVERY_TEST_SERVICE_h

#include "Services\Delivery\AbstractDeliveryService.h"

namespace TestDeliveryData
{
	static constexpr uint8_t Line1[] = { 'H', 'e' ,'l','l', 'o' };
	static constexpr uint8_t Line2[] = { 'w', 'o', 'r', 'l','d' ,'!' };

	static constexpr uint8_t LineCount = 2;
}

template<const char OnwerName,
	const uint8_t Port,
	const uint32_t ServiceId,
	const bool IsSender>
class DeliveryTestService : public AbstractDeliveryService<Port, ServiceId>
{
private:
	using BaseClass = AbstractDeliveryService<Port, ServiceId>;

protected:
	using BaseClass::CanRequestDelivery;
	using BaseClass::RequestDelivery;
	using BaseClass::CancelDelivery;

private:
	enum class TestStateEnum
	{
		Sending,
		Checking,
		Pausing
	};

private:
	static constexpr uint32_t RETRY_PERIOD_MILLIS = 2000;
	static constexpr uint32_t RESTART_PERIOD_MILLIS = 2500;

	uint32_t SendTime = 0;
	TestStateEnum TestState = TestStateEnum::Sending;
	uint8_t SendIndex = 0;

public:
	DeliveryTestService(Scheduler& scheduler, ILoLaLink* link)
		: BaseClass(scheduler, link)
	{}

#ifdef DEBUG_LOLA
protected:
	void PrintName()
	{
		Serial.print(millis());
		Serial.print('\t');
		Serial.print('[');
		Serial.print(OnwerName);
		Serial.print(']');
		Serial.print('\t');
	}
#endif

protected:
	void OnServiceStarted() final
	{
		BaseClass::OnServiceStarted();

		TestState = TestStateEnum::Sending;
		Task::enableDelayed(0);
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Test Delivery Started."));
#endif
	}

	void OnLinkedServiceRun() final
	{
		BaseClass::OnLinkedServiceRun();

		if (IsSender)
		{
			switch (TestState)
			{
			case TestStateEnum::Sending:
				if (CanRequestDelivery())
				{
					switch (SendIndex)
					{
					case 0:
						RequestDelivery(TestDeliveryData::Line1, sizeof(TestDeliveryData::Line1));
						break;
					case 1:
						RequestDelivery(TestDeliveryData::Line2, sizeof(TestDeliveryData::Line2));
						break;
					default:
						break;
					}
					TestState = TestStateEnum::Checking;
					SendTime = millis();

				}
				Task::enableDelayed(0);
				break;
			case TestStateEnum::Checking:
				if (millis() - SendTime > RETRY_PERIOD_MILLIS)
				{
#if defined(DEBUG_LOLA)
					PrintName();
					Serial.println(F("Delivery Check timed out."));
#endif
					CancelDelivery();
					TestState = TestStateEnum::Sending;
				}
				Task::enableDelayed(0);
				break;
			case TestStateEnum::Pausing:
				if (millis() - SendTime > RESTART_PERIOD_MILLIS)
				{
					SendIndex = (SendIndex + 1) % TestDeliveryData::LineCount;

					TestState = TestStateEnum::Sending;
				}
				Task::enableDelayed(0);
			default:
				break;
			}

			Task::enableDelayed(0);
		}
	}

	void OnDeliveryComplete() final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.println(F("Delivery complete."));
#endif
		TestState = TestStateEnum::Pausing;
		SendTime = millis();
	}

	void OnDeliveryReceived(const uint32_t timestamp, const uint8_t* data, const uint8_t dataSize) final
	{
#if defined(DEBUG_LOLA)
		PrintName();
		Serial.print(F("Got delivered:"));
		Serial.print('\t');
#endif
		Serial.write(data, (uint32_t)dataSize);
		Serial.println();
	}
};
#endif