#include <rfid.h>
#include <assert.h>
#include "ST25RU3993_driver_api.h"
#include "commands_application.h"
#include "st_stream.h"

RFID::RFID(	SafeFIFO<packet_t> *fifo_in, 
		SafeFIFO<packet_t> *fifo_out)
{
	assert(fifo_in != NULL);
	assert(fifo_out != NULL);

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
                m_serial.Stop();
                m_serial.UnInit();
                perror("rfid connection\n");
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

}

void RFID::threadWorker(void)
{
	packet_t cmd;
	rfid_cmd_t rfid_cmd;

	while(this->m_continue)
	{
		//check if there is any command to dequeu
		while(!m_fifo_in->Empty())
		{
			// get the command execute the approriate action
			m_fifo_in->Read(cmd);
			decapsulate(cmd, rfid_cmd);
			execute(rfid_cmd);
		}
	}
}

void RFID::threadScan(void)
{
	uint16_t tagDetected = 0;
	packet_t info;

	info.type = INFO_RFID;
	info.size = 4;
	info.data[0] = CMD_TAG_DETECTED; // type
	info.data[1] = 2; // argc

	CmdReaderOnOff(&m_serial, 1);

	while(this->m_status == RFID_SCANNING)
	{
                if (!CmdTagDetected(&(this->m_serial), &tagDetected))
		{
			info.data[2] = (tagDetected >> 8) & 0xFF; // argv[0]
			info.data[3] = tagDetected & 0xFF; // argv[1]
			m_fifo_out->Write(info);
		}
	}

	CmdReaderOnOff(&m_serial, 0);
	printf("End thread scan\n");
}

void RFID::decapsulate(packet_t &command, rfid_cmd_t &rfid_cmd)
{
	printf("enter %s\n", __FUNCTION__);
	rfid_cmd.type = command.data[0];
	rfid_cmd.argc = command.data[1];
	for(uint32_t i=0; i<rfid_cmd.argc; i++)
		rfid_cmd.args[i] = command.data[i+2];
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
				m_status = RFID_IDLE;
			}
		break;

		default:
		printf("default\n");

	}
}