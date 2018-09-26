#include <rfid.h>
#include <assert.h>
#include <iostream>
#include <chrono>
#include <wiringPi.h>
#include "ST25RU3993_driver_api.h"
#include "commands_application.h"
#include "st_stream.h"

RFID::RFID(	SafeFIFO<packet_t> *fifo_in, 
		SafeFIFO<packet_t> *fifo_out)
{
	assert(fifo_in != NULL);
	assert(fifo_out != NULL);

	// reset RFID board
	pinMode(2, OUTPUT);
	digitalWrite(2, LOW);
	delay(500);
	digitalWrite(2, HIGH);
	delay(500);

	m_fifo_in = fifo_in;
	m_fifo_out = fifo_out;

	m_status = RFID_DISCONECTED;

	m_serial = CSerialCommHelper();
        if (m_serial.Init("/dev/serial0", 115200, 0, 1, 8))
        {
        	perror("serial init\n");
        }
        printf("serial opened\n");

        m_serial.Start();
        printf("serial started\n");

        if(CheckConnection(&m_serial))
        {
        	printf("Error: RFID board not detected\n");
                m_serial.Stop();
                m_serial.UnInit();
        }
        printf("RFID board detected\n");
        m_status = RFID_IDLE;

        m_continue = true;
        m_threadWorker = std::thread(&RFID::threadWorker, this);
}

RFID::~RFID()
{
	m_threadWorker.join();
}

void RFID::Init()
{

}

void RFID::DeInit()
{
	m_continue = false;
	printf("Stop RFID\n");
}

void RFID::threadWorker(void)
{
	packet_t cmd;
	rfid_cmd_t rfid_cmd;
	std::string rxBuffer;
	std::vector<uint8_t> rxData;
	rxBuffer.clear();
	rxData.clear();
	uint8_t old_status = 0;

	while(this->m_continue)
	{
		//check if there is any command to dequeu
		while(!m_fifo_in->Empty())
		{
			// get the command execute the approriate action
			m_fifo_in->Read(cmd);
			decapsulate(cmd, rfid_cmd);
		}

		if(old_status != this->m_status)
		{
			printf("RFID status %d\n",this->m_status);
			old_status = this->m_status;	
		}

		// If we are not scanning read from serial
		if(this->m_status == RFID_IDLE)
		{
			if (this->m_serial.ReadAvailable(rxBuffer) == S_OK && rxBuffer.size() != 0)
			{
				printf("%d byte(s) read from serial\n",rxBuffer.size());
				printf("Data before insertion: ");
				for(uint32_t i=0; i<rxData.size() ;i++)
					printf("0x%x ",rxData[i]);
				printf("\n");
				std::copy(rxBuffer.begin(), rxBuffer.end(), std::back_inserter(rxData));
				// check if we have received a full packet already
				// this assume we don't skip a packet and we never get mixed up (further check might be needed)
				if(rxData.size() >= 7 && rxData.size() >= (uint8_t)(rxData[3]+4))
				{
					uint16_t size = rxData[3]+4;

					printf("Data before encapsulate: ");
					for(uint32_t i=0; i<rxData.size() ;i++)
						printf("0x%x ",rxData[i]);
					printf("\n");
					encapsulate(RFID_DIRECT, size, (uint8_t*)rxData.data());
					rxData.erase(rxData.begin(), rxData.begin()+size);
					
					printf("Data after deletion: ");
					for(uint32_t i=0; i<rxData.size() ;i++)
						printf("0x%x ",rxData[i]);
					printf("\n");
				}
			}
		}
	}
}

void RFID::threadScan(void)
{
	using namespace std::chrono;
	milliseconds ms;
	uint64_t timestamp;

	uint16_t tagDetected = 0;
	packet_t info;
	rfid_scan_t scanData; 

	info.type = INFO_RFID;
	info.size = sizeof(scanData)+1;
	info.data[0] = CMD_TAG_DETECTED; 


	CmdReaderOnOff(&m_serial, 1);

	while(this->m_status == RFID_SCANNING)
	{
                if (!CmdTagDetected(&(this->m_serial), &tagDetected))
		{
			ms = duration_cast< milliseconds >(
    			system_clock::now().time_since_epoch());
    			timestamp = ms.count();
    			//printf("%lld %lld\n",ms, timestamp);
			
			scanData.timestamp = timestamp;
			scanData.tagCount = tagDetected;

			memcpy(info.data+1, (void*)&scanData, sizeof(scanData));
			printf("RFID scan write fifo out\n");
			m_fifo_out->Write(info);
			printf("RFID scan write fifo out done\n");
		}
	}

	CmdReaderOnOff(&m_serial, 0);
	this->m_status = RFID_IDLE;
	printf("End thread scan %d\n",this->m_status);
}

void RFID::encapsulate(uint8_t type, uint16_t size, uint8_t* data)
{
	packet_t info;

	printf("enter %s\n", __FUNCTION__);
	if(type == CMD_RFID)
	{
		printf("CMD_RFID not handle from device to host\n");
	}
	else if(type == RFID_DIRECT)
	{
		printf("RFID_DIRECT device -> host\n");
		info.type = RFID_DIRECT;
		info.size = size;
		memcpy(info.data, (void*)data, size);
	}

	m_fifo_out->Write(info);
}

void RFID::decapsulate(packet_t &command, rfid_cmd_t &rfid_cmd)
{
	printf("enter %s\n", __FUNCTION__);
	if(command.type == CMD_RFID)
	{
		rfid_cmd.type = command.data[0];
		rfid_cmd.argc = command.data[1];
		for(uint32_t i=0; i<rfid_cmd.argc; i++)
			rfid_cmd.args[i] = command.data[i+2];

		execute(rfid_cmd);
	}
	else if(command.type == RFID_DIRECT)
	{
		printf("RFID_DIRECT host -> device\n");
		for(uint32_t i=0; i<command.size ;i++)
			printf("0x%x ",command.data[i]);
		printf("\n");

		m_serial.Write((const char*)command.data, command.size);
	}
}

void RFID::execute(rfid_cmd_t &cmd)
{
	printf("enter %s\n", __FUNCTION__);
	if((m_status == RFID_SCANNING) && (cmd.type != CMD_READER_ON_OFF))
	{
		printf("The command can not be executed\n");
	}

	switch(cmd.type)
	{
		case CMD_READER_ON_OFF:
			printf("CMD_READER_ON_OFF arg:%d\n",cmd.args[0]);
			if(cmd.args[0] == 1)
			{
				m_status = RFID_SCANNING;
				m_threadScan = std::thread(&RFID::threadScan, this);
				m_threadScan.detach();
				printf("threadScan detached\n");

			}
			else
			{
				m_status = RFID_STOPSCANN;
			}
		break;

		default:
		printf("default\n");

	}
}