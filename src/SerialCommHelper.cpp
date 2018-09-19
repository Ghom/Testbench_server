// RCSerial.cpp: implementation of the CSerialCommHelper class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <assert.h>
#include <sys/ioctl.h>

#include "SerialCommHelper.h"


thread_event wait_for_signal(thread_signal_t* sigHandler, unsigned long event_flags, unsigned int waittime)
{
	struct timespec ts;

	if(waittime != 0)
	{
		event_flags |= TIME_OUT;
	}

	while((sigHandler->event_flag & event_flags) == 0)
	{
		if(waittime == 0)
		{
			pthread_cond_wait (&(sigHandler->event), &(sigHandler->lock));
		}
		else
		{
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec += waittime;
			if(pthread_cond_timedwait(&(sigHandler->event), &(sigHandler->lock), &ts) == ETIMEDOUT)
				sigHandler->event_flag |= TIME_OUT;
		}
	}
	
	for(int event=1; event<TH_EVENT_MAX; event*=2)
	{
		if(event & sigHandler->event_flag & event_flags)
		{
			// sigHandler->event_flag &= ~event;
			return (thread_event)event;
		}
	}

	return TH_EVENT_NONE;
}

int send_signal(thread_signal_t* sigHandler, thread_event event)
{
	sigHandler->event_flag |= (int)event;
	return pthread_cond_signal (&(sigHandler->event));
}

void reset_signal(thread_signal_t* sigHandler, thread_event event)
{
	sigHandler->event_flag &= ~event;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

// void CSerialCommHelper::InvalidateHandle(HANDLE& hHandle )
// {

// }


// void CSerialCommHelper::CloseAndCleanHandle(HANDLE& hHandle)
// {

// }

CSerialCommHelper::CSerialCommHelper()
{
	InitLock();
	m_eState = SS_UnInit;
}

CSerialCommHelper::~CSerialCommHelper()
{
	m_eState = SS_Unknown;
	DelLock();
}

speed_t termiosSpeed(uint32_t baudrate)
{
	switch(baudrate)
	{
        	case 0:
        		return B0;
        	case 50:
        		return B50;
        	case 75:
        		return B75;
        	case 110:
        		return B110;
        	case 134:
        		return B134;
        	case 150:
        		return B150;
        	case 200:
        		return B200;
        	case 300:
        		return B300;
        	case 600:
        		return B600;
        	case 1200:
        		return B1200;
        	case 1800:
        		return B1800;
        	case 2400:
        		return B2400;
        	case 4800:
        		return B4800;
        	case 9600:
        		return B9600;
        	case 19200:
        		return B19200;
        	case 38400:
        		return B38400;
        	case 57600:
        		return B57600;
        	case 115200:
        		return B115200;
        	case 230400:
        		return B230400;
		default:
			ERROR("The serial speed choosen is not recognised (%d)",baudrate);
			return -1;
	}

	return -1;
}

uint32_t termiosDataSize(uint8_t dataSize)
{
	switch(dataSize)
	{
		case 5:
			return CS5;
			break;
		case 6:
			return CS6;
			break;
		case 7:
			return CS7;
			break;
		case 8:
			return CS8;
			break;
		default:
			ERROR("The serial data size is not recognised (%d)",dataSize);
			return -1;
	}
	return -1;
}

long CSerialCommHelper:: Init(std::string portName, speed_t baudRate, unsigned char parity, unsigned char stopBits, unsigned char byteSize)
{
	
	long hr = S_OK;
	try
	{
		//open the COM Port
		m_fdCommPort = open(portName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK); 
		if ( m_fdCommPort < 0 )
		{
			DEBUG ( "CSerialCommHelper: Failed to open COM Port Reason: %s",strerror(errno));
			return E_FAIL;
		}

		
		DEBUG ( "CSerialCommHelper: COM port opened successfully" );
		
	        struct termios tty;

	        memset (&tty, 0, sizeof tty);
	        if (tcgetattr (m_fdCommPort, &tty) != 0) /* save current serial port settings */
	        {
	                printf("__LINE__ = %d, error %s\n", __LINE__, strerror(errno));
	                return -1;
	        }

	        cfsetspeed (&tty, termiosSpeed(baudRate));

	        tty.c_cflag = (tty.c_cflag & ~CSIZE) | termiosDataSize(byteSize);     // set data size
	        // disable IGNBRK for mismatched speed tests; otherwise receive break
	        // as \000 chars
	        tty.c_iflag &= ~IGNBRK;         // disable break processing
	        tty.c_lflag = 0;                // no signaling chars, no echo,

	        tty.c_oflag = 0;                // no remapping, no delays
	        tty.c_cc[VMIN]  = 1;            // read doesn't block
	        tty.c_cc[VTIME] = 0;   	// in unit of 100 milli-sec for set timeout value

	        tty.c_iflag &= ~(IXON | IXOFF | IXANY); // flow control: None

	        tty.c_cflag |= (CLOCAL | CREAD); // ignore modem controls, enable reading
	        tty.c_cflag &= ~(PARENB | PARODD); // set parity
	        tty.c_cflag |= parity;
	        if(stopBits == 1)
	        	tty.c_cflag &= ~CSTOPB;
	        else if(stopBits == 2)
	        	tty.c_cflag |= CSTOPB;
	        tty.c_cflag &= ~CRTSCTS;

	        if (tcsetattr (m_fdCommPort, TCSANOW, &tty) != 0)
	        {
	                ERROR("__LINE__ = %d, error %s\n", __LINE__, strerror(errno));
	                return E_FAIL;
	        }

		pthread_create(&m_Thread, NULL, CSerialCommHelper::ThreadFn, (void *)this);
		wait_for_signal(&m_signalThread, TH_EVENT_STARTED);
		
		m_abIsConnected = true;
		
	}
	catch(...)
	{
		assert(0);
		hr = E_FAIL;
	}
	if ( hr == S_OK ) 
	{
		m_eState = SS_Init;
	}
	return hr;
}

long CSerialCommHelper:: Start()
{
	m_eState = SS_Started;
	return S_OK;
}

long CSerialCommHelper:: Stop()
{
	m_eState = SS_Stopped;
	return S_OK;
}

long CSerialCommHelper:: UnInit()
{
	long hr = S_OK;
	try
	{
		m_abIsConnected = false;
		m_threadRun = false;
	}
	catch(...)
	{
		assert(0);
		hr = E_FAIL;
	}
	if ( hr == S_OK ) 
		m_eState = SS_UnInit;
	return hr;
}

bool CSerialCommHelper::isOpen(void)
{
	if(m_eState != SS_UnInit && m_eState != SS_Unknown)
		return true;
	else
		return false;
}

/*!
\fn void CSerialCommHelper::setDtr(bool set)
Sets DTR line to the requested state (high by default).  This function will have no effect if
the port associated with the class is not currently open.
*/
void CSerialCommHelper::SetDtr(bool set)
{
	LockThis();
	if (isOpen())
	{
		int DTR_flag = TIOCM_DTR;
		if (set)
		{   
			ioctl(m_fdCommPort,TIOCMBIC,&DTR_flag);//Set DTR pin

		}
		else
		{
			ioctl(m_fdCommPort,TIOCMBIC,&DTR_flag);//Clear DTR pin
		}
	}
	UnLockThis();
}

/*!
\fn void CSerialCommHelper::setRts(bool set)
Sets RTS line to the requested state (high by default).  This function will have no effect if
the port associated with the class is not currently open.
*/
void CSerialCommHelper::SetRts(bool set)
{
	LockThis();
	if (isOpen())
	{
		int RTS_flag = TIOCM_RTS;
		if (set)
		{   
			ioctl(m_fdCommPort,TIOCMBIC,&RTS_flag);//Set RTS pin

		}
		else
		{
			ioctl(m_fdCommPort,TIOCMBIC,&RTS_flag);//Clear RTS pin
		}
	}
	UnLockThis();
}

/*!
\fn void CSerialCommHelper::flush()
Flushes all pending I/O to the serial port.  This function has no effect if the serial port
associated with the class is not currently open.
*/
void CSerialCommHelper::flush()
{

}

#define MAX_BUFF_SIZE 200
void* CSerialCommHelper::ThreadFn(void*pvParam)
{
	CSerialCommHelper* apThis = (CSerialCommHelper*) pvParam;
	
	fd_set list;
	int ret = 0;
	struct timeval tv;

	send_signal(&(apThis->m_signalThread), TH_EVENT_STARTED);
	apThis->m_threadRun = true;
	while (  apThis->m_threadRun )
	{
		
		FD_ZERO(&list);

		if(apThis->m_fdCommPort >= 0)
		{
		  	FD_SET(apThis->m_fdCommPort, &list);
		}

		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		ret = select(apThis->m_fdCommPort+1, &list, NULL, NULL, &tv);

		if (ret>0)
		{
			
			if (apThis->m_fdCommPort >= 0 && FD_ISSET(apThis->m_fdCommPort, &list))
			{

				//read data here...
				int iAccum = 0;

				apThis->m_theSerialBuffer.LockBuffer();

				try
				{
					std::string szDebug;
					// bool abRet = false;

					int dwBytesRead = 0;
					char * szTmp = (char *)malloc(sizeof(char)*MAX_BUFF_SIZE);
					if (szTmp == NULL)
					{
						ERROR("Malloc failed");
						return 0;
					}

					do
					{
						memset(szTmp, 0, MAX_BUFF_SIZE);

						dwBytesRead = read(apThis->m_fdCommPort, szTmp, MAX_BUFF_SIZE);

						if (dwBytesRead > 0)
						{
							DEBUG("worker thred read %d bytes: %s",dwBytesRead, szTmp);
							apThis->m_theSerialBuffer.AddData(szTmp, dwBytesRead);
							iAccum += dwBytesRead;
						}
					} while (dwBytesRead > 0);

					if (szTmp)
						free(szTmp);
					szTmp = NULL;
				}
				catch (...)
				{
					assert(0);
				}

				//if we are not in started state then we should flush the queue...( we would still read the data)
				if (apThis->GetCurrentState() != SS_Started)
				{
					iAccum = 0;
					apThis->m_theSerialBuffer.Flush();
				}

				apThis->m_theSerialBuffer.UnLockBuffer();

				DEBUG("CSerialCommHelper: Q Unlocked:");
				if (iAccum > 0)
				{
					DEBUG("CSerialCommHelper(worker thread):  SetDataReadEvent() len:{%d} data:{%s}", iAccum, (apThis->m_theSerialBuffer.GetData()).c_str());
					apThis->SetDataReadEvent();
				}
			}
		}
		else if(ret == -1)
		{
			ERROR("select() failed with %d: %s", ret, strerror(errno));
		}	
	}
	return 0;
}


long  CSerialCommHelper::CanProcess ()
{
	switch ( m_eState  ) 
	{
		case SS_Unknown	:assert(0);return E_FAIL;
		case SS_UnInit	:return E_FAIL;
		case SS_Started :return S_OK;
		case SS_Init		:
		case SS_Stopped :
				return E_FAIL;
		default:assert(0);	

	}	
	return E_FAIL;
}

long CSerialCommHelper::Write (const char* data,unsigned long dwSize)
{
	long hr = CanProcess();
	if ( hr == E_FAIL) return hr;
	unsigned int dwBytesWritten = 0;
	dwBytesWritten = write(m_fdCommPort, data, dwSize);
	if(dwBytesWritten != dwSize)
		ERROR("Written less bytes than expected");
	std::string szData(data);
	DEBUG("CSerialCommHelper: Writing:\"%s\" len:%d",(szData).c_str(),szData.size());
	
	return S_OK;
}

long CSerialCommHelper::Read_Upto	(std::string& data,char chTerminator ,long alCount,long alTimeOut)
{
	long hr = CanProcess();
	if ( hr == E_FAIL) return hr;

	DEBUG("CSerialCommHelper: Read_Upto called  ");
	try
	{
	 	
		std::string szTmp;
		szTmp.erase ();
		long alBytesRead;
		
		bool abFound =  m_theSerialBuffer.Read_Upto(szTmp ,chTerminator,alBytesRead);
		if(m_theSerialBuffer.IsEmpty())
			reset_signal(&m_signalThread, TH_EVENT_DATA_RX);

		if ( abFound ) 
		{
			data = szTmp ;
		}
		else
		{//there are either none or less bytes...
			bool abContinue =  true;
			while (  abContinue )
			{
				DEBUG("CSerialCommHelper: Read_Upto () making blocking read call  ");
				thread_event event = wait_for_signal(&m_signalThread, TH_EVENT_DATA_RX, alTimeOut);
				
				if  ( event == TIME_OUT) 
				{
					DEBUG("CSerialCommHelper: Read_Upto () timed out in blocking read");
					data.erase ();
					hr = E_FAIL;
					return hr;

				}
   				
				bool abFound =  m_theSerialBuffer.Read_Upto(szTmp ,chTerminator,alBytesRead);
				if(m_theSerialBuffer.IsEmpty())
					reset_signal(&m_signalThread, TH_EVENT_DATA_RX);
				

				if ( abFound ) 
				{
					data = szTmp;
					DEBUG("CSerialCommHelper: Read_Upto WaitForSingleObject  data:{%s}len:{%d}",((szTmp)).c_str(),szTmp.size ());
					return S_OK;
				}
				DEBUG("CSerialCommHelper: Read_Upto WaitForSingleObject  not FOUND ");

			}
		}
	}
	catch(...)
	{
		DEBUG("CSerialCommHelper: Unhandled exception");
		assert ( 0  ) ;
	}
	return hr;	
}

long CSerialCommHelper::Read_N(std::string& data,long alCount,long  alTimeOut )
{
	long hr = CanProcess();
	if ( hr == E_FAIL) 
	{
		assert(0);
		return hr;
	}
	
	DEBUG("CSerialCommHelper: Read_N called for %ld bytes",alCount);
	try
	{
	 	
		std::string szTmp ;
		szTmp.erase();

		DEBUG("CSerialCommHelper: Read_N (%ld) locking the queue  ",alCount);
		
		int iLocal =  m_theSerialBuffer.Read_N(szTmp ,alCount );
		if(m_theSerialBuffer.IsEmpty())
			reset_signal(&m_signalThread, TH_EVENT_DATA_RX);
		
		if ( iLocal == alCount ) 
		{
			data = szTmp;
		}
		else
		{//there are either none or less bytes...
			long iRead = 0;
			int iRemaining = alCount - iLocal;
			while (  1 )
			{
				
				DEBUG("CSerialCommHelper: Read_N (%ld) making blocking read() ",alCount);
				
				thread_event event = wait_for_signal(&m_signalThread, TH_EVENT_DATA_RX, alTimeOut);
				
				if  ( event == TIME_OUT) 
				{
					DEBUG("CSerialCommHelper: Read_N (%ld) timed out in blocking read",alCount);
					data.erase ();
					hr = E_FAIL;
					return hr;

				}
				
				DEBUG("CSerialCommHelper: Read_N (%ld) Woke Up from WaitXXX() locking Q",alCount);
 				
				
				iRead =  m_theSerialBuffer.Read_N(szTmp , iRemaining);
				iRemaining -= iRead ;
				if(m_theSerialBuffer.IsEmpty())
					reset_signal(&m_signalThread, TH_EVENT_DATA_RX);
				
				DEBUG("CSerialCommHelper: Read_N (%ld) Woke Up from WaitXXX() Unlocking Q",alCount);


				if (  iRemaining  == 0) 
				{
					DEBUG("CSerialCommHelper: Read_N (%ld) Woke Up from WaitXXX() Done reading ",alCount);
					data = szTmp;
					return S_OK;
				}
			}
		}
	}
	catch(...)
	{
		DEBUG("CSerialCommHelper: Unhandled exception");
		assert ( 0  ) ;
	}
	return hr;
}
/*-----------------------------------------------------------------------
	-- Reads all the data that is available in the local buffer.. 
	does NOT make any blocking calls in case the local buffer is empty
-----------------------------------------------------------------------*/
long CSerialCommHelper::ReadAvailable(std::string& data)
{
	long hr = CanProcess();
	if ( hr == E_FAIL) return hr;
	try
	{
		std::string szTemp;
		m_theSerialBuffer.Read_Available (szTemp);
		if(m_theSerialBuffer.IsEmpty())
			reset_signal(&m_signalThread, TH_EVENT_DATA_RX);

		data = szTemp;
	}
	catch(...)
	{
		DEBUG("CSerialCommHelper: Unhandled exception in ReadAvailable()");
		assert ( 0  ) ;
		hr = E_FAIL;
	}
	return hr;
}





