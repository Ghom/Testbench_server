// SerialCommHelper.h: interface for the CSerialCommHelper class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <map>
#include <termios.h>
#include <string.h>

#pragma GCC system_header

#define __FILENAME__ strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__
#define S1(x) #x
#define S2(x) S1(x)
#define LOCATION __FILE__ ":" S2(__LINE__)

#include "SerialBuffer.h"

#define S_OK 	0
#define E_FAIL 	-1

#define DEBUG(format, ...) //printf("[DEBUG] %-30s" format "\n", LOCATION, ##__VA_ARGS__)
#define ERROR(format, ...) printf("[ERROR] %-30s" format "\n", LOCATION, ##__VA_ARGS__)
#define WARNING(format, ...) printf("[ERROR] %-30s" format "\n", LOCATION, ##__VA_ARGS__)

typedef enum tagSERIAL_STATE
{
	SS_Unknown,
	SS_UnInit,
	SS_Init,
	SS_Started ,
	SS_Stopped ,
	
} SERIAL_STATE;

typedef enum thread_event_enum 
{
	TH_EVENT_NONE=0, 
	TH_EVENT_STARTED=1, 
	TH_EVENT_TERMINATE=2, 
	TH_EVENT_DATA_RX=4,
	TIME_OUT=8, 
	TH_EVENT_MAX=16,
}thread_event;

typedef struct thread_signal
{
	pthread_cond_t 	event;
	pthread_mutex_t lock;
	unsigned long 	event_flag;
}thread_signal_t;

thread_event wait_for_signal(thread_signal_t* sigHandler, unsigned long event_flags, unsigned int waittime=0);
int send_signal(thread_signal_t* sigHandler, thread_event event);
void reset_signal(thread_signal_t* sigHandler, thread_event event);

class CSerialCommHelper  
{
private:
	SERIAL_STATE	m_eState;
	int		m_fdCommPort;
	thread_signal_t m_signalThread;
	pthread_t	m_Thread;
	bool		m_abIsConnected;
	bool		m_threadRun;
	// void	InvalidateHandle(HANDLE& hHandle );
	// void	CloseAndCleanHandle(HANDLE& hHandle) ;
	
	CSerialBuffer 		m_theSerialBuffer;
	pthread_mutex_t 	m_mutexLock;
	SERIAL_STATE GetCurrentState() {return m_eState;}
public:
	CSerialCommHelper();
	virtual ~CSerialCommHelper();
	//void		GetEventToWaitOn(HANDLE* hEvent) {*hEvent = m_hDataRx;}
	// pthread_cond_t	GetWaitForEvent() {return m_newDataReceived;} 

	inline void		LockThis()			{pthread_mutex_lock ( &m_mutexLock );}	
	inline void		UnLockThis()		{pthread_mutex_unlock (&m_mutexLock); }
	inline void		InitLock()			
	{
		m_signalThread.event = PTHREAD_COND_INITIALIZER;
		m_signalThread.event_flag = 0;
		m_signalThread.lock = PTHREAD_MUTEX_INITIALIZER;

		m_mutexLock = PTHREAD_MUTEX_INITIALIZER;
	}
	inline void		DelLock()				
	{
		pthread_mutex_destroy (&(m_signalThread.lock));
		pthread_mutex_destroy (&m_mutexLock);
	}
 	inline bool		IsInputAvailable()
	{
		LockThis (); 
		bool abData = ( !m_theSerialBuffer.IsEmpty() ) ;
		UnLockThis (); 
		return abData;
	} 
	inline bool		IsConnection() {return m_abIsConnected ;}
 	inline void		SetDataReadEvent()	{send_signal (&m_signalThread, TH_EVENT_DATA_RX);}
	
	bool 			isOpen(void);
	void			SetRts(bool set);
	void			SetDtr(bool set);
	void			flush();

	long			Read_N		(std::string& data,long alCount,long alTimeOut);
	long			Read_Upto	(std::string& data,char chTerminator ,long alCount,long alTimeOut);
	long			ReadAvailable(std::string& data);
	long			Write (const char* data,unsigned long dwSize);
	long			Init(std::string portName= "/dev/serial0", speed_t baudRate = B115200,unsigned char parity = 0,unsigned char stopBits = 1,unsigned char byteSize  = 8);
	long			Start();
	long			Stop();
	long			UnInit();

	static void * ThreadFn(void*pvParam);
	//-- helper fn.
 	long  CanProcess();
	void OnSetDebugOption(long  iOpt,bool bOnOff);
	
};


