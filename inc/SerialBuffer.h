// SerialBuffer.h: interface for the CSerialBuffer class.
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
// #include "stdafx.h"

class CSerialBuffer  
{
	std::string m_szInternalBuffer;
	pthread_mutex_t 	m_mutexLock;
	bool	m_abLockAlways;
	long	m_iCurPos;
	long  m_alBytesUnRead;
	void  Init();
	void	ClearAndReset();
public:
	inline void LockBuffer() {pthread_mutex_lock ( &m_mutexLock );}	
	inline void UnLockBuffer() {pthread_mutex_unlock (&m_mutexLock); }
	 
	
	CSerialBuffer( );
	virtual ~CSerialBuffer();

	//---- public interface --
	void AddData( char ch ) ;
	void AddData( std::string& szData ) ;
	void AddData( std::string& szData,int iLen ) ;
	void AddData( char *strData,int iLen ) ;
	std::string GetData() {return m_szInternalBuffer;}

	void		Flush();
	long		Read_N( std::string &szData,long alCount);
  bool		Read_Upto( std::string &szData,char chTerm,long  &alBytesRead);
	bool		Read_Available( std::string &szData);
	inline long GetSize() {return m_szInternalBuffer.size();}
	inline bool IsEmpty() {return m_szInternalBuffer.size() == 0; }

	bool Read_Upto_FIX( std::string &szData,char chTerm,long  &alBytesRead);
};
