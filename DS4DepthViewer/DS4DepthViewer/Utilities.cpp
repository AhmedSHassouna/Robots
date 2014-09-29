// Utilities.cpp
// Utility funcitons used by Depth Viewer process

#include "stdafx.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;
#include <DepthCommon.h>
//#include "DepthCommon.h"
#include "DS4DepthViewer.h"

///////////////////////////////////////////////////////////////////////////////////////////////
// Initialize shared memory and events for Inter Process Communication with the Robot control application
int InitIPC( HANDLE &hCommandEvent, LPCTSTR &pCommandBuf, HANDLE &hDepthDataAvailableEvent, LPCTSTR &pDepthDataBuf )
{
	pCommandBuf = NULL;
	pDepthDataBuf = NULL;
	hCommandEvent = NULL;
	hDepthDataAvailableEvent = NULL;
	HANDLE hMapFile = NULL;

	static BOOL bManualReset = FALSE;
	static BOOL bInitialState = FALSE; // Not Signaled 


	// TODO!  DO I NEED TO RELEASE MEMORY OR HANDLES on EXIT?

	/////////////////////////////////////////////////////////////////////////////////////////
	// For sending Depth data to the robot control application
	hMapFile = CreateFileMapping(
		INVALID_HANDLE_VALUE,    // use paging file
		NULL,                    // default security 
		PAGE_READWRITE,          // read/write access
		0,                       // max. object size high
		sizeof(MAX_DEPTH_DATA_SIZE),	// buffer size  
		_T(DEPTH_DATA_SHARED_FILE_NAME) );// name of mapping object

	if ( (INVALID_HANDLE_VALUE == hMapFile) || (NULL == hMapFile)  )
	{ 
		_tprintf(TEXT("Could not create DEPTH DATA file mapping object (%d).\n"), 
			GetLastError());
		return FAILED;
	}
	pDepthDataBuf = (LPTSTR) MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,                   
		0,                   
		MAX_DEPTH_DATA_SIZE );     // WAS sizeof(KOBUKI_BASE_STATUS_T)      

	if (pDepthDataBuf == NULL) 
	{ 
		_tprintf(TEXT("Could not map view of Status file (%d).\n"), GetLastError()); 
		CloseHandle(hMapFile);
		return FAILED;
	}

	// Event to telling Robot that new Depth Data is ready
	hDepthDataAvailableEvent = CreateEvent ( NULL, bManualReset, bInitialState, _T(DEPTH_DATA_AVAILABLE_EVENT_NAME) );
	if ( !hDepthDataAvailableEvent ) 
	{ 
		cerr << "Event creation failed!: " << DEPTH_DATA_AVAILABLE_EVENT_NAME << endl;
		return FAILED;
	}
	SetEvent( hDepthDataAvailableEvent ); // Indicate that the app has started


	/////////////////////////////////////////////////////////////////////////////////////////
	// For getting Commands from the robot control application

	// Create Event to allow Robot to signal when a new Command is pending
	hCommandEvent = CreateEvent ( NULL, bManualReset, bInitialState, _T(DEPTH_COMMAND_EVENT_NAME) );
	if ( !hCommandEvent ) 
	{ 
		cerr << "Event creation failed!: " << DEPTH_COMMAND_EVENT_NAME << endl;
		return FAILED;
	}

	// Now check that the event was signaled by the Robot process, indicating that it's ok to proceed
	// If not, this program was probably launched in stand-alone mode for testing
	const DWORD msTimeOut = 100;
	DWORD dwResult = WaitForSingleObject(hCommandEvent, msTimeOut);
	if( WAIT_OBJECT_0 != dwResult ) 
	{
		cerr << endl;
		cerr << "==============================================================" << endl;
		cerr << "WARNING!  COMMAND Event failed, assuming STAND ALONE mode!"     << endl;
		cerr << "==============================================================" << endl << endl;
		return STAND_ALONE_MODE; 
	}

	// Open Memory Mapped File

	hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,		// read/write access
		FALSE,						// do not inherit the name
		_T(DEPTH_COMMAND_SHARED_FILE_NAME) );	// name of mapping object 

	if ( (INVALID_HANDLE_VALUE == hMapFile) || (NULL == hMapFile)  )
	{ 
		_tprintf(TEXT("Could not create COMMAND file mapping object (%d).\n"), 
			GetLastError());
		return FAILED;
	}
	pCommandBuf = (LPTSTR) MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,                   
		0,                   
		sizeof(DEPTH_COMMAND_T) );           

	if (pCommandBuf == NULL) 
	{ 
		_tprintf(TEXT("Could not map view of file (%d).\n"), GetLastError()); 
		CloseHandle(hMapFile);
		return FAILED;
	}
	else
	{
		cout << "COMMAND Shared Memory File Opened Sucessfully!" << endl;
	} 
	
	return SUCCESS; 
}
