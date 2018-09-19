#pragma once
// ST25RU3993_driver_api : définit le point d'entrée pour le driver du module ST25RU3993
//
#include "SerialCommHelper.h"
#include "st_stream.h"
#include "commands_application.h"

/** Definition for the maximal TID length in an inventory round in bytes*/
#define TIDLENGTH               12

/** Definition for the PC length */
#define PCLENGTH                2
/** Definition for the maximal EPC length, standard allows up to 62 bytes */
#define EPCLENGTH               32

#define SENDTAGFIXDATALEN		7

#define READ_TIMEOUT 500

void encapsulate(const unsigned char protocol, const unsigned short tx_size, const unsigned short rx_size, const unsigned char *data);
void decapsulate(const char * const rxBuffer, char * const TID, char * const protocol, short * const tx_size, short * const rx_size, char * const data);

typedef struct payload
{
	char id;
	short txSize;
	short rxSize;
	char * data;
}payload_t;

typedef struct protocolPacket
{
	char TID;
	unsigned char payloadSize;
	payload protocol;
}protocolPacket_t;


int WaitForPacket(CSerialCommHelper *driver, protocolPacket *packet, unsigned long timeout);

void ReaderSetConfig(CSerialCommHelper *driver);
void ReaderStartScan(CSerialCommHelper *driver);
void ReaderStopScan(CSerialCommHelper *driver);

int CheckConnection(CSerialCommHelper *Driver);

void GetPacketInfo(const char * const rxBuffer, char * const TID, char * const payload);

// ST_COM_CTRL_CMD_FW_INFORMATION     0x66 /* returns zero-terminated string with information about fw e.g. chip, board */
void CmdGetFwInfo(CSerialCommHelper *Driver, char *hardware_info, char *software_info);

// ST_COM_CTRL_CMD_FW_NUMBER          0x67 /* returns the 3-byte FW number */
void CmdGetFwVersion(CSerialCommHelper *Driver, char *major, char *minor, char *rev);

// ST_COM_WRITE_REG                   0x68
void CmdWriteReg(CSerialCommHelper *Driver, char addr, char value);
// ST_COM_READ_REG                    0x69
void CmdReadReg(CSerialCommHelper *Driver, char addr, char *retValue);

// ST_COM_READ_REG                    0x69
void CmdReadAllRegs(CSerialCommHelper *Driver, char *regBuffer);

typedef struct boardSettings {
	unsigned char Satus;
	unsigned char VCO; // 0 = Internal, 1 = External
	unsigned char PA; // 0 = internal_PA, 1 = external_PA, 2 = both available
	unsigned char INP; // 0 = BALANCED_INP, 1 = SINGLE_INP 
	unsigned char ANT; // 0 = ANT2, 1 = ANT2
	unsigned char Tuner; // bit0 = TUNER_CIN, bit1 = TUNER_CLEN, bit2 = TUNER_COUT bit3-7 = N/a
	unsigned char pow_down; // power down mode
	unsigned char HW_ID; // hardware ID number
}boardSettings_t;

/** appl command ID for callReaderConfig() */
// CMD_READER_CONFIG       0x00
void CmdReaderConfig(CSerialCommHelper *Driver, char changePowerDown, char value, boardSettings *config);
// SUBCMD_CHANGE_FREQ_RSSI				0x00
void CmdFreqRssi(CSerialCommHelper *Driver, unsigned int freq, unsigned char * rssiLog);
// SUBCMD_CHANGE_FREQ_REFL				0x01
void CmdFreqRefl(CSerialCommHelper *Driver, unsigned int freq, unsigned char useTunerSettings, unsigned short * iqTogetherNoise, unsigned short * iqTogetherReflected);
// SUBCMD_CHANGE_FREQ_ADD				0x02
void CmdFreqAdd(CSerialCommHelper *Driver, unsigned int freq, unsigned char clearFrequency, unsigned char profile);
// SUBCMD_CHANGE_FREQ_GETFREQ          0x03
void CmdFreqGetFreq(CSerialCommHelper *Driver, unsigned char *profile, unsigned int *freqMin, unsigned int *freqMax, unsigned char *numFreqs);

typedef struct Gen2Settings
{
	unsigned char setLinkFreq;
	unsigned char linkFreq;
	unsigned char setMiller;
	unsigned char miller;
	unsigned char setSession;
	unsigned char session;
	unsigned char setTrext;
	unsigned char trext;
	unsigned char setTari;
	unsigned char tari;
	unsigned char setGen2qbegin;
	unsigned char gen2qbegin;
	unsigned char setSel;
	unsigned char sel;
	unsigned char setTarget;
	unsigned char target;
}Gen2Settings_t;

/** appl command ID for callConfigGen2() */
// CMD_GEN2_SETTINGS       0x03
void CmdGen2Settings(CSerialCommHelper *driver, Gen2Settings *settingsIn, Gen2Settings *settingsOut);

/** appl command ID for callConfigTxRx() */
// CMD_CONFIG_TX_RX        0x04
void CmdConfigTxRx(CSerialCommHelper *driver,
	unsigned char setSensi, char *sensi,
	unsigned char setAntenna, unsigned char *antenna, unsigned short *alternateAntennaInterval);

typedef struct invGen2DataHeader
{
	unsigned char cyclicInventory;
	unsigned char availableTagSlot;
	unsigned char numTagsConnected;
	unsigned char tunningStatus;
	unsigned short roundCounter;
	unsigned char sensitivity;
	unsigned char gen2qbegin; // adaptive Q
	unsigned short adc;
	unsigned int frequency;
}invGen2DataHeader_t;

typedef struct invGen2Tag
{
	/** RN16 number */
	uint8_t rn16[2];
	/** PC value */
	uint8_t pc[2];
	/** EPC array */
	uint8_t epc[EPCLENGTH]; /* epc must immediately follow pc */
							/** EPC length, in bytes */
	uint8_t epclen;
	/** Handle for write and read communication with the Tag */
	uint8_t handle[2];
	/** logarithmic rssi which has been measured when reading this Tag. */
	uint8_t rssiLog;
	/** linear rssi which has been measured when reading this Tag. */
	int8_t rssiLinI;
	/** linear rssi which has been measured when reading this Tag. */
	int8_t rssiLinQ;
	/** content of AGC and Internal Status Display Register 0x2A after reading a tag. */
	uint8_t agc;
	/** TID length, in bytes */
	uint8_t tidlength;
	/** TID array */
	uint8_t tid[TIDLENGTH];
}invGen2Tag_t;

/** appl command ID for callInventoryGen2Data() */
// CMD_INVENTORY_GEN2_DATA 0x05
void CmdInvGen2Data(CSerialCommHelper *driver, invGen2DataHeader *Gen2Header, invGen2Tag **tagsInfo);

typedef struct scanSettings
{
	unsigned char set : 1;
	unsigned char cyclicInventory : 1;
	unsigned char autoAckMode : 1;
	unsigned char fastInventory : 1;
	unsigned char readTIDinInventoryRound : 1;
	unsigned char scanMode : 1;
	unsigned char padding : 2;
	unsigned char rssiMode;
	unsigned short scanDuration;
}scanSettings_t;

#define SCAN_MODE_SECONDS   0
#define SCAN_MODE_ROUNDS    1

#define RSSI_MODE_REALTIME                  0x00
#define RSSI_MODE_PILOT                     0x04
#define RSSI_MODE_2NDBYTE                   0x06
#define RSSI_MODE_PEAK                      0x08

/** appl command ID for callStartStop() */
// CMD_START_STOP          0x0B
void CmdStartStop(CSerialCommHelper *driver, scanSettings *params, unsigned char *cyclicInventory);

/** appl command ID for callConfigPA() */
// CMD_CONFIG_PA           0x14
void CmdConfigPA(CSerialCommHelper *driver, unsigned char readWriteMode, unsigned char *externalPA);

typedef struct rxSettings
{
	unsigned char wDelayMode;
	unsigned short inventoryDelay;
	unsigned char wQAdjustParamsMode;
	unsigned char adaptiveQ;
	unsigned char adjustmentRounds;
	unsigned char adjustmentUpThreshold;
	unsigned char adjustmentDownThreshold;
	unsigned char wScanTuningParamsMode;
	unsigned short autoTuningInterval;
	unsigned char autoTuningLevel;
	unsigned char autoTuningEnable;
	unsigned char wAdaptiveSensitivityMode;
	unsigned char adaptiveSensitivityEnable;
	unsigned short adaptiveSensitivityInterval;
}rxSettings_t;

/** appl command ID for callInventoryParams() */
// CMD_INV_PARAMS          0x15
void CmdInvParams(CSerialCommHelper *driver, rxSettings *setParams, rxSettings *getParams);

/** appl command ID for tagDetected() */
// CMD_TAG_DETECTED          0x16
int CmdTagDetected(CSerialCommHelper *driver, unsigned short *numTag);

/** appl command ID for readerOnOff() */
// CMD_READER_ON_OFF          0x17
unsigned char CmdReaderOnOff(CSerialCommHelper *driver, unsigned char readerState);