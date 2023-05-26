//// LoLaSi446xSPIDriver.h
//
//#ifndef _LOLASI446XSPIDRIVER_h
//#define _LOLASI446XSPIDRIVER_h
//
////#define SI446X_USE_DMA
//
//#include <stdint.h>
//#include <SPI.h>
//#include <Arduino.h>
//
//
//
///// <summary>
///// Abstracts the pins and SPI communication.
///// </summary>
//class LoLaSi446xSPIDriver
//{
//public:
//	static const uint16_t PROPERTY_GROUP_PA = ((uint16_t)0x22 << 8);
//	static const uint16_t PROPERTY_GROUP_INTERRUPTS = ((uint16_t)0x01 << 8);
//	static const uint16_t PROPERTY_GROUP_PACKET = ((uint16_t)0x12 << 8);
//	static const uint16_t PROPERTY_GROUP_GLOBAL = ((uint16_t)0x00 << 8);
//
//	static const uint16_t CONFIG_PA_POWER = PROPERTY_GROUP_PA | 0x01;
//	static const uint16_t CONFIG_INTERRUPTS_ENABLE = PROPERTY_GROUP_INTERRUPTS | 0x01;
//	static const uint16_t CONFIG_INTERRUPTS_CONTROL = PROPERTY_GROUP_INTERRUPTS | 0x03;
//	static const uint16_t CONFIG_PACKET_SIZE_ENABLE = PROPERTY_GROUP_PACKET | 0x12;
//	static const uint16_t CONFIG_WUT = PROPERTY_GROUP_GLOBAL | 0x04;
//	static const uint16_t CONFIG_WUT_LBD_EN = 0x02;
//
//
//	static const uint8_t COMMAND_GET_PART_INFO = 0x01;
//	static const uint8_t COMMAND_FUNCTION_INFO = 0x10;
//	static const uint8_t COMMAND_SET_PROPERTY = 0x11;
//	static const uint8_t COMMAND_GET_PROPERTY = 0x12;
//	static const uint8_t COMMAND_IR_CAL = 0x17;
//	static const uint8_t COMMAND_LOW_BAT_INTERRUPT_ENABLE = 0x01;
//	static const uint8_t COMMAND_GET_STATUS = 0x20;
//	static const uint8_t COMMAND_SET_STATUS = 0x34;
//	static const uint8_t COMMAND_START_TX = 0x31;
//	static const uint8_t COMMAND_START_RX = 0x32;
//
//	static const uint8_t COMMAND_FIFO_INFO = 0x15;
//
//	static const uint8_t COMMAND_CLEAR_TX_FIFO = 0x01;
//	static const uint8_t COMMAND_CLEAR_RX_FIFO = 0x02;
//
//	static const uint8_t COMMAND_READ_RX_FIFO = 0x77;
//	static const uint8_t COMMAND_WRITE_TX_FIFO = 0x66;
//	static const uint8_t COMMAND_READ_FRR_B = 0x51;
//
//	static const uint8_t PENDING_EVENT_SYNC_DETECT = _BV(0);
//	static const uint8_t PENDING_EVENT_LOW_BATTERY = _BV(1);
//	static const uint8_t PENDING_EVENT_WUT = _BV(0);
//	static const uint8_t PENDING_EVENT_CRC_ERROR = _BV(3);
//	static const uint8_t PENDING_EVENT_RX = _BV(4);
//	static const uint8_t PENDING_EVENT_SENT_OK = _BV(5);
//
//protected:
//	static const uint8_t COMMAND_READ_BUFFER = 0x44;
//
//	//SPI instance.
//	const SPI& spi = nullptr;
//
//	//TODO: Store timestamp on event pending flag?
//	volatile uint32_t EventTimestamp = 0;
//
//	const uint8_t CS_PIN;
//	const uint8_t RESET_PIN;
//	const uint8_t IRQ_PIN;
//
//	volatile bool InterruptsEnabled = false;
//
//
//public:
//	LoLaSi446xSPIDriver(SPIClass& spi_instance, const uint8_t cs, const uint8_t reset, const uint8_t irq)
//		: CS_PIN(cs)
//		, RESET_PIN(reset)
//		, IRQ_PIN(irq)
//	{
//		spi = spi_instance;
//	}
//
//	void OnRadioInterrupt()
//	{
//		EventTimestamp = micros();
//		//RadioTask->Wake();
//	}
//
//protected:
//	void AttachInterrupt();
//
//protected:
//	/*virtual void OnResetLiveData()
//	{
//		LoLaPacketDriver::OnResetLiveData();
//		EventTimestamp = 0;
//	}*/
//
//	/*inline void Wake()
//	{
//		RadioTask->Wake();
//	}
//
//	void SetRadioTask(IRadioTask* radioTask)
//	{
//		RadioTask = radioTask;
//	}*/
//
//	void InterruptOff()
//	{
//		if (InterruptsEnabled)
//		{
//			InterruptsEnabled = false;
//			detachInterrupt(digitalPinToInterrupt(IRQ_PIN));
//		}
//	}
//
//	void InterruptOn()
//	{
//		if (!InterruptsEnabled)
//		{
//			AttachInterrupt();
//			Wake();
//			InterruptsEnabled = true;
//		}
//	}
//
//protected:
//	const bool Setup()
//	{
//		CsOff();
//
//		pinMode(CS_PIN, OUTPUT);
//		pinMode(RESET_PIN, OUTPUT);
//		pinMode(IRQ_PIN, INPUT_PULLUP);
//
//		spi.begin();
//
//#define LOLA_SET_SPEED_SPI
//
//#ifdef LOLA_SET_SPEED_SPI
//		//The SPI interface is designed to operate at a maximum of 10 MHz.
//#if defined(ARDUINO_ARCH_AVR)
//		spi->setClockDivider(SPI_CLOCK_DIV2); // 16 MHz / 2 = 8 MHz
//#elif defined(ARDUINO_ARCH_STM32F1)
//		spi->setClockDivider(SPI_CLOCK_DIV8); // 72 MHz / 8 = 9 MHz
//#endif
//#endif
//
//		RadioReset();
//
//		return true;
//	}
//
//
//	void CsOn()
//	{
//		digitalWrite(CS_PIN, LOW);
//	}
//
//	void CsOff()
//	{
//		digitalWrite(CS_PIN, HIGH);
//	}
//
//	// Reset the RF chip
//	void RadioReset()
//	{
//		digitalWrite(RESET_PIN, HIGH);
//		delayMicroseconds(100);
//		digitalWrite(RESET_PIN, LOW);
//		delayMicroseconds(100);
//	}
//
//protected:
//	// Low level communications.
//	void Send(const uint8_t* source, const uint8_t size)
//	{
//		CsOn();
//#ifdef SI446X_USE_DMA
//		spi->dmaSend(MessageOut, size);
//#else
//		for (uint8_t i = 0; i < size; i++)
//		{
//			spi->transfer(source[i]);
//		}
//#endif
//		CsOff();
//	}
//
//
//	/// <summary>
//	/// Read CTS and if its ok then read the command buffer
//	/// </summary>
//	/// <returns></returns>
//	const bool GetCommandOk()
//	{
//		CsOn();
//		// Send command. //TODO : replace command?
//		spi.transfer(COMMAND_READ_BUFFER);
//
//		// Get CTS value.
//		const bool cts = spi.transfer(0xFF) == 0xFF;
//
//		CsOff();
//
//
//		return cts;
//	}
//
//	const bool GetResponse(uint8_t* target, const uint8_t size)
//	{
//		bool cts = false;
//
//		CsOn();
//
//		// Same as GetCommandOk().
//		spi.transfer(COMMAND_READ_BUFFER);
//		cts = spi.transfer(0xFF) == 0xFF;
//
//		if (cts)
//		{
//			// Get response data
//#ifdef SI446X_USE_DMA
//			//If passed as 0, it sends FF repeatedly for "length" bytes.
//			spi.dmaTransfer(0, target, size);
//#else
//			for (uint8_t i = 0; i < size; i++)
//			{
//				target[i] = spi_transfer(0xFF);
//			}
//#endif					
//		}
//		CsOff();
//
//		return cts;
//	}
//};
//#endif
//
