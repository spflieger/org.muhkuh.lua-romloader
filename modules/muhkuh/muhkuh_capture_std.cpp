/***************************************************************************
 *   Copyright (C) 2011 by Christoph Thelen                                *
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


#include "muhkuh_capture_std.h"

#include <errno.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>


#include <wx/wx.h>
#include <wx/process.h>


#define MAX_COMMANDLINE_ARGUMENT 4096


capture_std::capture_std(long lMyId, long lEvtHandlerId)
 : m_lMyId(lMyId)
 , m_lEvtHandlerId(lEvtHandlerId)
 , m_tCaptureThread(-1)
 , m_tExecThread(-1)
{
}


capture_std::~capture_std(void)
{
}





char **capture_std::get_strings_from_table(int iIndex, lua_State *ptLuaState) const
{
	int iResult;
	size_t sizTable;
	size_t sizEntryCnt;
	char **ppcStrings;
	const char *pcLuaString;
	char *pcString;
	size_t sizString;


	/* Be optimistic. */
	iResult = 0;

	/* get the size of the table */
	sizTable = lua_objlen(ptLuaState, iIndex);
	printf("The table has %d elements.\n", sizTable);

	/* Allocate the array for the strings and one terminating NULL. */
	ppcStrings = (char**)malloc(sizeof(char*)*(sizTable+1));
	if( ppcStrings==NULL )
	{
		/* Failed to allocate the array. */
		iResult = -1;
	}
	else
	{
		/* Clear the complete array with 0. */
		memset(ppcStrings, 0, sizeof(char*)*(sizTable+1));

		/* Loop over all table elements and add them to the array. */
		sizEntryCnt = 0;
		while( iResult==0 && sizEntryCnt<sizTable )
		{
			printf("Get entry %d\n", sizEntryCnt+1);
			/* Push the table entry on the stack. */
			lua_rawgeti(ptLuaState, iIndex, sizEntryCnt+1);
			/* Is this a string or a number? */
			if( lua_isstring(ptLuaState, -1)!=1 )
			{
				/* No String, cancel. */
				iResult = -1;
			}
			else
			{
				pcLuaString = lua_tolstring(ptLuaState, -1, &sizString);
				if( pcLuaString==NULL )
				{
					/* Failed to convert the data to a string. */
					iResult = -1;
				}
				else if( sizString>MAX_COMMANDLINE_ARGUMENT )
				{
					/* The argument exceeds the maximum size. */
					iResult = -1;
				}
				else
				{
					/* Add the string to the array. */

					pcString = (char*)malloc(sizString);
					/* Copy the string data and the terminating '\0' to the new buffer. */
					memcpy(pcString, pcLuaString, sizString+1);
					ppcStrings[sizEntryCnt] = pcString;
					printf("ppcStrings[%d] = '%s'\n", sizEntryCnt, pcString);
				}
			}

			/* Pop one element from the stack. */
			lua_pop(ptLuaState, 1);

			++sizEntryCnt;
		}

		/* Free the string array if something went wrong. */
		if( iResult!=0 )
		{
			free_string_table(ppcStrings);
			ppcStrings = NULL;
		}
	}

	return ppcStrings;
}


int capture_std::free_string_table(char **ppcTable) const
{
	char **ppcCnt;
	char *pcString;


	if( ppcTable!=NULL )
	{
		ppcCnt = ppcTable;
		while( *ppcCnt!=NULL )
		{
			free(*ppcCnt);
			++ppcCnt;
		}
		free(ppcTable);
	}
}


void capture_std::send_finished_event(int iPid, int iResult)
{
	wxWindow *ptHandlerWindow;
	wxProcessEvent tProcessEvent(m_lMyId, iPid, iResult);


	if( m_lEvtHandlerId!=wxID_ANY )
	{
		ptHandlerWindow = wxWindow::FindWindowById(m_lEvtHandlerId, NULL);
		if( ptHandlerWindow!=NULL )
		{
			ptHandlerWindow->AddPendingEvent(tProcessEvent);
		}
	}
}


int capture_std::get_pty(void)
{
	int iResult;


	/* Get an unused pseudo-terminal master device. */
	m_iFdPtyMaster = posix_openpt(O_RDWR);
	if( m_iFdPtyMaster<0 )
	{
		fprintf(stderr, "Failed to get PTY master: %d %s\n", errno, strerror(errno));
		iResult = -1;
	}
	else
	{
		printf("iFdPtyMaster = %d\n", m_iFdPtyMaster);
		iResult = grantpt(m_iFdPtyMaster);
		if( iResult!=0 )
		{
			fprintf(stderr, "grantpt failed: %d %s\n", errno, strerror(errno));
		}
		else
		{
			iResult = unlockpt(m_iFdPtyMaster);
			if( iResult!=0 )
			{
				fprintf(stderr, "unlockpt failed: %d %s\n", errno, strerror(errno));
			}
			else
			{
				/* Get the name of the pty. */
				iResult = ptsname_r(m_iFdPtyMaster, m_acPtsName, c_iMaxPtsName);
				if( iResult!=0 )
				{
					fprintf(stderr, "ptsname_r failed: %d %s\n", errno, strerror(errno));
				}
				else
				{
					printf("pts name: '%s'\n", m_acPtsName);
				}
			}
		}
	}

	return iResult;
}


void capture_std::exec_thread(const char *pcCommand, char **ppcCmdArguments)
{
	int iPtyFd;
	int iResult;
	int iCnt;


	fprintf(stderr, "exec_thread start\n");
	
	/* Open the slave pty in write mode. */
	iPtyFd = open(m_acPtsName, O_WRONLY);
	if( iPtyFd<0 )
	{
		fprintf(stderr, "open failed: %d %s\n", errno, strerror(errno));
	}
	else
	{
		/* Dup the pty to stdout. */
		iResult = dup2(iPtyFd, STDOUT_FILENO);
		if( iResult==-1 )
		{
			fprintf(stderr, "dup2 failed: %d %s\n", errno, strerror(errno));
		}
		else
		{
/*
			for(iCnt=0; iCnt<10; ++iCnt)
			{
				write(STDOUT_FILENO, "This is exec stdout.\n", 21);
				sleep(1);
			}
*/
			/* Run the command. */
			iResult = execv(pcCommand, ppcCmdArguments);

			/* If this part is reached, the exec command failed. */
			fprintf(stderr, "execv returned with errorcode %d.\n", iResult);
		}
		close(iPtyFd);
	}

	fprintf(stderr, "exec_thread finish\n");
}


int capture_std::run(const char *pcCommand, lua_State *ptLuaStateForTableAccess)
{
	int iResult;
	char **ppcCmdArguments;
	pid_t tPidExec;
	pid_t tPidCapture;
	ssize_t ssizRead;
	unsigned char aucBuffer[4096];
	int iNewFd;
	fd_set tOpenFdSet;
	fd_set tReadFdSet;
	int iMaxFd;
	int iStatus;
	int iThreadResult;


	/* Init all variables. */
	tPidExec = -1;
	tPidCapture = -1;

	/* The third argument of the function is the table. This is stack index 3. */
	ppcCmdArguments = get_strings_from_table(3, ptLuaStateForTableAccess);
	if( ppcCmdArguments==NULL )
	{
		iResult = 0;
	}
	else
	{
		/* Create the pty. */
		iResult = get_pty();
		if( iResult==0 )
		{
			/* Create the exec thread. */
			tPidExec = fork();
			if( tPidExec==-1 )
			{
				fprintf(stderr, "Failed to create the exec thread: %s\n", strerror(errno));
				iResult = -1;
			}
			else if( tPidExec==0 )
			{
				/* This is the exec thread. */
				exec_thread(pcCommand, ppcCmdArguments);
				exit(EXIT_FAILURE);
			}
			else
			{
				/* This is the parent. */
				m_tExecThread = tPidExec;


				printf("*** Start Capture ***\n");

				tPidCapture = fork();
				if( tPidCapture==-1 )
				{
					fprintf(stderr, "Failed to create the capture thread (%d): %s\n", errno, strerror(errno));
					iResult = -1;
				}
				else if( tPidCapture==0 )
				{
					/* This is the capture thread. */
					printf("This is the capture thread.\n");

					iMaxFd = m_iFdPtyMaster;
//					if( iMaxFd<aiPipeErrFd[PIPE_READ_END] )
//					{
//						iMaxFd = aiPipeErrFd[PIPE_READ_END];
//					}
					++iMaxFd;
					printf("maxfd = %d\n", iMaxFd);


					FD_ZERO(&tOpenFdSet);
					FD_SET(m_iFdPtyMaster, &tOpenFdSet);
//					FD_SET(aiPipeErrFd[PIPE_READ_END], &tOpenFdSet);

					do
					{
						FD_ZERO(&tReadFdSet);
						if( FD_ISSET(m_iFdPtyMaster, &tOpenFdSet) )
						{
							FD_SET(m_iFdPtyMaster, &tReadFdSet);
						}
//						if( FD_ISSET(aiPipeErrFd[PIPE_READ_END], &tOpenFdSet) )
//						{
//							FD_SET(aiPipeErrFd[PIPE_READ_END], &tReadFdSet);
//						}

						iStatus = select(iMaxFd, &tReadFdSet, NULL, NULL, NULL);
						printf("select: %d\n", iStatus);
						if( iStatus==-1 )
						{
							/* Select failed! */
							iResult = EXIT_FAILURE;
							break;
						}
						else if( iStatus>0 )
						{
							if( FD_ISSET(m_iFdPtyMaster, &tReadFdSet) )
							{
								ssizRead = read(m_iFdPtyMaster, aucBuffer, sizeof(aucBuffer)-1);
								if( ssizRead<0 )
								{
									fprintf(stderr, "read error %d %s\n", errno, strerror(errno));
									iResult = EXIT_FAILURE;
									break;
								}
								else if( ssizRead==0 )
								{
									FD_CLR(m_iFdPtyMaster, &tOpenFdSet);
								}
								else
								{
									aucBuffer[ssizRead] = 0;
									printf("<OUT len=%d>%s</OUT>\n", ssizRead, aucBuffer);
								}
							}
/*
							if( FD_ISSET(aiPipeErrFd[PIPE_READ_END], &tReadFdSet) )
							{
								ssizRead = read(aiPipeErrFd[PIPE_READ_END], aucBuffer, sizeof(aucBuffer)-1);
								if( ssizRead<0 )
								{
									iResult = EXIT_FAILURE;
									break;
								}
								else if( ssizRead==0 )
								{
									FD_CLR(aiPipeErrFd[PIPE_READ_END], &tOpenFdSet);
								}
								else
								{
									aucBuffer[ssizRead] = 0;
									printf("<ERR len=%d>%s</ERR>\n", ssizRead, aucBuffer);
								}
							}
*/
						}
					} while( (FD_ISSET(m_iFdPtyMaster, &tOpenFdSet))
//					         || (FD_ISSET(aiPipeErrFd[PIPE_READ_END], &tOpenFdSet))
					);

					close(m_iFdPtyMaster);

					printf("*** Stop Capture ***\n");

					iStatus = waitpid(m_tExecThread, NULL, 0);
					if( WIFEXITED(iStatus) )
					{
						iThreadResult = WEXITSTATUS(iStatus);
					}
					else
					{
						iThreadResult = -1;
					}

					/* Send an event to the handler. */
//					send_finished_event(m_tExecThread, iThreadResult);

					exit(iResult);
				}
				else
				{
					/* This is the parent. */
					m_tCaptureThread = tPidCapture;


					printf("wait until the threads finished.\n");
					waitpid(m_tCaptureThread, NULL, 0);
					printf("ok, all finished!\n");
				}
			}
		}

		free_string_table(ppcCmdArguments);
	}
	return 0;
}

