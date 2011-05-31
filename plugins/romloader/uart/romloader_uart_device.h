/***************************************************************************
 *   Copyright (C) 2008 by Christoph Thelen and M. Trensch                 *
 *   doc_bacardi@users.sourceforge.net                                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef __ROMLOADER_UART_DEVICE_H__
#define __ROMLOADER_UART_DEVICE_H__


#include "../romloader.h"

#ifdef _WINDOWS
#       include <windows.h>
#       define PATH_MAX 259
#else
#       include <pthread.h>
#endif


class romloader_uart_device
{
public:
	romloader_uart_device(const char *pcPortName);
	~romloader_uart_device(void);

	/* low level interface is platform specific */
	virtual bool Open(void) = 0;
	virtual void Close(void) = 0;
	virtual bool Flush(void) = 0;
	virtual unsigned long Peek(void) = 0;
	virtual size_t SendRaw(const unsigned char *pucData, size_t sizData, unsigned long ulTimeout) = 0;
	virtual size_t RecvRaw(unsigned char *pucData, size_t sizData, unsigned long ulTimeout) = 0;
	virtual bool Cancel(void) = 0;
	virtual unsigned long GetMaxBlockSize(void) = 0;


	/* higher level interface */
	bool wait_for_prompt(unsigned long ulTimeout);
	bool GetLine(unsigned char **ppucLine, const char *pcEol, unsigned long ulTimeout);
	bool IdentifyLoader(void);

	static unsigned int crc16(unsigned short usCrc, unsigned char ucData);


	static const size_t mc_sizCardSize = 16384;
	struct sBufferCard;

	typedef struct sBufferCard
	{
		unsigned char *pucEnd;
		unsigned char *pucRead;
		unsigned char *pucWrite;
		sBufferCard *ptNext;
		unsigned char aucData[mc_sizCardSize];
	} tBufferCard;


	void initCards(void);
	void deleteCards(void);
	void writeCards(const unsigned char *pucBuffer, size_t sizBufferSize);
	size_t readCards(unsigned char *pucBuffer, size_t sizBufferSize);
	size_t getCardSize(void) const;

protected:
	char *m_pcPortName;

	tBufferCard *m_ptFirstCard;
	tBufferCard *m_ptLastCard;
#ifdef _WINDOWS
	CRITICAL_SECTION m_csCardLock;
#else
	pthread_mutex_t m_csCardLock;

	pthread_cond_t m_tRxDataAvail_Condition;
	pthread_mutex_t m_tRxDataAvail_Mutex;
#endif

private:
	typedef struct
	{
		unsigned long ulResetVector;
		unsigned long ulVersionAddress;
		unsigned long ulVersionValue;
		ROMLOADER_CHIPTYP tChiptyp;
		const char *pcChiptypName;
		ROMLOADER_ROMCODE tRomcode;
		const char *pcRomcodeName;
	} ROMCODE_RESET_ID_T;
	static const ROMCODE_RESET_ID_T atResIds[4];

	bool legacy_read(unsigned long ulAddress, unsigned long *pulValue);
        bool netx50_load_code(const unsigned char *pucNetxCode, size_t sizNetxCode);
	bool netx50_start_code(void);
	bool SendBlankLineAndDiscardResponse(void);
	bool update_device(void);
	void hexdump(const unsigned char *pucData, unsigned long ulSize);
};


#endif  /* __ROMLOADER_UART_DEVICE_H__ */

