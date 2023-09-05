// TestDuplex.h

#ifndef _TESTDUPLEX_h
#define _TESTDUPLEX_h

class TestDuplex
{
private:
	static constexpr uint32_t DUPLEX_PERIOD_MICROS = 5000;
	static constexpr uint32_t DUPLEX_ASSYMETRIC_RATIO = 10;


private:
	/// <summary>
	/// </summary>
	/// <typeparam name="Period"></typeparam>
	/// <typeparam name="IsFullDuplex"></typeparam>
	/// <typeparam name="StartIn"></typeparam>
	/// <typeparam name="EndIn"></typeparam>
	/// <typeparam name="Duration"></typeparam>
	/// <param name="duplex"></param>
	/// <returns>True if test successful.</returns>
	template<uint32_t Period, bool IsFullDuplex, uint32_t StartIn, uint32_t EndIn, uint16_t Duration>
	static const bool TestDuplexSlot(IDuplex* duplex)
	{
		if (duplex == nullptr)
		{
			Serial.println(F("Duplex is null."));
			return false;
		}

		const uint16_t period = duplex->GetPeriod();

		if (IsFullDuplex)
		{
			if (period != IDuplex::DUPLEX_FULL)
			{
				Serial.println(F("Duplex is expected to be full."));
				return false;
			}

			for (uint32_t i = 0; i < Period; i++)
			{
				if (!duplex->IsInRange(i, Duration))
				{
					Serial.print(F("Slot negative at timestamp "));
					Serial.println(i);

					return false;
				}
			}
		}
		else
		{
			if (duplex->GetPeriod() == IDuplex::DUPLEX_FULL)
			{
				Serial.println(F("Duplex is expected to not be full."));
				return false;
			}

			if (period != Period)
			{
				Serial.println(F("Duplex has wrong period."));
				return false;
			}

			if (duplex->GetRange() <= Duration)
			{
				Serial.println(F("Slot is shorter than Duration."));
				return false;
			}

			for (uint32_t i = 0; i < StartIn; i++)
			{
				if (duplex->IsInRange(i, Duration))
				{
					Serial.print(F("Slot false pre-positive at timestamp "));
					Serial.println(i);

					return false;
				}
			}

			for (uint32_t i = StartIn; i < EndIn - Duration; i++)
			{
				if (!duplex->IsInRange(i, Duration))
				{
					Serial.print(F("Slot false negative at timestamp "));
					Serial.println(i);

					return false;
				}
			}

			for (uint32_t i = EndIn - Duration; i <= EndIn; i++)
			{
				if (duplex->IsInRange(i, Duration))
				{
					Serial.print(F("Slot duration false positive at timestamp "));
					Serial.println(i);

					return false;
				}
			}

			for (uint32_t i = EndIn + 1; i < Period; i++)
			{
				if (duplex->IsInRange(i, Duration))
				{
					Serial.print(F("Slot false post-positive at timestamp "));
					Serial.println(i);

					return false;
				}
			}
		}

		return true;
	}

	static const bool TestDuplexes()
	{
		FullDuplex DuplexFull{};
		HalfDuplex<DUPLEX_PERIOD_MICROS, false, 0> DuplexSymmetricalA{};
		HalfDuplex<DUPLEX_PERIOD_MICROS, true, 0> DuplexSymmetricalB{};
		HalfDuplexAsymmetric< DUPLEX_PERIOD_MICROS, UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO, false, 0> DuplexAssymmetricalA{};
		HalfDuplexAsymmetric< DUPLEX_PERIOD_MICROS, UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO, true, 0> DuplexAssymmetricalB{};

		SlottedDuplex<DUPLEX_PERIOD_MICROS, 0> DuplexSlottedA{};
		SlottedDuplex<DUPLEX_PERIOD_MICROS, 0> DuplexSlottedB{};
		SlottedDuplex<DUPLEX_PERIOD_MICROS, 0> DuplexSlottedC{};

		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, true, 0, DUPLEX_PERIOD_MICROS, 0>(&DuplexFull)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, true, 0, DUPLEX_PERIOD_MICROS, 100>(&DuplexFull)) { return false; }

		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, 0, DUPLEX_PERIOD_MICROS / 2, 0>(&DuplexSymmetricalA)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, 0, (DUPLEX_PERIOD_MICROS / 2), 200>(&DuplexSymmetricalA)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, DUPLEX_PERIOD_MICROS / 2, DUPLEX_PERIOD_MICROS, 0>(&DuplexSymmetricalB)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, (DUPLEX_PERIOD_MICROS / 2), DUPLEX_PERIOD_MICROS, 1333>(&DuplexSymmetricalB)) { return false; }


		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, 0, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, 0>(&DuplexAssymmetricalA)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, 0, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, 100>(&DuplexAssymmetricalA)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, DUPLEX_PERIOD_MICROS, 0>(&DuplexAssymmetricalB)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, ((uint32_t)(UINT8_MAX / DUPLEX_ASSYMETRIC_RATIO) * DUPLEX_PERIOD_MICROS) / UINT8_MAX, DUPLEX_PERIOD_MICROS, 100>(&DuplexAssymmetricalB)) { return false; }

		if (!DuplexSlottedA.SetTotalSlots(3)
			|| !DuplexSlottedB.SetTotalSlots(3)
			|| !DuplexSlottedC.SetTotalSlots(3))
		{
			Serial.println(F("SetTotalSlots failed."));
			return false;
		}

		if (!DuplexSlottedA.SetSlot(0)
			|| !DuplexSlottedB.SetSlot(1)
			|| !DuplexSlottedC.SetSlot(2))
		{
			Serial.println(F("SetTotalSlots failed."));
			return false;
		}

		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, 0, ((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3, 0 >(&DuplexSlottedA)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, 0, (((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3), DUPLEX_PERIOD_MICROS / 10>(&DuplexSlottedA)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, ((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3, ((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3, 0>(&DuplexSlottedB)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, (((uint32_t)1 * DUPLEX_PERIOD_MICROS) / 3), ((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3, DUPLEX_PERIOD_MICROS / 5>(&DuplexSlottedB)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, ((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3, DUPLEX_PERIOD_MICROS, 0>(&DuplexSlottedC)) { return false; }
		if (!TestDuplexSlot<DUPLEX_PERIOD_MICROS, false, (((uint32_t)2 * DUPLEX_PERIOD_MICROS) / 3), DUPLEX_PERIOD_MICROS, DUPLEX_PERIOD_MICROS / 4>(&DuplexSlottedC)) { return false; }

		return true;
	}

public:
	static const bool RunTests()
	{
		if (!TestDuplexes())
		{
			Serial.println(F("TestDuplexes failed"));
			return false;
		}

		return true;
	}
};
#endif