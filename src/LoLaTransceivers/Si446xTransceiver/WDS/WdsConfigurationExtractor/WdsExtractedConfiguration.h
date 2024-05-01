// WdsExtractedConfiguration.h

#ifndef _WDS_EXTRACTED_CONFIGURATION_h
#define _WDS_EXTRACTED_CONFIGURATION_h

// Replace radio_config_Si4463 file or contents with exported configuration from WDS editor.
// Removed/comment the generated #include "..\drivers\radio\Si446x\"
#define SI446X_PATCH_CMDS 0x00, {}
#include "radio_config_Si4463.h"


namespace WdsExtractedConfiguration
{
	static constexpr uint8_t PowerUp[] = { RF_POWER_UP };
	static constexpr uint8_t GpioPinCfg[] = { RF_GPIO_PIN_CFG };
	static constexpr uint8_t CrystalTune[] = { RF_GLOBAL_XO_TUNE_1 };
	static constexpr uint8_t GlobalConfig1[] = { RF_GLOBAL_CONFIG_1 };
	static constexpr uint8_t PreambleConfig[] = { RF_PREAMBLE_CONFIG_1 };
	static constexpr uint8_t ModemType1[] = { RF_MODEM_MOD_TYPE_12 };
	static constexpr uint8_t ModemFreqDeviation[] = { RF_MODEM_FREQ_DEV_0_1 };
	static constexpr uint8_t ModemTxRampDelay1[] = { RF_MODEM_TX_RAMP_DELAY_12 };
	static constexpr uint8_t ModemBcrNcoOffset1[] = { RF_MODEM_BCR_NCO_OFFSET_2_12 };
	static constexpr uint8_t ModemAfcLimiter1[] = { RF_MODEM_AFC_LIMITER_1_3 };
	static constexpr uint8_t ModemAgcControl1[] = { RF_MODEM_AGC_CONTROL_1 };
	static constexpr uint8_t ModemAdcWindow1[] = { RF_MODEM_AGC_WINDOW_SIZE_12 };
	static constexpr uint8_t ModemRawControl1[] = { RF_MODEM_RAW_CONTROL_5 };
	static constexpr uint8_t ModemRssiThreshold[] = { RF_MODEM_RSSI_JUMP_THRESH_4 };
	static constexpr uint8_t ModemRawSearch1[] = { RF_MODEM_RAW_SEARCH2_2 };
	static constexpr uint8_t ModemSpikeDetect1[] = { RF_MODEM_SPIKE_DET_2 };
	static constexpr uint8_t ModemRssiMute1[] = { RF_MODEM_RSSI_MUTE_1 };
	static constexpr uint8_t ModemDsaControl1[] = { RF_MODEM_DSA_CTRL1_5 };
	static constexpr uint8_t ModemChannelFilter1[] = { RF_MODEM_CHFLT_RX1_CHFLT_COE13_7_0_12 };
	static constexpr uint8_t ModemChannelFilter2[] = { RF_MODEM_CHFLT_RX1_CHFLT_COE1_7_0_12 };
	static constexpr uint8_t ModemChannelFilter3[] = { RF_MODEM_CHFLT_RX2_CHFLT_COE7_7_0_12 };
	static constexpr uint8_t PaTuneControl[] = { RF_PA_TC_1 };
	static constexpr uint8_t SynthPowerFactor1[] = { RF_SYNTH_PFDCP_CPFF_7 };
	static constexpr uint8_t FrequencyControl1[] = { RF_FREQ_CONTROL_INTE_8 };
	static constexpr uint8_t StartRx[] = { RF_START_RX };
	static constexpr uint8_t IrCalibration1[] = { RF_IRCAL };
	static constexpr uint8_t IrCalibration2[] = { RF_IRCAL_1 };
	static constexpr uint8_t GlobalClockConfig[] = { RF_GLOBAL_CLK_CFG_1 };
	static constexpr uint8_t GlobalConfig2[] = { RF_GLOBAL_CONFIG_1_1 };
	static constexpr uint8_t InterruptControlEnable[] = { RF_INT_CTL_ENABLE_4 };
	static constexpr uint8_t FrrControl[] = { RF_FRR_CTL_A_MODE_4 };
	static constexpr uint8_t PreambleLength[] = { RF_PREAMBLE_TX_LENGTH_9 };
	static constexpr uint8_t SyncConfig[] = { RF_SYNC_CONFIG_6 };
	static constexpr uint8_t PacketCrcConfig[] = { RF_PKT_CRC_CONFIG_12 };
	static constexpr uint8_t PacketRxThreshold[] = { RF_PKT_RX_THRESHOLD_12 };
	static constexpr uint8_t PacketFieldConfig1[] = { RF_PKT_FIELD_3_CRC_CONFIG_12 };
	static constexpr uint8_t PacketFieldConfig2[] = { RF_PKT_RX_FIELD_1_CRC_CONFIG_12 };
	static constexpr uint8_t PacketFieldConfig3[] = { RF_PKT_RX_FIELD_4_CRC_CONFIG_5 };
	static constexpr uint8_t PacketCrcSeed[] = { RF_PKT_CRC_SEED_31_24_4 };
	static constexpr uint8_t ModemModulationType[] = { RF_MODEM_MOD_TYPE_12_1 };
	static constexpr uint8_t ModemFrequencyDeviation[] = { RF_MODEM_FREQ_DEV_0_1_1 };
	static constexpr uint8_t ModemTxRampDelay2[] = { RF_MODEM_TX_RAMP_DELAY_12_1 };
	static constexpr uint8_t ModemBcrNcoOffset2[] = { RF_MODEM_BCR_NCO_OFFSET_2_12_1 };
	static constexpr uint8_t ModemAfcLimiter2[] = { RF_MODEM_AFC_LIMITER_1_3_1 };
	static constexpr uint8_t ModemAgcControl2[] = { RF_MODEM_AGC_CONTROL_1_1 };
	static constexpr uint8_t ModemAdcWindow2[] = { RF_MODEM_AGC_WINDOW_SIZE_12_1 };
	static constexpr uint8_t ModemRawControl2[] = { RF_MODEM_RAW_CONTROL_10 };
	static constexpr uint8_t ModemRawSearch2[] = { RF_MODEM_RAW_SEARCH2_2_1 };
	static constexpr uint8_t ModemSpikeDetect2[] = { RF_MODEM_SPIKE_DET_2_1 };
	static constexpr uint8_t ModemRssiMute2[] = { RF_MODEM_RSSI_MUTE_1_1 };
	static constexpr uint8_t ModemDsaControl2[] = { RF_MODEM_DSA_CTRL1_5_1 };
	static constexpr uint8_t ModemChannelFilter4[] = { RF_MODEM_CHFLT_RX1_CHFLT_COE13_7_0_12_1 };
	static constexpr uint8_t ModemChannelFilter5[] = { RF_MODEM_CHFLT_RX1_CHFLT_COE1_7_0_12_1 };
	static constexpr uint8_t ModemChannelFilter6[] = { RF_MODEM_CHFLT_RX2_CHFLT_COE7_7_0_12_1 };
	static constexpr uint8_t PaMode[] = { RF_PA_MODE_4 };
	static constexpr uint8_t SynthPowerFactor2[] = { RF_SYNTH_PFDCP_CPFF_7_1 };
	static constexpr uint8_t MatchValue[] = { RF_MATCH_VALUE_1_12 };
	static constexpr uint8_t FrequencyControl2[] = { RF_FREQ_CONTROL_INTE_8_1 };

	static constexpr size_t GetFullSize()
	{
		return sizeof(PowerUp)
			+ sizeof(GpioPinCfg)
			+ sizeof(CrystalTune)
			+ sizeof(GlobalConfig1)
			+ sizeof(PreambleConfig)
			+ sizeof(ModemType1)
			+ sizeof(ModemFreqDeviation)
			+ sizeof(ModemTxRampDelay1)
			+ sizeof(ModemBcrNcoOffset1)
			+ sizeof(ModemAfcLimiter1)
			+ sizeof(ModemAgcControl1)
			+ sizeof(ModemAdcWindow1)
			+ sizeof(ModemRawControl1)
			+ sizeof(ModemRssiThreshold)
			+ sizeof(ModemRawSearch1)
			+ sizeof(ModemSpikeDetect1)
			+ sizeof(ModemRssiMute1)
			+ sizeof(ModemDsaControl1)
			+ sizeof(ModemChannelFilter1)
			+ sizeof(ModemChannelFilter2)
			+ sizeof(ModemChannelFilter3)
			+ sizeof(PaTuneControl)
			+ sizeof(SynthPowerFactor1)
			+ sizeof(FrequencyControl1)
			+ sizeof(StartRx)
			+ sizeof(IrCalibration1)
			+ sizeof(IrCalibration2)
			+ sizeof(GlobalClockConfig)
			+ sizeof(GlobalConfig2)
			+ sizeof(InterruptControlEnable)
			+ sizeof(FrrControl)
			+ sizeof(PreambleLength)
			+ sizeof(SyncConfig)
			+ sizeof(PacketCrcConfig)
			+ sizeof(PacketRxThreshold)
			+ sizeof(PacketFieldConfig1)
			+ sizeof(PacketFieldConfig2)
			+ sizeof(PacketFieldConfig3)
			+ sizeof(PacketCrcSeed)
			+ sizeof(ModemModulationType)
			+ sizeof(ModemFrequencyDeviation)
			+ sizeof(ModemTxRampDelay2)
			+ sizeof(ModemBcrNcoOffset2)
			+ sizeof(ModemAfcLimiter2)
			+ sizeof(ModemAgcControl2)
			+ sizeof(ModemAdcWindow2)
			+ sizeof(ModemRawControl2)
			+ sizeof(ModemRawSearch2)
			+ sizeof(ModemSpikeDetect2)
			+ sizeof(ModemRssiMute2)
			+ sizeof(ModemDsaControl2)
			+ sizeof(ModemChannelFilter4)
			+ sizeof(ModemChannelFilter5)
			+ sizeof(ModemChannelFilter6)
			+ sizeof(PaMode)
			+ sizeof(SynthPowerFactor2)
			+ sizeof(MatchValue)
			+ sizeof(FrequencyControl2);
	}
};

