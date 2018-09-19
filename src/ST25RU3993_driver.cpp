//------------------------------------------------------------------------------------------//
//-------------------------------- ST25RU3993 module driver --------------------------------//
//------------------------------------------------------------------------------------------//
#include <assert.h>
#include "ST25RU3993_driver_api.h"

unsigned char gTxBuffer[50] = { 0 };
unsigned char readTIDinInventoryRound = 0;

void encapsulate(const unsigned char protocol,
	const unsigned short tx_size,
	const unsigned short rx_size,
	const unsigned char *data)
{
	static char TID = 0x00;
	char payload = 5 + tx_size;
	char writemask = 0x80;

	gTxBuffer[0] = 0x0F & TID++;
	gTxBuffer[1] = 0x00;
	gTxBuffer[2] = 0x00;
	gTxBuffer[3] = payload;
	gTxBuffer[4] = protocol;
	gTxBuffer[5] = (tx_size >> 8) & 0xFF;
	gTxBuffer[6] = tx_size & 0xFF;
	gTxBuffer[7] = (rx_size >> 8) & 0xFF;
	gTxBuffer[8] = rx_size & 0xFF;

	//write the data after the header
	if (data != NULL && tx_size != 0)
		memcpy(&gTxBuffer[9], data, tx_size);
}

void decapsulate(const char * const rxBuffer,
	char * const TID,
	char * const protocol,
	short * const tx_size,
	short * const rx_size,
	char * const data)
{
	char payload = 0;
	*TID = rxBuffer[0];
	//rxBuffer[1] = 0x00;
	//rxBuffer[2] = 0x00;
	payload = rxBuffer[3];
	*protocol = rxBuffer[4];
	*tx_size = ((((unsigned short)((rxBuffer)[5])) << 8) | (rxBuffer)[6]);
	*rx_size = ((((unsigned short)((rxBuffer)[7])) << 8) | (rxBuffer)[8]);

	//read the data
	if (data != NULL && *rx_size != 0)
		memcpy(data, &rxBuffer[9], *rx_size);
}

int WaitForPacket(CSerialCommHelper *driver, protocolPacket *packet, unsigned long timeout)
{
	assert(driver != NULL);
	std::string rxBuffer;

	if (driver->Read_N(rxBuffer, 4, timeout) != S_OK)
	{
		DEBUG("Wait for packet timeout");
		return -1;
	}

	packet->TID = rxBuffer[0];
	packet->payloadSize = rxBuffer[3];

	if (packet->payloadSize < 5)
	{
		ERROR("Payload size is less than 5");
		return -1;
	}
	rxBuffer.clear();

	while ((driver->Read_N(rxBuffer, packet->payloadSize, timeout) != S_OK));

	if (rxBuffer.size() < packet->payloadSize)
	{
		ERROR("didn't read expected number of bytes");
		return -1;
	}

	packet->protocol.id = rxBuffer[0];
	packet->protocol.txSize = ((((unsigned short)((rxBuffer)[1])) << 8) | (rxBuffer)[2]);
	packet->protocol.rxSize = ((((unsigned short)((rxBuffer)[3])) << 8) | (rxBuffer)[4]);

	packet->protocol.data = (char *)malloc(sizeof(char) * packet->protocol.rxSize);
	//read the data
	if (packet->protocol.data != NULL && packet->protocol.rxSize != 0)
		memcpy(packet->protocol.data, &rxBuffer[5], packet->protocol.rxSize);

	// monitoring the potential missed packet (need to be fixed)
	if (packet->protocol.rxSize + 5 != packet->payloadSize)
		WARNING("Protocol packet have been missed");

	return 0;
}

void ReaderSetConfig(CSerialCommHelper *driver)
{
	// CmdReaderConfig
	boardSettings config;
	CmdReaderConfig(driver, 0, 0, &config);

	// CmdFreqGetFreq
	unsigned char profile, numFreqs;
	unsigned int freqMin, freqMax;
	CmdFreqGetFreq(driver, &profile, &freqMin, &freqMax, &numFreqs);

	// CmdConfigTxRx
	unsigned char setSensi = 0;
	char sensi = 0;
	unsigned char setAntenna = 0;
	unsigned char antenna = 0;
	unsigned short alternateAntennaInterval = 0;
	CmdConfigTxRx(driver, setSensi, &sensi, setAntenna, &antenna, &alternateAntennaInterval);

	// CmdGen2Settings
	Gen2Settings settingsIn, settingsOut;
	memset(&settingsIn, 0, sizeof(Gen2Settings));
	memset(&settingsOut, 0, sizeof(Gen2Settings));
	CmdGen2Settings(driver, &settingsIn, &settingsOut);

	// CmdReadReg Regulator and PA bias register
	char retValue = 0;
	CmdReadReg(driver, 0x0B, &retValue);

	// CmdWriteReg Regulator and PA bias register (value 0x1B = 0b0001-1011)
	// bit 0-2: regulator voltage settings
	// bit 3-5: VDD_PA regulator voltage settings
	// bit 6: Increase internal PA bias four times
	// bit 7: Increase internal PA bias two times
	CmdWriteReg(driver, 0x0B, 0x1B);

	// CmdWriteReg Automatic power supply level setting (this is a direct command register) 
	CmdWriteReg(driver, 0xA2, 0x00);

	//PERSONAL config from here//

	// use PA intern and set output level to -8db (-8+0)db
	unsigned char useExternalPA = 0;
	CmdConfigPA(driver, 1, &useExternalPA);
	CmdWriteReg(driver, 0x15, 0x08);

	// set receive gain to 0dB
	sensi = 0x00; // 0db
	CmdConfigTxRx(driver, 1, &sensi, 0, NULL, 0);

	// disable adaptive sensi
	rxSettings params;
	memset(&params, 0, sizeof(rxSettings));
	CmdInvParams(driver, &params, &params); // read
	params.wAdaptiveSensitivityMode = 1;// disable option
	params.adaptiveSensitivityEnable = 0;
	CmdInvParams(driver, &params, &params); // write
}

void ReaderStartScan(CSerialCommHelper *driver)
{
	// check register 0x0A (Rx mixer and gain register) is equal to 0x60
	char retValue = 0;
	CmdReadReg(driver, 0x0A, &retValue);
	if (retValue != 0x60)
		CmdWriteReg(driver, 0x0A, 0x60);

	// check register 0x09 (Rx filter setting register) is equal to 0x24
	CmdReadReg(driver, 0x09, &retValue);
	if (retValue != 0x24)
		CmdWriteReg(driver, 0x09, 0x24);

	// CmdStartStop
	scanSettings params;
	unsigned char cyclicInventory = 0;

	params.set = 1;
	params.cyclicInventory = 1;
	params.autoAckMode = 0;
	params.fastInventory = 1;
	params.readTIDinInventoryRound = 0;
	params.scanMode = SCAN_MODE_SECONDS;
	params.rssiMode = RSSI_MODE_2NDBYTE;
	params.scanDuration = 0xD4C0; // 0xD4C0

	CmdStartStop(driver, &params, &cyclicInventory);
}

void ReaderStopScan(CSerialCommHelper *driver)
{
	// CmdStartStop RESET all values to zero
	scanSettings params;
	unsigned char cyclicInventory = 0;
	memset(&params, 0, sizeof(scanSettings));
	params.set = 1;
	CmdStartStop(driver, &params, &cyclicInventory);

	// CmdConfigTxRx set sensi
	unsigned char setSensi = 1;
	char sensi = 3;
	unsigned char setAntenna = 0;
	unsigned char antenna = 0;
	unsigned short alternateAntennaInterval = 0x24; // ??
	CmdConfigTxRx(driver, setSensi, &sensi, setAntenna, &antenna, &alternateAntennaInterval);

	// CmdGen2Settings read
	Gen2Settings settingsIn, settingsOut;
	memset(&settingsIn, 0, sizeof(Gen2Settings));
	memset(&settingsOut, 0, sizeof(Gen2Settings));
	CmdGen2Settings(driver, &settingsIn, &settingsOut);

	// 00 09 00 03 00 00 00 01 00 02 00 02 00 00 00 00
	// XX	 XX	   XX	 XX	   XX    XX XX		 XX
	// 01 09 01 03 01 00 01 01 01 02 01 03 00 00 01 00

	settingsOut.setLinkFreq;
	settingsOut.setMiller;
	settingsOut.setSession;
	settingsOut.setTrext;
	settingsOut.setTari;
	settingsOut.setGen2qbegin;
	settingsOut.gen2qbegin = 0x03; // the only param that have changed 
	settingsOut.setTarget;

	// CmdGen2Settings write
	CmdGen2Settings(driver, &settingsOut, &settingsOut);

}

void GetPacketInfo(const char * const rxBuffer,
	char * const TID,
	char * const payload)
{
	*TID = rxBuffer[0];
	//rxBuffer[1] = 0x00;
	//rxBuffer[2] = 0x00;
	*payload = rxBuffer[3];
}

/* protocol ids can range from 0x60 - 0x7f */
// ST_COM_CONFIG                      0x60 /* reserved */                            
// ST_COM_I2C                         0x61
// ST_COM_I2C_CONFIG                  0x62
// ST_COM_SPI                         0x63
// ST_COM_SPI_CONFIG                  0x64
// ST_COM_CTRL_CMD_RESET              0x65

// ST_COM_CTRL_CMD_FW_INFORMATION     0x66 /* returns zero-terminated string with information about fw e.g. chip, board */
void CmdGetFwInfo(CSerialCommHelper *Driver, char *hardware_info, char *software_info)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));

	encapsulate(ST_COM_CTRL_CMD_FW_INFORMATION, 0, 0x40, NULL);
	Driver->Write((const char *)gTxBuffer, 9);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		char *ch;
		ch = strtok(packet.protocol.data, "||");
		strcpy(software_info, ch);
		ch = strtok(NULL, "||");
		strcpy(hardware_info, ch);

		free(packet.protocol.data);
	}
}

int CheckConnection(CSerialCommHelper *Driver)
{
	assert(Driver != NULL);
	char major, minor, rev;
	int version = 0;
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));

	encapsulate(ST_COM_CTRL_CMD_FW_NUMBER, 0, 3, NULL);
	Driver->Write((const char *)gTxBuffer, 9);

	WaitForPacket(Driver, &packet, 1000);

	if (packet.protocol.data != NULL)
	{
		major = packet.protocol.data[0];
		minor = packet.protocol.data[1];
		rev = packet.protocol.data[2];

		version = major * 100 + minor * 10 + rev;

		free(packet.protocol.data);
	}

	if (version == 150)
		return 0;
	else
		return 1;
}

// ST_COM_CTRL_CMD_FW_NUMBER          0x67 /* returns the 3-byte FW number */
void CmdGetFwVersion(CSerialCommHelper *Driver, char *major, char *minor, char *rev)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));

	encapsulate(ST_COM_CTRL_CMD_FW_NUMBER, 0, 3, NULL);
	Driver->Write((const char *)gTxBuffer, 9);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		*major = packet.protocol.data[0];
		*minor = packet.protocol.data[1];
		*rev = packet.protocol.data[2];

		free(packet.protocol.data);
	}
}

// ST_COM_WRITE_REG                   0x68
void CmdWriteReg(CSerialCommHelper *Driver, char addr, char value)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[2];

	dataOut[0] = addr;
	dataOut[1] = value;

	encapsulate(ST_COM_WRITE_REG, 2, 1, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 11);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		DEBUG("ST_COM_WRITE_REG returned: 0x%x\n", packet.protocol.data[0]);
		free(packet.protocol.data);
	}
}

// ST_COM_READ_REG                    0x69
void CmdReadReg(CSerialCommHelper *Driver, char addr, char *retValue)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[2];

	dataOut[0] = 1;
	dataOut[1] = addr;

	encapsulate(ST_COM_READ_REG, 2, 1, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 11);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);
	
	if (packet.protocol.data != NULL)
	{
		*retValue = packet.protocol.data[0];
		DEBUG("ST_COM_READ_REG returned: 0x%x\n", packet.protocol.data[0]);
		free(packet.protocol.data);
	}
}

// ST_COM_READ_REG                    0x69
void CmdReadAllRegs(CSerialCommHelper *Driver, char *regBuffer)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[1];

	dataOut[0] = 0;

	encapsulate(ST_COM_READ_REG, 1, 48, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 11);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		DEBUG("ST_COM_READ_REG (ALL) returned:\n");
		for (int i = 0; i<48; i++)
			DEBUG("Reg[0x%x] : 0x%x\n", i, packet.protocol.data[i]);

		free(packet.protocol.data);
	}
}

/** appl command ID for callReaderConfig() */
// CMD_READER_CONFIG       0x00
void CmdReaderConfig(CSerialCommHelper *Driver,
	char changePowerDown, char value,
	boardSettings *config)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_READER_CONFIG_RX_SIZE];

	dataOut[0] = changePowerDown;
	dataOut[1] = value;

	encapsulate(CMD_READER_CONFIG, CMD_READER_CONFIG_RX_SIZE, CMD_READER_CONFIG_REPLY_SIZE, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 9 + CMD_READER_CONFIG_RX_SIZE);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		config->Satus = packet.protocol.data[0];
		config->VCO = packet.protocol.data[1];
		config->PA = packet.protocol.data[2];
		config->INP = packet.protocol.data[3];
		config->ANT = packet.protocol.data[4];
		config->Tuner = packet.protocol.data[5];
		config->pow_down = packet.protocol.data[6];
		config->HW_ID = packet.protocol.data[7];

		free(packet.protocol.data);
	}
}

/** appl command ID for callAntennaPower() */
// CMD_ANTENNA_POWER       0x01

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_RSSI				0x00
void CmdFreqRssi(CSerialCommHelper *Driver, unsigned int freq, unsigned char * rssiLog)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_CHANGE_FREQ_RSSI_RX_SIZE];

	dataOut[0] = SUBCMD_CHANGE_FREQ_RSSI;

	// frequency
	dataOut[1] = freq & 0xFF;
	dataOut[2] = (freq >> 8) & 0xFF;
	dataOut[3] = (freq >> 16) & 0xFF;

	DEBUG("%s(): f=%d, subcmd=0x%02X\n", freq, dataOut[0]);

	encapsulate(CMD_CHANGE_FREQ, CMD_CHANGE_FREQ_RSSI_RX_SIZE, CMD_CHANGE_FREQ_RSSI_REPLY_SIZE, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 9 + CMD_CHANGE_FREQ_RSSI_RX_SIZE);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		*rssiLog = 0;
		*rssiLog += packet.protocol.data[0] & 0x0F;
		*rssiLog += (packet.protocol.data[1] << 4) & 0xF0;
		DEBUG("RSSI Log: %d", *rssiLog);

		free(packet.protocol.data);
	}
}

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_REFL				0x01
void CmdFreqRefl(CSerialCommHelper *Driver, unsigned int freq, unsigned char useTunerSettings, unsigned short * iqTogetherNoise, unsigned short * iqTogetherReflected)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_CHANGE_FREQ_REFL_RX_SIZE];

	dataOut[0] = SUBCMD_CHANGE_FREQ_REFL;

	// frequency
	dataOut[1] = freq & 0xFF;
	dataOut[2] = (freq >> 8) & 0xFF;
	dataOut[3] = (freq >> 16) & 0xFF;
	dataOut[4] = useTunerSettings;

	DEBUG("%s(): f=%d, subcmd=0x%02X\n", freq, dataOut[0]);

	encapsulate(CMD_CHANGE_FREQ, CMD_CHANGE_FREQ_REFL_RX_SIZE, CMD_CHANGE_FREQ_REFL_REPLY_SIZE, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 9 + CMD_CHANGE_FREQ_REFL_RX_SIZE);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		*iqTogetherReflected = 0;
		*iqTogetherReflected += packet.protocol.data[0];					// I 
		*iqTogetherReflected += (packet.protocol.data[1] << 8) && 0xFF00;	// Q

		*iqTogetherNoise = 0;
		*iqTogetherNoise += packet.protocol.data[2];					// I 
		*iqTogetherNoise += (packet.protocol.data[3] << 8) && 0xFF00;	// Q

		free(packet.protocol.data);
	}
}

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_ADD				0x02
void CmdFreqAdd(CSerialCommHelper *Driver, unsigned int freq, unsigned char clearFrequency, unsigned char profile)
{
	assert(Driver != NULL);
	char dataOut[CMD_CHANGE_FREQ_ADD_RX_SIZE];

	dataOut[0] = SUBCMD_CHANGE_FREQ_ADD;

	// frequency
	dataOut[1] = freq & 0xFF;
	dataOut[2] = (freq >> 8) & 0xFF;
	dataOut[3] = (freq >> 16) & 0xFF;

	dataOut[4] = clearFrequency; // clear frequency list before adding:  0 = NO, 1 = YES
	dataOut[5] = profile; // PROFILE_EUROPE_CERTIFIED etc ...

	DEBUG("%s(): f=%d, subcmd=0x%02X\n", freq, dataOut[0]);

	encapsulate(CMD_CHANGE_FREQ, CMD_CHANGE_FREQ_ADD_RX_SIZE, CMD_CHANGE_FREQ_ADD_REPLY_SIZE, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 9 + CMD_CHANGE_FREQ_ADD_RX_SIZE);
}

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_GETFREQ          0x03
void CmdFreqGetFreq(CSerialCommHelper *Driver, unsigned char *profile, unsigned int *freqMin, unsigned int *freqMax, unsigned char *numFreqs)
{
	assert(Driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_CHANGE_FREQ_GETFREQ_RX_SIZE];

	dataOut[0] = SUBCMD_CHANGE_FREQ_GETFREQ;

	encapsulate(CMD_CHANGE_FREQ, CMD_CHANGE_FREQ_GETFREQ_RX_SIZE, CMD_CHANGE_FREQ_GETFREQ_REPLY_SIZE, (const unsigned char *)dataOut);
	Driver->Write((const char *)gTxBuffer, 9 + CMD_CHANGE_FREQ_GETFREQ_RX_SIZE);

	while (WaitForPacket(Driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		*profile = packet.protocol.data[0];

		*freqMin = 0;
		*freqMin += (unsigned int)packet.protocol.data[1];
		*freqMin += ((unsigned int)packet.protocol.data[2]) << 8;
		*freqMin += ((unsigned int)packet.protocol.data[3]) << 16;

		*freqMax = 0;
		*freqMax += (unsigned int)packet.protocol.data[4];
		*freqMax += ((unsigned int)packet.protocol.data[5]) << 8;
		*freqMax += ((unsigned int)packet.protocol.data[6]) << 16;

		*numFreqs = packet.protocol.data[6];

		free(packet.protocol.data);
	}
}
/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_SETHOP			0x04

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_GETHOP			0x05

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_CONTMOD          0x06

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_SETLBT           0x07

/** appl command ID for callChangeFreq() */
// CMD_CHANGE_FREQ         0x02
// SUBCMD_CHANGE_FREQ_GETLBT           0x08

/** appl command ID for callConfigGen2() */
// CMD_GEN2_SETTINGS       0x03
void CmdGen2Settings(CSerialCommHelper *driver, Gen2Settings *settingsIn, Gen2Settings *settingsOut)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_GEN2_SETTINGS_RX_SIZE] = { 0 };

	if (settingsIn != NULL)
	{
		dataOut[0] = settingsIn->setLinkFreq;
		dataOut[1] = settingsIn->linkFreq;
		dataOut[2] = settingsIn->setMiller;
		dataOut[3] = settingsIn->miller;
		dataOut[4] = settingsIn->setSession;
		dataOut[5] = settingsIn->session;
		dataOut[6] = settingsIn->setTrext;
		dataOut[7] = settingsIn->trext;
		dataOut[8] = settingsIn->setTari;
		dataOut[9] = settingsIn->tari;
		dataOut[10] = settingsIn->setGen2qbegin;
		dataOut[11] = settingsIn->gen2qbegin;
		dataOut[12] = settingsIn->setSel;
		dataOut[13] = settingsIn->sel;
		dataOut[14] = settingsIn->setTarget;
		dataOut[15] = settingsIn->target;
	}

	encapsulate(CMD_GEN2_SETTINGS, CMD_GEN2_SETTINGS_RX_SIZE, CMD_GEN2_SETTINGS_REPLY_SIZE, (const unsigned char *)dataOut);
	driver->Write((const char *)gTxBuffer, 9 + CMD_GEN2_SETTINGS_RX_SIZE);

	while ( (WaitForPacket(driver, &packet, READ_TIMEOUT) != 0) && (packet.protocol.id != CMD_GEN2_SETTINGS) );

	if (settingsOut != NULL && packet.protocol.data != NULL)
	{
		settingsOut->linkFreq = packet.protocol.data[1];
		settingsOut->miller = packet.protocol.data[3];
		settingsOut->session = packet.protocol.data[5];
		settingsOut->trext = packet.protocol.data[7];
		settingsOut->tari = packet.protocol.data[9];
		settingsOut->gen2qbegin = packet.protocol.data[11];
		settingsOut->sel = packet.protocol.data[13];
		settingsOut->target = packet.protocol.data[15];
	}
}

/** appl command ID for callConfigTxRx() */
// CMD_CONFIG_TX_RX        0x04
void CmdConfigTxRx(CSerialCommHelper *driver,
	unsigned char setSensi, char *sensi,
	unsigned char setAntenna, unsigned char *antenna, unsigned short *alternateAntennaInterval)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_CONFIG_TX_RX_RX_SIZE] = { 0 };

	dataOut[0] = setSensi;
	if (sensi != NULL)
		dataOut[1] = *sensi;
	dataOut[2] = setAntenna;
	if (antenna != NULL)
		dataOut[3] = *antenna;
	if (alternateAntennaInterval != NULL)
	{
		dataOut[4] = (*alternateAntennaInterval >> 8) & 0xFF;
		dataOut[5] = *alternateAntennaInterval & 0xFF;
	}

	encapsulate(CMD_CONFIG_TX_RX, CMD_CONFIG_TX_RX_RX_SIZE, CMD_CONFIG_TX_RX_REPLY_SIZE, (const unsigned char *)dataOut);
	
	do
	{
		driver->Write((const char *)gTxBuffer, 9 + CMD_CONFIG_TX_RX_RX_SIZE);
		WaitForPacket(driver, &packet, 2000);
	} while (packet.protocol.id != CMD_CONFIG_TX_RX);

	if (packet.protocol.data != NULL)
	{
		if (sensi != NULL)
			*sensi = packet.protocol.data[1];
		if (antenna != NULL)
			*antenna = packet.protocol.data[3];
		if (alternateAntennaInterval != NULL)
		{
			*alternateAntennaInterval = 0;
			*alternateAntennaInterval += (unsigned short)packet.protocol.data[5];
			*alternateAntennaInterval += ((unsigned short)packet.protocol.data[4]) << 8;
		}

		free(packet.protocol.data);
	}
}

/** appl command ID for callInventoryGen2Data() */
// CMD_INVENTORY_GEN2_DATA 0x05
void CmdInvGen2Data(CSerialCommHelper *driver, invGen2DataHeader *Gen2Header, invGen2Tag **tagsInfo)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_INVENTORY_GEN2_DATA_RX_SIZE];

	dataOut[0] = 0x01;

	encapsulate(CMD_INVENTORY_GEN2_DATA, CMD_INVENTORY_GEN2_DATA_RX_SIZE, CMD_INVENTORY_GEN2_DATA_REPLY_SIZE, (const unsigned char *)dataOut);
	driver->Write((const char *)gTxBuffer, 9 + CMD_INVENTORY_GEN2_DATA_RX_SIZE);

	while (WaitForPacket(driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data == NULL)
	{
		DEBUG("Allocation error");
		return;
	}

	// Get the constant header
	Gen2Header->cyclicInventory = packet.protocol.data[0];
	Gen2Header->availableTagSlot = packet.protocol.data[1];
	Gen2Header->numTagsConnected = packet.protocol.data[2];
	Gen2Header->tunningStatus = packet.protocol.data[3];
	Gen2Header->roundCounter = 0;
	Gen2Header->roundCounter += ((unsigned short)packet.protocol.data[4] << 8);
	Gen2Header->roundCounter += (unsigned short)packet.protocol.data[5];
	Gen2Header->sensitivity = packet.protocol.data[6];
	Gen2Header->gen2qbegin = packet.protocol.data[7];
	Gen2Header->adc = 0;
	Gen2Header->adc = ((unsigned short)packet.protocol.data[8] << 8);
	Gen2Header->adc = (unsigned short)packet.protocol.data[9];
	Gen2Header->frequency = packet.protocol.data[10];
	Gen2Header->frequency += ((unsigned int)packet.protocol.data[11] << 8);
	Gen2Header->frequency += ((unsigned int)packet.protocol.data[12] << 16);

	*tagsInfo = (invGen2Tag*)malloc(sizeof(invGen2Tag)*Gen2Header->numTagsConnected);
	if (*tagsInfo == NULL)
	{
		DEBUG("Allocation error");
		return;
	}

	int offset = CMD_INVENTORY_GEN2_DATA_REPLY_SIZE; // index in buffer where data for next tag can be put
	int element = 0;

	// Get Tags info
	while (element < Gen2Header->numTagsConnected)
	{
		tagsInfo[element]->agc = packet.protocol.data[offset + 0];
		tagsInfo[element]->rssiLog = packet.protocol.data[offset + 1];
		tagsInfo[element]->rssiLinI = packet.protocol.data[offset + 2];
		tagsInfo[element]->rssiLinQ = packet.protocol.data[offset + 3];
		tagsInfo[element]->epclen = packet.protocol.data[offset + 4];
		tagsInfo[element]->pc[0] = packet.protocol.data[offset + 5];
		tagsInfo[element]->pc[1] = packet.protocol.data[offset + 6];

		memcpy(tagsInfo[element]->epc, &packet.protocol.data[offset + SENDTAGFIXDATALEN], tagsInfo[element]->epclen - 2);

		if (readTIDinInventoryRound)
		{
			tagsInfo[element]->tidlength = packet.protocol.data[offset + SENDTAGFIXDATALEN + tagsInfo[element]->epclen];
			memcpy(tagsInfo[element]->tid, &packet.protocol.data[offset + SENDTAGFIXDATALEN + tagsInfo[element]->epclen + 1], tagsInfo[element]->tidlength);
			offset += tagsInfo[element]->tidlength + 1;
		}

		offset += SENDTAGFIXDATALEN + tagsInfo[element]->epclen;
		element++;
	}

	if (*tagsInfo != NULL)
		free(*tagsInfo);

	if (packet.protocol.data != NULL)
		free(packet.protocol.data);
}

/** appl command ID for callSelectTag() */
// CMD_SELECT_TAG          0x06

/** appl command ID for callWriteToTag() */
// CMD_WRITE_TO_TAG        0x07

/** appl command ID for callReadFromTag() */
// CMD_READ_FROM_TAG       0x08

/** appl command ID for callLockUnlockTag() */
// CMD_LOCK_UNLOCK_TAG     0x09

/** appl command ID for callKillTag() */
// CMD_KILL_TAG            0x0A

/** appl command ID for callStartStop() */
// CMD_START_STOP          0x0B
void CmdStartStop(CSerialCommHelper *driver, scanSettings *params, unsigned char *cyclicInventory)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_START_STOP_RX_SIZE];

	dataOut[0] = params->set |
		(params->cyclicInventory << 1) |
		(params->autoAckMode << 2) |
		(params->fastInventory << 3) |
		(params->readTIDinInventoryRound << 3) |
		(params->scanMode << 4);
	dataOut[1] = params->rssiMode;
	dataOut[2] = (params->scanDuration >> 8) & 0xFF;
	dataOut[3] = params->scanDuration & 0xFF;

	encapsulate(CMD_START_STOP, CMD_START_STOP_RX_SIZE, CMD_START_STOP_REPLY_SIZE, (const unsigned char *)dataOut);
	driver->Write((const char *)gTxBuffer, 9 + CMD_START_STOP_RX_SIZE);
	
	do
	{
		driver->Write((const char *)gTxBuffer, 9 + CMD_START_STOP_RX_SIZE);
		WaitForPacket(driver, &packet, READ_TIMEOUT);
	}while (packet.protocol.id != CMD_START_STOP);

	std::string buff;
	do
	{
		driver->ReadAvailable(buff);
	}
	while (buff.size() != 0);

	if (packet.protocol.data != NULL)
	{
		*cyclicInventory = packet.protocol.data[0];
		free(packet.protocol.data);
	}
}

/** appl command ID for callTunerTable() */
// CMD_TUNER_TABLE         0x0C
// SUBCMD_TUNER_TABLE_ADD   		    0x00
// SUBCMD_TUNER_TABLE_DEFAULT          0x01
// SUBCMD_TUNER_TABLE_SAVE             0x02

/** appl command ID for callTuning() */
// CMD_TUNING              0x0D

/** appl command ID for callGetSetTuning() */
// CMD_GET_SET_TUNING      0x0E

/** appl command ID for callInventory6B() */
// CMD_INVENTORY_6B        0x0F

/** appl command ID for callReadFromTag6B() */
// CMD_READ_FROM_TAG_6B    0x10

/** appl command ID for callWriteToTag6B() */
// CMD_WRITE_TO_TAG_6B     0x11

/** appl command ID for callGenericCMD() */
// CMD_GENERIC_CMD         0x12

/** appl command ID for callRSSIMeasureCMD() */
// CMD_RSSI_MEAS_CMD       0x13

/** appl command ID for callConfigPA() */
// CMD_CONFIG_PA           0x14
void CmdConfigPA(CSerialCommHelper *driver, unsigned char readWriteMode, unsigned char *externalPA)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_CONFIG_PA_RX_SIZE];

	dataOut[0] = readWriteMode;
	dataOut[1] = *externalPA;

	encapsulate(CMD_CONFIG_PA, CMD_CONFIG_PA_RX_SIZE, CMD_CONFIG_PA_REPLY_SIZE, (const unsigned char *)dataOut);
	driver->Write((const char *)gTxBuffer, 9 + CMD_CONFIG_PA_RX_SIZE);

	while (WaitForPacket(driver, &packet, READ_TIMEOUT) != 0);

	if (packet.protocol.data != NULL)
	{
		*externalPA = packet.protocol.data[0];
		free(packet.protocol.data);
	}
}

/** appl command ID for callInventoryParams() */
// CMD_INV_PARAMS          0x15
void CmdInvParams(CSerialCommHelper *driver, rxSettings *setParams, rxSettings *getParams)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_INV_PARAMS_RX_SIZE] = { 0 };

	if (setParams != NULL)
	{
		dataOut[0] = setParams->wDelayMode;
		dataOut[1] = setParams->inventoryDelay >> 8;
		dataOut[2] = setParams->inventoryDelay;
		dataOut[3] = setParams->wQAdjustParamsMode;
		dataOut[4] = setParams->adaptiveQ;
		dataOut[5] = setParams->adjustmentRounds;
		dataOut[6] = setParams->adjustmentUpThreshold;
		dataOut[7] = setParams->adjustmentDownThreshold;
		dataOut[8] = setParams->wScanTuningParamsMode;
		dataOut[9] = setParams->autoTuningInterval >> 8;
		dataOut[10] = setParams->autoTuningInterval;
		dataOut[11] = setParams->autoTuningLevel;
		dataOut[12] = setParams->autoTuningEnable;
		dataOut[13] = setParams->wAdaptiveSensitivityMode;
		dataOut[14] = setParams->adaptiveSensitivityEnable;
		dataOut[15] = setParams->adaptiveSensitivityInterval >> 8;
		dataOut[16] = setParams->adaptiveSensitivityInterval;
	}

	encapsulate(CMD_INV_PARAMS, CMD_INV_PARAMS_RX_SIZE, CMD_INV_PARAMS_REPLY_SIZE, (const unsigned char *)dataOut);
	driver->Write((const char *)gTxBuffer, 9 + CMD_INV_PARAMS_RX_SIZE);

	while (WaitForPacket(driver, &packet, READ_TIMEOUT) != 0);

	if (getParams != NULL && packet.protocol.data != NULL)
	{
		getParams->inventoryDelay = ((unsigned short)packet.protocol.data[0] << 8) & 0xFF00;
		getParams->inventoryDelay |= ((unsigned short)packet.protocol.data[1]) & 0xFF;
		getParams->adaptiveQ = packet.protocol.data[2];
		getParams->adjustmentRounds = packet.protocol.data[3];
		getParams->adjustmentUpThreshold = packet.protocol.data[4];
		getParams->adjustmentDownThreshold = packet.protocol.data[5];
		getParams->autoTuningInterval = ((unsigned short)packet.protocol.data[6] << 8) & 0xFF00;
		getParams->autoTuningInterval |= ((unsigned short)packet.protocol.data[7]) & 0xFF;
		getParams->autoTuningLevel = packet.protocol.data[8];
		getParams->autoTuningEnable = packet.protocol.data[9];
		getParams->adaptiveSensitivityEnable = packet.protocol.data[10];
		getParams->adaptiveSensitivityInterval = ((unsigned short)packet.protocol.data[11] << 8) & 0xFF00;
		getParams->adaptiveSensitivityInterval |= ((unsigned short)packet.protocol.data[12]) & 0xFF;

		DEBUG("%s(): delay=%d, adaptive Q=%d\n", __FUNCTION__, (packet.protocol.data[0] << 8) | packet.protocol.data[1], packet.protocol.data[2]);
		DEBUG("addRounds=%d, adjust up thh=%d, adjust up thh=%d\n", packet.protocol.data[3], packet.protocol.data[4], packet.protocol.data[5]);
		DEBUG("scan tuning interval=%d, retuning level=%d\n", (packet.protocol.data[6] << 8) | packet.protocol.data[7], packet.protocol.data[8]);

		free(packet.protocol.data);
	}
}

/** appl command ID for tagDetected() */
// CMD_TAG_DETECTED          0x16
int CmdTagDetected(CSerialCommHelper *driver, unsigned short *numTag)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));

	WaitForPacket(driver, &packet, 1000);

	if (packet.protocol.id != 0x16)
		return -1;
	if (packet.protocol.data != NULL)
	{
		*numTag = (packet.protocol.data[0] << 8) & 0xFF00;
		*numTag += (packet.protocol.data[1] & 0xFF);
		free(packet.protocol.data);
	}

	return 0;
}

/** appl command ID for readerOnOff() */
// CMD_READER_ON_OFF          0x17
unsigned char CmdReaderOnOff(CSerialCommHelper *driver, unsigned char readerState)
{
	assert(driver != NULL);
	protocolPacket packet;
	memset(&packet, 0, sizeof(protocolPacket));
	char dataOut[CMD_READER_ON_OFF_RX_SIZE];

	dataOut[0] = readerState;

	encapsulate(CMD_READER_ON_OFF, CMD_READER_ON_OFF_RX_SIZE, CMD_READER_ON_OFF_REPLY_SIZE, (const unsigned char *)dataOut);
	driver->Write((const char *)gTxBuffer, 9 + CMD_READER_ON_OFF_RX_SIZE);

	while (WaitForPacket(driver, &packet, READ_TIMEOUT) != 0);
	
	readerState = 0;

	if (packet.protocol.data != NULL)
	{
		readerState = packet.protocol.data[0];
		free(packet.protocol.data);
	}

	return readerState;
}