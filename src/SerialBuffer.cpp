// SerialBuffer.cpp: implementation of the CSerialBuffer class.
//
//////////////////////////////////////////////////////////////////////

#include <assert.h>

#include "SerialBuffer.h"
#include "SerialCommHelper.h"


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSerialBuffer::CSerialBuffer()
{
	Init();
}
void CSerialBuffer::Init()
{
	m_mutexLock = PTHREAD_MUTEX_INITIALIZER;
	m_abLockAlways	= true;
 	m_iCurPos		= 0;
	m_alBytesUnRead = 0;
	m_szInternalBuffer.erase ();
}

CSerialBuffer::~CSerialBuffer()
{
	pthread_mutex_destroy (&m_mutexLock);
}

void CSerialBuffer::AddData( char ch )
{
	DEBUG ("CSerialBuffer: (tid:%u) AddData(char) called ", (unsigned int)pthread_self());	
	m_szInternalBuffer += ch;
	m_alBytesUnRead += 1;
}

void CSerialBuffer::AddData( std::string& szData,int iLen ) 
{
	DEBUG ("CSerialBuffer: (tid:%u) AddData(%s,%d) called ", (unsigned int)pthread_self (),szData.c_str (),iLen);	
	m_szInternalBuffer.append ( szData.c_str () ,iLen);
	m_alBytesUnRead += iLen;
}

void CSerialBuffer::AddData( char *strData,int iLen ) 
{
	DEBUG ("CSerialBuffer: (tid:%u) AddData(char*,%d) called ", (unsigned int)pthread_self (),iLen);	
	assert ( strData != NULL );
	m_szInternalBuffer.append ( strData,iLen);
	m_alBytesUnRead += iLen;
}

void CSerialBuffer::AddData( std::string &szData ) 
{
	DEBUG ("CSerialBuffer: (tid:%u) AddData(%s) called ", (unsigned int)pthread_self (),szData.c_str () );	
	m_szInternalBuffer +=  szData;
	m_alBytesUnRead += szData.size ();
}

void CSerialBuffer::Flush()
{
	LockBuffer();
	m_szInternalBuffer.erase ();
	m_alBytesUnRead = 0;
	m_iCurPos  = 0;
	UnLockBuffer();
}

long min(long first, long second)
{
	return (first <= second)? first : second;
}

long	 CSerialBuffer::Read_N( std::string &szData,long  alCount)
{
	DEBUG ("CSerialBuffer: (tid:%u) Read_N() called ", (unsigned int)pthread_self ());	

	LockBuffer();
	long alTempCount	= min( alCount ,  m_alBytesUnRead);
   	
	szData.append (m_szInternalBuffer,m_iCurPos ,alTempCount);
	
	m_iCurPos +=  alTempCount ;
	
	m_alBytesUnRead -= alTempCount;
	if (m_alBytesUnRead == 0 )
	{
		ClearAndReset();
	}
 
	UnLockBuffer();
	DEBUG ("CSerialBuffer : (tid:%u) Read_N returned %ld/%ld bytes (data:%s) ", (unsigned int)pthread_self (),alTempCount, alCount,((szData)).c_str());
	return alTempCount;
}

bool CSerialBuffer::Read_Available( std::string &szData)
{
	DEBUG ("CSerialBuffer : (tid:%u) Read_Upto called ", (unsigned int)pthread_self ());

	LockBuffer();
	szData += m_szInternalBuffer ;

	ClearAndReset();

	UnLockBuffer();

	DEBUG ("CSerialBuffer : (tid:%u) Read_Available returned (data:%s)  ", (unsigned int)pthread_self (),((szData)).c_str());	
	return ( szData.size () > 0 );
}


void CSerialBuffer::ClearAndReset()
{
	m_szInternalBuffer.erase();
	m_alBytesUnRead = 0;
	m_iCurPos = 0;
}

bool CSerialBuffer::Read_Upto( std::string &szData,char chTerm,long  &alBytesRead)
{
	return Read_Upto_FIX(szData,chTerm,alBytesRead);
	
	DEBUG ("CSerialBuffer : (tid:%u) Read_Upto called ", (unsigned int)pthread_self ());

	LockBuffer();
   	
	alBytesRead = 0 ;
	bool abFound = false;
	if ( m_alBytesUnRead > 0 ) 
	{//if there are some bytes un-read...
				
			int iActualSize = GetSize ();
			

			for ( int i = m_iCurPos ; i < iActualSize; ++i )
			{
				alBytesRead++;
				szData .append ( m_szInternalBuffer,i,1);
				m_alBytesUnRead -= 1;
				if ( m_szInternalBuffer[i] == 	chTerm) 
				{
					abFound = true;
					break;
				}
			}
			if ( m_alBytesUnRead == 0 ) 
			{
				ClearAndReset();
			}
			else 
			{ 
				//if we are here it means that there is still some data in the local buffer and
				//we have already found what we want... maybe this is ok but we want to catch this
				//scenario --- fix is in TCP/ip SocketBuffer.
				assert(0); 
			} 
	}

	UnLockBuffer();
  	DEBUG ("CSerialBuffer : (tid:%u) Read_Upto returned %ld bytes (data:%s)  ", (unsigned int)pthread_self (),alBytesRead,((szData)).c_str());	
	return abFound;
}

bool CSerialBuffer::Read_Upto_FIX( std::string &szData,char chTerm,long  &alBytesRead)
{
	DEBUG ("CSerialBuffer : (tid:%u) Read_Upto called ", (unsigned int)pthread_self ());

	LockBuffer();
		alBytesRead = 0 ;

   	
 	bool abFound = false;
	if ( m_alBytesUnRead > 0 ) 
	{//if there are some bytes un-read...
		
		int iActualSize = GetSize ();
 		int iIncrementPos = 0;
		for ( int i = m_iCurPos ; i < iActualSize; ++i )
		{
			//szData .append ( m_szInternalBuffer,i,1);
			szData+=m_szInternalBuffer[i];
			m_alBytesUnRead -= 1;
			if ( m_szInternalBuffer[i] == 	chTerm) 
			{
				iIncrementPos++;
				abFound = true;
				break;
			}
			iIncrementPos++;
		}
		m_iCurPos += iIncrementPos;
		if ( m_alBytesUnRead == 0 ) 
		{
			ClearAndReset();
		}
	}
	UnLockBuffer();	
	return abFound;
}