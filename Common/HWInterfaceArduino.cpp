// HWInterfaceArduino.cpp
// Direct interface to Arduino hardware

#include "stdafx.h"
#include "ClientOrServer.h"
#if ( ROBOT_SERVER == 1 )	// This module used for Robot Server only

#include "MotorControl.h"
#include "DynamixelControl.h"
#include "Module.h"
#include "Globals.h"

#include "HWInterface.h"
#include "HardwareConfig.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define ARDUINO_SIO_BUF_LEN		 128

__itt_string_handle* pshPicThreadReadLoop = __itt_string_handle_create("Read Loop");
__itt_string_handle* pshPicThreadWriteLoop = __itt_string_handle_create("Write Loop");

__itt_string_handle* pshReadError = __itt_string_handle_create("Read Error");
__itt_string_handle* pshPicError = __itt_string_handle_create("Arduino Reported Error");
__itt_string_handle* pshHandleArduinoMessage = __itt_string_handle_create("Handle Arduino Msg");
__itt_string_handle* pshWaitForMoreData = __itt_string_handle_create("Wait for More Data");

__itt_string_handle* pshVersionRequest = __itt_string_handle_create("VersionRequest");
__itt_string_handle* pshPicWriteFile = __itt_string_handle_create("WriteFile");
__itt_string_handle* pshWriteIndex = __itt_string_handle_create("WriteIndex = ");
__itt_string_handle* pshReadIndex = __itt_string_handle_create("ReadIndex = ");


/**

#define GPS_BUF_LEN			 256
#define MOTOR_RX_BUF_LEN	 256
#define SERVO_SYNC_0		0xFF	// required sync for SSC II controller
//#define SERVO_SYNC_0		0x80	// required sync for Pololu controller
#define SERVO_CONTROLLER_ID	0x01	// only one controller attached

//#if ( ROBOT_SERVER == 1 )  // Nothing in this file used for client!

#define CAMERA_UNKNOWN_POSITION 0xFFFF

**/

/////////////////////////////////////////////////////////////////////////////
// HandleAndroidInput
// Commands from Android Phone (if connected)
// Android communicates to robot via bluetooth module on the Arduino
//
void HandleAndroidInput( )
{
	static bool	AndroidHasMotorControl = false; 
	static int 	NextChatPhrase = 0;


	if( g_pFullSensorStatus->AndroidConnected != (BOOL)g_RawArduinoStatus.AndroidConnected )
	{
		g_pFullSensorStatus->AndroidConnected = (BOOL)g_RawArduinoStatus.AndroidConnected;
		if( g_pFullSensorStatus->AndroidConnected )
		{
			ROBOT_LOG( TRUE,  "****** Android Connected! ******\n" )
		}
		else
		{
			ROBOT_LOG( TRUE,  "****** Android Disconnected! ******\n" )
			g_pFullSensorStatus->AndroidCommand = 0;
			g_pFullSensorStatus->AndroidCompass = 0;
			g_pFullSensorStatus->AndroidPitch = 0;
			g_pFullSensorStatus->AndroidRoll = 0;
		}
	}

	if( g_pFullSensorStatus->AndroidConnected )
	{
		g_pFullSensorStatus->AndroidCommand = g_RawArduinoStatus.AndroidCmd;
		g_pFullSensorStatus->AndroidAccEnabled = g_RawArduinoStatus.AndroidAccEnabled;
		g_pFullSensorStatus->AndroidUpdatePending = g_RawArduinoStatus.AndroidUpdatePending;

		if( 0 != g_pFullSensorStatus->AndroidCommand )
		{
			ROBOT_LOG( TRUE,  "DEBUG: AndroidCommand = %d", g_pFullSensorStatus->AndroidCommand )
		}

		if( g_pFullSensorStatus->AndroidAccEnabled )
		{
			if( g_pFullSensorStatus->AndroidUpdatePending )
			{
				// Accelerometer and compass measurements are in degrees
				signed short  TempAndroidCompass = g_RawArduinoStatus.AndroidCompassHigh <<8;
				TempAndroidCompass += g_RawArduinoStatus.AndroidCompassLow;
				g_pFullSensorStatus->AndroidCompass = TempAndroidCompass;

				signed short  TempAndroidPitch = g_RawArduinoStatus.AndroidPitchHigh <<8;
				TempAndroidPitch += g_RawArduinoStatus.AndroidPitchLow;
				g_pFullSensorStatus->AndroidPitch = TempAndroidPitch;

				signed short  TempAndroidRoll = g_RawArduinoStatus.AndroidRollHigh <<8;
				TempAndroidRoll += g_RawArduinoStatus.AndroidRollLow;
				g_pFullSensorStatus->AndroidRoll = TempAndroidRoll;
				ROBOT_LOG( TRUE,  "DEBUG: AndroidCompass = %d", g_pFullSensorStatus->AndroidCompass )
				ROBOT_LOG( TRUE,  "DEBUG: AndroidPitch = %d", g_pFullSensorStatus->AndroidPitch )
				ROBOT_LOG( TRUE,  "DEBUG: AndroidRoll = %d\n", g_pFullSensorStatus->AndroidRoll )
			}
		}
		else
		{
			g_pFullSensorStatus->AndroidCompass = 0;
			g_pFullSensorStatus->AndroidPitch = 0;
			g_pFullSensorStatus->AndroidRoll = 0;
		}
	}


	switch( g_pFullSensorStatus->AndroidCommand )
	{	

		// Handle Button commands from Android

	#if ( ROBOT_TYPE == LOKI )

		case 0: // No new command
		{
			break;
		}
		case 1: // Wave and say hello
		{
			SendCommand( WM_ROBOT_SET_ARM_MOVEMENT, (DWORD)RIGHT_ARM, (DWORD)ARM_MOVEMENT_WAVE );
			ROBOT_LOG( TRUE,  "Got Android command: Wave\n")
			break;
		}
		case 2: // Head Center
		{
			SendCommand( WM_ROBOT_USER_CAMERA_PAN_CMD, (DWORD)CAMERA_PAN_ABS_CENTER, 5 );
			SendCommand( WM_ROBOT_CAMERA_SIDETILT_ABS_CMD, (DWORD)CAMERA_SIDETILT_CENTER, 5 );

			// KLUDGE - also toggle lights
			ROBOT_LOG( TRUE,  "Processing command from Android: Toggle Lights\n")
			if( g_pFullSensorStatus->AuxLightsOn ) // keeps track of current state
			{
				SendCommand( WM_ROBOT_AUX_LIGHT_POWER_CMD, 0, (DWORD)FALSE ); // turn off
			}
			else
			{
				SendCommand( WM_ROBOT_AUX_LIGHT_POWER_CMD, 0, (DWORD)TRUE ); // turn on
			}

//			ROBOT_LOG( TRUE,  "Got Android command: Head Center\n")
			break;
		}
		case 3: //  "New Chat" - chat with adults  // OLD: New Child Conversation
		{
			ROBOT_LOG( TRUE,  "Got Android command: New Adult Chat\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_CHAT_DEMO_WITH_ADULT, 1 );	// TRUE = start mode
			break;
		}
		case 4: // "Quick Chat" - say a quick phrase   // OLD: New Child Conversation
		{
			ROBOT_LOG( TRUE,  "Got Android command: New Quick Chat\n")
			CString TextToSpeak;
			switch( NextChatPhrase++ )
			{
				case 0:   TextToSpeak =  "My name is Low Key" ;break;
				case 1:   TextToSpeak =  "What is your name?" ;break;
				case 2:   TextToSpeak =  "Do you like robots?" ;break;
				case 3:   TextToSpeak =  "want to hear some jokes?" ;break;
				default:  
					NextChatPhrase = 0;
					TextToSpeak =  "Do you live here?" ;
					break;
			}
			SpeakText( TextToSpeak );
			//g_MoveArmsWhileSpeaking = FALSE;
			break;
		}
		case 5: // Follow Me
		{
			ROBOT_LOG( TRUE,  "Got Android command: Follow Me\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_FOLLOW_PERSON, (DWORD)TRUE );	// TRUE = start mode
			SpeakText( "I will follow you" );
			break;
		}
		case 6: // Tell Joke
		{
			ROBOT_LOG( TRUE,  "Got Android command: Tell Joke\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_TELL_JOKES, (DWORD)0 ); // single joke only
			break;
		}
		// TODO - Make this "come here"?
		case 7: // Take Picture // OLD: Pan/Tilt Head by Accelerometer
		{

			ROBOT_DISPLAY( TRUE, "android  Command Recognized: Arms/Hands UP")
			SendCommand( WM_ROBOT_SET_ARM_MOVEMENT, (DWORD)BOTH_ARMS, (DWORD)ARM_MOVEMENT_ARM_UP_FULL );	// Right/Left arm, Movement to execute, 
			RobotSleep(100, pDomainSpeakThread); // give time for arm to raise 
			// Respond with random phrases
			int RandomNumber = ((3 * rand()) / RAND_MAX);
			ROBOT_LOG( TRUE, "DEBUG: RAND = %d\n", RandomNumber)
			switch( RandomNumber )
			{
				case 0: SpeakText( "Ok, Don't shoot" ); break;
				default:  SpeakText( "I am not the droid you are looking for" );// If const is larger number, this gets called more often
			}
			
			//ROBOT_LOG( TRUE,  "Got Android command: Take Picture \n")
			//SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_TAKE_PHOTO, (DWORD)TRUE );	// TRUE = start mode

			/* Set Speed
			m_pHeadControl->SetHeadSpeed(HEAD_OWNER_USER_CONTROL, WiiRollPanServoSpeed, WiiPitchTiltServoSpeed, NOP );
			// Set position
			m_pHeadControl->SetHeadPositionRelative( HEAD_OWNER_USER_CONTROL,
				WiiRollPanChangeTenthDegrees, WiiPitchTiltChangeTenthDegrees, NOP, FALSE ); // TRUE = Limit to front of robot
			m_pHeadControl->ExecutePositionAndSpeed( HEAD_OWNER_USER_CONTROL );
			*/
						
			break;
		}
		case 8: // Shake
		{
			ROBOT_LOG( TRUE,  "Got Android command: Shake Hands\n")
			SendCommand( WM_ROBOT_SET_ARM_MOVEMENT, (DWORD)RIGHT_ARM, (DWORD)ARM_MOVEMENT_SHAKE_READY );
			break;
		}

		case 9: // Have Something
		{
			SendCommand( WM_ROBOT_SET_ARM_MOVEMENT, (DWORD)LEFT_ARM, (DWORD)ARM_MOVEMENT_EXTEND_ARM_AUTO_CLAW );
			break;
		}
		case 10: // Throw Away
		{
			SendCommand( WM_ROBOT_SET_ARM_MOVEMENT, (DWORD)LEFT_ARM, (DWORD)ARM_MOVEMENT_PUT_IN_BASKET );
			// Respond with random phrases
			int RandomNumber = ((4 * rand()) / RAND_MAX);
			ROBOT_LOG( TRUE, "DEBUG: RAND = %d\n", RandomNumber)
			switch( RandomNumber )
			{
				case 0:  SpeakText( "easy come, easy go" );break;
				case 1:  SpeakText( "I will take care of this later" );break;
				case 3:  SpeakText( "OK, if you say so" );break;
				default: SpeakText( "Good idea" ); // If const is larger number, this gets called more often
			}
			break;
		}
		case 11: // Clean Up
		{
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_PICKUP_OBJECTS, (DWORD)1 );
			SpeakText( "A robots work is never done" );break;
			break;
		}
		case 12: // Intro
		{
			ROBOT_LOG( TRUE,  "Got Android command: 12 Intro \n")
			PostThreadMessage( g_dwSpeakThreadId, WM_ROBOT_SPEAK_TEXT, SPEAK_INTRO, 0 );
			break;
		}

		case 13: // EMERGENCY STOP!
		{
			AndroidHasMotorControl = TRUE;
			ROBOT_LOG( TRUE,  "Got Android command: STOP\n")
			SendCommand( WM_ROBOT_STOP_CMD, 0, 0 ); // Forces stops and tells all modules to reset (cancel current behavior)
			g_StopSpeechBehavior = TRUE; // KLUDGE to interrupt monologs
			break;
		}
		case 14: // What Time
		{
			ROBOT_LOG( TRUE,  "Got Android command: 17 What Time \n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_WHAT_TIME_IS_IT, (DWORD)0 );
			break;
		}
		case 15: // Danger
		{
			ROBOT_LOG( TRUE,  "Got Android command: 18 Danger\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_FREAK_OUT, (DWORD)0 );
			break;
		}

		default:
			ROBOT_LOG( TRUE,  "ERROR! Unhandled Android Button: %d\n", g_pFullSensorStatus->AndroidCommand)
	}
	

	#else // Not Loki
		case 0: // No new command
		{
			break;
		}
		case 1:
		{
			ROBOT_LOG( TRUE,  "Got Android command: Say Hello\n")
			// Respond with random phrases
			int RandomNumber = ((6 * rand()) / RAND_MAX);
			ROBOT_LOG( TRUE,"DEBUG: RAND = %d\n", RandomNumber)
			switch( RandomNumber )
			{
				case 0:  SpeakText( "Hello" );break;
				case 1:  SpeakText( "Hi there!" );break;
				case 2:  SpeakText( "Hey there");break;
				case 3:  SpeakText( "Hi" );break;									
				default: SpeakText( "Hello!" ); // If const is larger number, this gets called more often
			}
			break;
		}
		case 2: // "Quick Chat" - rotate through quick phrases
		{
			ROBOT_LOG( TRUE,  "Got Android command: New Quick Chat\n")
			CString TextToSpeak;
			switch( NextChatPhrase++ )
			{
				case 0:   SpeakText( "My name is Alice" ) ;break;
				case 1:   SpeakText( "What is your name?" ) ;break;
				case 2:   SpeakText( "Do you like my lights?" ) ;break;
				case 3:   SpeakText( "want to hear some jokes?" ) ;break;
				default:  
					NextChatPhrase = 0;
					break;
			}
			break;
		}
		case 3: 
		{
			ROBOT_LOG( TRUE,  "Got Android command: RUN_ACROSS_STAGE\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_RUN_ACROSS_STAGE, (DWORD)0 ); 
			break;
		}
		case 4: 
		{
			ROBOT_LOG( TRUE,  "Got Android command: Run Back\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_RUN_BACK, (DWORD)0 ); 
			break;
		}
		case 5: // Follow Me
		{
			ROBOT_LOG( TRUE,  "Got Android command: Follow Me\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_FOLLOW_PERSON, (DWORD)TRUE );	// TRUE = start mode
			SpeakText( "I will follow you" );
			break;
		}
		case 6: // Tell Joke
		{
			ROBOT_LOG( TRUE,  "Got Android command: Tell Joke\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_TELL_JOKES, (DWORD)0 ); // single joke only
			break;
		}
		case 7: // Danger
		{
			ROBOT_LOG( TRUE,  "Got Android command: Danger\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_FREAK_OUT, (DWORD)0 );
			break;
		}
		case 8:  //ACTION_MODE_FIND_DOCK
		{
			ROBOT_LOG( TRUE,  "Got Android command: Find Dock\n")
			SendCommand( WM_ROBOT_SET_ACTION_CMD, (DWORD)ACTION_MODE_FIND_DOCK, (DWORD)0 );
			break;
		}
		case 9: 
		{
			ROBOT_LOG( TRUE,  "Got Android command: Say Yes\n")
			SpeakText( "yes" );
			break;
		}
		case 10: 
		{
			ROBOT_LOG( TRUE,  "Got Android command: Say No\n")
			SpeakText( "no" );
			break;
		}
		case 11: 
		{
			ROBOT_LOG( TRUE,  "Got Android command: Say thank you\n")
			SpeakText( "thank you" );
			break;
		}
		case 12: 
		{
			ROBOT_LOG( TRUE,  "Got Android command: Say You are welcome\n")
			SpeakText( "Youre welcome" );
			break;
		}
		case 13: // EMERGENCY STOP!
		{
			AndroidHasMotorControl = TRUE;
			ROBOT_LOG( TRUE,  "Got Android command: STOP\n")
			SendCommand( WM_ROBOT_STOP_CMD, 0, 0 ); // Forces stops and tells all modules to reset (cancel current behavior)
			break;
		}
		case 14: // Mic Toggle
		{
			ROBOT_LOG( TRUE,  "Got Android command: Mic Off \n")
			if( !g_SpeechRecoBlocked )
			{
				g_SpeechRecoBlocked = TRUE;
				SpeakText( "mike off" );
			}
			else
			{
				g_SpeechRecoBlocked = FALSE;
				SpeakText( "mike on" );
			}
			break;
		}
		case 15: // Lights
		{
			ROBOT_LOG( TRUE,  "Processing command from Android: Toggle Lights\n")
			if( g_pFullSensorStatus->AuxLightsOn ) // keeps track of current state
			{
				SendCommand( WM_ROBOT_AUX_LIGHT_POWER_CMD, 0, (DWORD)FALSE ); // turn off
			}
			else
			{
				SendCommand( WM_ROBOT_AUX_LIGHT_POWER_CMD, 0, (DWORD)TRUE ); // turn on
			}
			break;
		}


		default:
			ROBOT_LOG( TRUE,  "ERROR! Unhandled Android Button: %d\n", g_pFullSensorStatus->AndroidCommand)
	}
#endif // Not Loki	



	// Handle Accelerometer
	if( g_pFullSensorStatus->AndroidAccEnabled )
	{
		if( g_pFullSensorStatus->AndroidUpdatePending )
		{
			int AndoidCmdSpeed = 0;
			int AndoidCmdTurn = 0;

			// Pitch can go to over 90 degrees, but then roll does not work well, so max at pitch = 60 degrees
			// Convert Speed from +/- 60 to +/- 127, but leave guard band at center				
			int Pitch = g_pFullSensorStatus->AndroidPitch - 10; //  To make it comfortable to hold the phone, "neutral" is 10 degrees up
			if( (Pitch > -15) && (Pitch< 10) )
			{
				AndoidCmdSpeed = 0;
			}
			else
			{					
				AndoidCmdSpeed = (int)( ((double)Pitch * -127.0) / 60.0 );	// Invert, down = forward.
				if( AndoidCmdSpeed > 127 ) AndoidCmdSpeed = 127;
				if( AndoidCmdSpeed < -127 ) AndoidCmdSpeed = -127;
			}

			// Convert Turn from +/- 90 to +/- 64, but leave guard band at center
			if( (g_pFullSensorStatus->AndroidRoll > -10) && (g_pFullSensorStatus->AndroidRoll < 10) )
			{
				AndoidCmdTurn = 0;
			}
			else
			{					
				AndoidCmdTurn = (int)( ((double)g_pFullSensorStatus->AndroidRoll * 64.0) / 80.0 ); // slightly less then 90 to ease wrist motion
				if( AndoidCmdTurn > 64 ) AndoidCmdTurn = 64;
				if( AndoidCmdTurn < -64 ) AndoidCmdTurn = -64;
			}

			ROBOT_LOG( TRUE,  "AndoidCmdSpeed = %d, AndoidCmdTurn = %d\n", AndoidCmdSpeed, AndoidCmdTurn)

			SendCommand( WM_ROBOT_DRIVE_LOCAL_CMD, AndoidCmdSpeed, AndoidCmdTurn ); 
			AndroidHasMotorControl = TRUE;
			g_pFullSensorStatus->AndroidUpdatePending = false;  // command has been handled
		}
	}	
	else
	{
		if( AndroidHasMotorControl )
		{
			// Android had control, but now released (or dropped connection!). Force a stop
			SendCommand( WM_ROBOT_DRIVE_LOCAL_CMD, SPEED_STOP, TURN_CENTER ); // stop
			SendCommand( WM_ROBOT_RELEASE_OWNER_CMD, LOCAL_USER_MODULE, 0 );  // then release ownership immediately
			AndroidHasMotorControl = FALSE;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////
void HandleArduinoMessage(char *CmdResponseBuf, int nResponseLength)
{

#if ( (SENSOR_CONFIG_TYPE == SENSOR_CONFIG_LOKI) || (SENSOR_CONFIG_TYPE == SENSOR_CONFIG_KOBUKI_WITH_ARDUINO) )
// Need to figure out what to do for Carbot

	// Handles messages from the Arduino
	// First byte is
	// ACK or AKx means things went well
	// ERR means a Serial I/O error occured
	// NAK means command not recognized or parameter error
	//TAL_Event("HandlePicMsg Top");
	//-TAL_SCOPED_TASK_NAMED("pshHandleArduinoMessage");

	CString strStatus;
	static int	nReadIndex = 0; // For tracking how many reads sucessfully received

	// First byte is the version, and is a santity check that the message is not corrupted
	int nArduinoVersion = CmdResponseBuf[0];
	if( ARDUINO_CODE_VERSION != nArduinoVersion )
	{
		// PC and Arduino not in sync, or corrupted data!
		ROBOT_DISPLAY( TRUE, "ERROR! ARDUINO Version does not match PC Version!" )
		ROBOT_ASSERT(0);
	}
	else if( SUBSYSTEM_CONNECTED != g_ArduinoSubSystemStatus )
	{
		///////////////////////////////////
		// First message from the Arduino
		///////////////////////////////////
		g_ArduinoSubSystemStatus = SUBSYSTEM_CONNECTED;
		// Now throw message in the queue, to indicate that the version has been updated
		//PostThreadMessage( g_dwControlThreadId, (WM_ROBOT_PIC_VERSION_READY), 0, nArduinoVersion );
		PostThreadMessage( g_dwSpeakThreadId, WM_ROBOT_SPEAK_TEXT, SPEAK_ARDUINO_CONNECTED, 0 );  // speak that Arduino is connected

		SendCommand( WM_ROBOT_SET_LED_EYES_CMD, (DWORD)LED_EYES_BLINK, 0 );  // Turn on eyes after first status received
	}


	int   nMessageType = CmdResponseBuf[1];
	char *pMessage = &CmdResponseBuf[2];
	int   nMessageLength = nResponseLength - 2;

	switch( nMessageType )
	{
		case ARDUINO_MESSAGE_STATUS:
		{
			// Sensor and other Status Data from the Arduino
			ROBOT_LOG( SERIAL_DBG, "Arduino: AK2 Status Data\n" )	// happens several times per second!
			// TODO!  TEST nMessageLength for a good status packet, and update this code! <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< TODO-MUST

			if( nMessageLength < sizeof(ARDUINO_STATUS_T) )
			{
				CString strStatus;
				strStatus.Format( "ARDUINO ERROR!: Short Status Packet.  Length = %d" );
				ROBOT_DISPLAY( TRUE,  (LPCTSTR)strStatus )
				__itt_marker(pDomainControlThread, __itt_null, pshReadError, __itt_marker_scope_task);
				return;
			}
			// copy the data
			memcpy( &g_RawArduinoStatus, pMessage, sizeof( ARDUINO_STATUS_T ) );

			// Handle Android commands and accelerometer data, if any
			HandleAndroidInput();

			// Now throw message in the queue, to indicate that the status has been updated, and let sensor module handle the rest 
				if(!g_PicFirstStatusReceived)
				{
					// Force status to be updated on the GUI, for example when client connects
					PostThreadMessage( g_dwControlThreadId, (WM_ROBOT_SENSOR_STATUS_READY), 1, 1 ); // use 1 to force status updates every time!
					g_PicFirstStatusReceived = TRUE;
				}
				else
				{
					PostThreadMessage( g_dwControlThreadId, (WM_ROBOT_SENSOR_STATUS_READY), 0, 0 );
				}
			break;

		}
		case ARDUINO_MESSAGE_TEXT:
		{
			// Standard Acknowledgement with status string.  Just print the string.
			pMessage[nMessageLength] = '\0';	// make sure null terminated
			strStatus.Format( "ARDUINO: %s", pMessage );
			ROBOT_DISPLAY( TRUE,  (LPCTSTR)strStatus )
			break;

		}
		default:
		{
			strStatus.Format( "ARDUINO ERROR: Bad Arduino Message: %02X %02X %02X %02X", 
				CmdResponseBuf[1], CmdResponseBuf[2], CmdResponseBuf[3], CmdResponseBuf[4] );
			ROBOT_DISPLAY( TRUE,  (LPCTSTR)strStatus )
			return;

		}
	}
	// We got something reasonable.  Reset the watchdog timer
	gPicWatchDogTimer = 0;

	//__itt_metadata_add(pDomainControlThread, __itt_null, pshReadIndex, __itt_metadata_s32, 1, (void*)&nReadIndex);
	//nReadIndex++;
#endif // #if ROBOT_TYPE == LOKI
}


/////////////////////////////////////////////////////////////////////////////
DWORD WINAPI ArduinoCommReadThreadFunc(LPVOID lpParameter)
{

#if ( (SENSOR_CONFIG_TYPE == SENSOR_CONFIG_LOKI) || (SENSOR_CONFIG_TYPE == SENSOR_CONFIG_KOBUKI_WITH_ARDUINO) )
// Need to figure out what to do for Carbot

	__itt_thread_set_name( "Arduino Read Thread" );

	//const char PicReadCounterName[] = "PicReadCount";

	// Arduino Response Parser State Machine
	#define	SIO_LOOK_FOR_SYNC_1		0
	#define	SIO_LOOK_FOR_SYNC_2		1
	#define	SIO_GET_MSG_SIZE		2
	#define	SIO_COPY_RESPONSE		3

//	DWORD fdwCommMask;
	//TCHAR szwcsBuffer[1000];
	//char szBuffer[1000];
	HANDLE hCommPort = (HANDLE)lpParameter;

	char	*pCmdResponse;
	char	SioBuffer[ARDUINO_SIO_BUF_LEN];
	char	CmdResponseBuf[ARDUINO_SIO_BUF_LEN];
	BOOL	bFirstPacket;
	DWORD	nSioBytesReceived;
	int		nCmdResponseLen = 0;
	int		nResponseBytesCopied;
	int		ret = 0;
	int		nSioCharNum;
	UINT	SerialParserState = SIO_LOOK_FOR_SYNC_1;
	int		DbgDotCount = 0;

	//SetCommMask (hCommPort, EV_RXCHAR | EV_BREAK );
	//Sleep(10000, pDomainArduinoThread);	// Wait 10 seconds for the port to be ready?!?
	//Sleep(200, pDomainArduinoThread);

	pCmdResponse = CmdResponseBuf;	// Pointer to walk the buffer, filling it up
	memset( CmdResponseBuf, '\0', ARDUINO_SIO_BUF_LEN );	// clean out garbage chars
	memset( SioBuffer, '\0', ARDUINO_SIO_BUF_LEN );	// clean out garbage chars
	memset( &g_RawArduinoStatus, '\0', sizeof(ARDUINO_STATUS_T) );	// clean out garbage chars

	bFirstPacket = TRUE;
	nSioBytesReceived = 0;
	nResponseBytesCopied = 0;

	while(hCommPort != INVALID_HANDLE_VALUE)
	{
		__itt_task_begin(pDomainArduinoThread, __itt_null, __itt_null, pshPicThreadReadLoop);

		///TAL_SCOPED_TASK_NAMED("Arduino Read SIO");
		//xTAL_IncCounter(PicReadCounterName);
		// 

/*		if( SUBSYSTEM_CONNECTED != g_ArduinoSubSystemStatus )
		{
			// WARNING! The following call will hang until a character comes in the serial port!
			// This means it will hang on shutdown unless a Arduino processor is hooked up and running!
			ROBOT_LOG( TRUE, "Arduino COMM Read: Waiting for Arduino...\n")
			if(!WaitCommEvent (hCommPort, &fdwCommMask, 0))
			{
				// has the comm port been closed?
				if(GetLastError() != ERROR_INVALID_HANDLE)
					ReportCommError("WaitCommEvent.");
				return 0;		// terminate thread on error
			}
		}
*/
		if( SERIAL_DBG )
		{
			ROBOT_LOG( TRUE,  " [RX ");	// just show activity for each read, so we know Arduino is not dead.
		}
		//SetCommMask (hCommPort, EV_RXCHAR | EV_BREAK);
		memset( SioBuffer, '\0', ARDUINO_SIO_BUF_LEN );	// clean out garbage chars
		if( 1 == ROBOT_SIMULATION_MODE )
		{
			g_ArduinoSubSystemStatus = SUBSYSTEM_CONNECTED;	// Receipt of version is confirmation that we are connected.
			__itt_task_end(pDomainArduinoThread);  // pshPicThreadReadLoop
			RobotSleep(SIMULATED_SIO_SLEEP_TIME, pDomainArduinoThread);
			continue;
		}

		if( SERIAL_DBG )
		{
			ROBOT_LOG( TRUE,  "1,");	// just show activity for each read, so we know Arduino is not dead.
		}
		if(!ReadFile(hCommPort,		// where to read from (the open comm port)
				 SioBuffer,		// where to put the character
				 (ARDUINO_SIO_BUF_LEN-1),	// number of bytes to read
				 &nSioBytesReceived,	// how many bytes were actually read
				 NULL))				// overlapped I/O not supported
		{

			ROBOT_DISPLAY( TRUE, "ERROR Reading Arduino Comm port\n" )
			PurgeComm(hCommPort, PURGE_RXABORT|PURGE_RXCLEAR);
			//PurgeComm(hCommPort, PURGE_RXABORT|PURGE_TXABORT|PURGE_RXCLEAR|PURGE_TXCLEAR);
			DWORD dwCommError = 0;
			ClearCommError(hCommPort, &dwCommError, NULL);
			ReportCommError("Arduino SIO Read Error", dwCommError );

			__itt_marker(pDomainControlThread, __itt_null, pshReadError, __itt_marker_scope_task);
			__itt_task_end(pDomainArduinoThread);  // pshPicThreadReadLoop
			//ROBOT_ASSERT(0);	// TODO REMOVE THIS - looking for cause of Arduino errors DWS
			continue; // CHANGED TO TEST Arduino RELIABILITY! DWS return 0;		// terminate thread on first error
		}

		if( SERIAL_DBG )
		{
			ROBOT_LOG( TRUE,  "2,");	// just show activity for each read, so we know Arduino is not dead.
		}
		if( 0 == nSioBytesReceived )
		{
			//ROBOT_LOG( SERIAL_DBG, "Arduino COMM Read - no data ready.\n" )
			if( SERIAL_DBG )
			{
				ROBOT_LOG( TRUE,  "0] ");	// just show activity for each read, so we know Arduino is not dead.
			}

			__itt_task_end(pDomainArduinoThread);  // pshPicThreadReadLoop
			if( SUBSYSTEM_CONNECTED != g_ArduinoSubSystemStatus )
			{
				RobotSleep(100, pDomainArduinoThread);
			}
			else
			{
				RobotSleep(50, pDomainArduinoThread);
			}
			continue;
		}		
		if( SERIAL_DBG )
		{
			ROBOT_LOG( TRUE,  "3 RX CHAR! ,");	// just show activity for each read, so we know Arduino is not dead.
		}

		///////////////////////// Buffer Parse Loop /////////////////////////////////
		char* pParser = SioBuffer;	// get a pointer to the buffer

		{
			///TAL_SCOPED_TASK_NAMED("Parse Buffer From Arduino");

/*			if( SERIAL_DBG )
			{
				ROBOT_LOG( TRUE,  " [RX] ");	// just show activity for each read, so we know Arduino is not dead.
				if( DbgDotCount++ > 30 )
				{
					ROBOT_LOG( TRUE,  "\n" );
					DbgDotCount = 0;
				}
			}	
*/
		if( SERIAL_DBG )
		{
			ROBOT_LOG( TRUE,  "4,");	// just show activity for each read, so we know Arduino is not dead.
		}
			int nMsgBytesReceived = 0;	
			for (nSioCharNum=0; nSioCharNum< (int)nSioBytesReceived; nSioCharNum++)
			{
				switch( SerialParserState )
				{
					case SIO_LOOK_FOR_SYNC_1:
					{
						// Look for Sync Frame (2 "Z" chars in a row)
						if( 'Z' == *pParser++ )
						{
							// got first Sync char.  
							SerialParserState = SIO_LOOK_FOR_SYNC_2;
						}
					}
					break;

					case SIO_LOOK_FOR_SYNC_2:
					{
						if( 'Z' == *pParser++ )
						{
							// Second Sync found
							SerialParserState = SIO_GET_MSG_SIZE;
						}
						else
						{
							// error, bail!
							SerialParserState = SIO_LOOK_FOR_SYNC_1;
						}
					}
					break;

					case SIO_GET_MSG_SIZE:
					{
						// Next byte is the size of the packet
						nCmdResponseLen = (int)*pParser++;	// Length the Arduino said the message should be!
						if( nCmdResponseLen > RESPONSE_STRING_LEN+1 )	// Longest Message allowed
						{
							ROBOT_DISPLAY( TRUE, "ERROR: Invalid Arduino Response Length!" )
							SerialParserState = SIO_LOOK_FOR_SYNC_1;
							__itt_marker(pDomainControlThread, __itt_null, pshReadError, __itt_marker_scope_task);
						}
						else if( nCmdResponseLen <= 0 )
						{
							ROBOT_DISPLAY( TRUE, "ERROR: Arduino Response Length Zero!" )
							SerialParserState = SIO_LOOK_FOR_SYNC_1;
							__itt_marker(pDomainControlThread, __itt_null, pshReadError, __itt_marker_scope_task);
						}
						else
						{
							bFirstPacket = FALSE;
							SerialParserState = SIO_COPY_RESPONSE;
						}
					}
					break;

					case SIO_COPY_RESPONSE:
					{

						// Save response, without the size byte or sync bytes
						nMsgBytesReceived = nSioBytesReceived - nSioCharNum; // Subtract out leading sync and length bytes
						if( (nResponseBytesCopied + nMsgBytesReceived) >= nCmdResponseLen )	// nResponseBytesCopied holds bytes recieved previously
						{
							// We received the full message now, with possible extra 
							// bytes for the next message in the buffer too!
							// Copy just the remaining bytes for THIS message
							int BytesToCopy = nCmdResponseLen - nResponseBytesCopied;
							memcpy( pCmdResponse, pParser, BytesToCopy );

							nSioCharNum += (BytesToCopy-1);	// Move counter up for the number of bytes copied out of the SIO buffer, minus the next i++
							pParser += BytesToCopy;		// Same for the input pointer
							nResponseBytesCopied += BytesToCopy;

							if( nResponseBytesCopied != nCmdResponseLen )
							{
								ROBOT_LOG( TRUE, "ERROR: BytesCopied != CmdResponseLen!\n")
								__itt_marker(pDomainControlThread, __itt_null, pshReadError, __itt_marker_scope_task);
							}

							// Sucessfully read the full response!
							CmdResponseBuf[nCmdResponseLen+1] = '\0';	// Null terminate
							//ROBOT_LOG( TRUE, "Start:HandleArduinoMessage ----------------------\n")
							
							//Sleep(5);
							{
								///TAL_Event("Arduino Msg Received!");
								///TAL_SCOPED_TASK_NAMED("Call  HandleArduinoMessage");
								__itt_task_begin(pDomainArduinoThread, __itt_null, __itt_null, pshHandleArduinoMessage);
								HandleArduinoMessage(CmdResponseBuf, nCmdResponseLen);
								__itt_task_end(pDomainArduinoThread);
							}
							//ROBOT_LOG( TRUE, "End:HandleArduinoMessage ============================\n")

							// Initialize for new message
							nResponseBytesCopied = 0;
							pCmdResponse = CmdResponseBuf;
							memset( CmdResponseBuf, '\0', ARDUINO_SIO_BUF_LEN );	// clean out garbage chars
							
							SerialParserState = SIO_LOOK_FOR_SYNC_1;
						}
						else
						{
							// The full response is not in the buffer, so 
							// just copy what we've received so far
							memcpy( pCmdResponse, pParser, nMsgBytesReceived ); 
							pCmdResponse = pCmdResponse + nMsgBytesReceived;
							nResponseBytesCopied += nMsgBytesReceived;
							nSioCharNum += (nMsgBytesReceived-1);	// for/next loop should exit after this
							__itt_marker(pDomainControlThread, __itt_null, pshWaitForMoreData, __itt_marker_scope_task);
						}
					}
					break;

					default:
					{
						ROBOT_DISPLAY( TRUE, "ERROR!  Bad STATE in ArduinoCommReadThreadFunc" )
						SerialParserState = SIO_LOOK_FOR_SYNC_1;
					}

				}	// Switch state

			}	// For each char in incoming buffer
		}	// TAL ParseData

		if( SERIAL_DBG )
		{
			ROBOT_LOG( TRUE,  "5] ");	// just show activity for each read, so we know Arduino is not dead.
		}

		__itt_task_end(pDomainArduinoThread);  // pshPicThreadReadLoop
		Sleep(0); // force thread switch after each read

	}	// while port open

	ROBOT_LOG( TRUE, "Arduino COMM Thread exiting.\n" )

#endif // #if ROBOT_TYPE == LOKI
	return 0;
}

DWORD WINAPI ArduinoCommWriteThreadFunc(LPVOID lpParameter) // g_dwArduinoCommWriteThreadId
{

#if ( (ROBOT_TYPE == LOKI) || ( (ROBOT_TYPE == TURTLE) && (TURTLE_TYPE == TURTLE_KOBUKI_WITH_ARDUINO) ) )
// Need to figure out what to do for Carbot

	// Writes a command to the Arduino and waits for an ACKnowlegement
	// Messages are sent by SendHardwareCmd calls
	// This thread is accessed via g_dwArduinoCommWriteThreadId
	IGNORE_UNUSED_PARAM(lpParameter);
	__itt_thread_set_name( "Arduino Write Thread" );

	//const char PicWriteCounterName[] = "PicWriteCount";

	MSG		msg;
	char	CmdBuf[16];
	DWORD	dwBytesWritten;
	BYTE	Request;
	BYTE	Index;
	BYTE	Value;
	BYTE	Option1;
	BYTE	Option2;
//	WORD	TempWord;
	CString strStatus;
//	BYTE	Checksum;

	DWORD	nSioResponseTime = 0;
	DWORD	nSioStartTime = 0;
	UINT	PicRetryCount = 0;
	int		DbgDotCount = 0;

	static int nWriteIndex = 0;


	// Process message loop for this thread
	while( 0 != GetMessage( &msg, NULL, WM_ROBOT_MESSAGE_BASE, (WM_ROBOT_MESSAGE_BASE+HW_MAX_MESSAGE) ) )
		// OK to filter out Windows messages, since we don't use WM_QUIT
	{
		__itt_task_begin(pDomainArduinoThread, __itt_null, __itt_null, pshPicThreadWriteLoop);

		Request = (BYTE)(msg.message - WM_ROBOT_MESSAGE_BASE);
		if( WM_ROBOT_THREAD_EXIT_CMD == msg.message ) 
		{
			break; // Quit command received!
		}

		if ( INVALID_HANDLE_VALUE == g_hArduinoCommPort )
		{
			// Usually, we should get a WM_QUIT before the COMM port closes, 
			// but if not, go ahead and exit...
			ROBOT_LOG( TRUE, "\nPIC COMM WRITE: Port INVALID HANDLE \n" )
			__itt_task_end(pDomainArduinoThread);  // pshPicThreadWriteLoop
			return 0;	
		}

		if( (msg.message < WM_ROBOT_MESSAGE_BASE) || ((msg.message > (WM_ROBOT_MESSAGE_BASE+HW_MAX_MESSAGE) ) ) )
		{
			// Ignore any messages not intended for user (where do these come from?)
			ROBOT_LOG( TRUE, "ArduinoCommWriteThread: message out of Range, WM_ROBOT_MESSAGE_BASE = 0x%08X, Msg = 0x%08lX, wParam = 0x%08lX, lParam = 0x%08lX\n", 
				WM_ROBOT_MESSAGE_BASE, msg.message, msg.wParam, msg.lParam )
			__itt_task_end(pDomainArduinoThread);  // pshPicThreadWriteLoop
			continue;
		}

		if ( (g_DownloadingFirmware) && (Request != HW_RESET_CPU) )
		{
			ROBOT_LOG( TRUE, ">>> >>> DOWNLOADING FIRMWARE TO Arduino!  CMD IGNORED!!!  <<< <<<<\n")
			__itt_task_end(pDomainArduinoThread);  // pshPicThreadWriteLoop
			continue;
		}

		if( Request == HW_UPDATE_MOTOR_SPEED_CONTROL ) 
		{
			ROBOT_LOG( TRUE, "WEIRD! HW_UPDATE_MOTOR_SPEED_CONTROLU to Arduino!\n")
		}

		if( Request == HW_SET_SERVO_POWER ) 
		{
			ROBOT_LOG( TRUE, "Sending HW_SET_SERVO_POWER to Arduino!\n")
		}

		if( Request == HW_RESET_CPU ) 
		{
			ROBOT_LOG( TRUE, ">>> Sending RESET_CPU to Arduino!\n")
		}

		// Get ready to send the message to the Arduino
		Index = (BYTE)(msg.wParam);
		Value = (BYTE)(msg.lParam);
		Option1 = 0;
		Option2 = 0;

		// TODO: CheckSum = Request+Index+Value+Option1+Option2;	// Rolls over and drops bits
		// Copy command and parameters into buffer
		// This must match ARDUINO_CMD_T - TODO, use the struct instead of this!
		CmdBuf[0] = (char)(BYTE)SIO_SYNC_0;
		CmdBuf[1] = (char)(BYTE)SIO_SYNC_1;
		CmdBuf[2] = (char)Request;
		CmdBuf[3] =	(char)Index;
		CmdBuf[4] =	(char)Value;
		CmdBuf[5] =	(char)Option1;
		CmdBuf[6] =	(char)Option2;
		CmdBuf[7] = (char)(BYTE)CMD_TERM_CHAR;	// Check to make sure no bytes were dropped

		if( HW_GET_VERSION == Request )
		{
			///TAL_Event("Arduino Requesting Version");
			//ROBOT_DISPLAY( TRUE, "\nPIC COMM WRITE: Requesting Version..." )
			__itt_marker(pDomainControlThread, __itt_null, pshVersionRequest, __itt_marker_scope_task);
			nWriteIndex = 0;
		}
		else
		{
			__itt_metadata_add(pDomainControlThread, __itt_null, pshWriteIndex, __itt_metadata_s32, 1, (void*)&nWriteIndex);
			nWriteIndex++;
		}

		BOOL bSucess = 0;

		ROBOT_LOG( SERIAL_DBG, "Arduino CMD: Sending cmd %02X to Arduino.\n", Request )
		// Don't retry or wait for status, version or Joystick commands
		if( 1 == ROBOT_SIMULATION_MODE )
		{
			__itt_task_end(pDomainArduinoThread);  // pshPicThreadWriteLoop
			RobotSleep(SIMULATED_SIO_SLEEP_TIME, pDomainArduinoThread);
			continue;
		}


		//////////////////////////////////////////////////////////////////////////
		// Write to the COM Port
		__itt_task_begin(pDomainArduinoThread, __itt_null, __itt_null, pshPicWriteFile);
		bSucess = WriteFile(g_hArduinoCommPort,	// where to write to (the open comm port)
				CmdBuf,				// what to write
				SERIAL_CMD_SIZE,	// number of bytes to be written to port
				&dwBytesWritten,	// number of bytes that were actually written
				NULL);				// overlapped I/O not supported
		__itt_task_end(pDomainArduinoThread);
		//////////////////////////////////////////////////////////////////////////

		if( !bSucess )
		{
			ROBOT_DISPLAY( TRUE, "ERROR Writing Arduino Comm port\n" )
			PurgeComm(g_hArduinoCommPort, PURGE_RXABORT|PURGE_RXCLEAR);
			//PurgeComm(hCommPort, PURGE_RXABORT|PURGE_TXABORT|PURGE_RXCLEAR|PURGE_TXCLEAR);
			DWORD dwCommError = 0;
			ClearCommError(g_hArduinoCommPort, &dwCommError, NULL);
			ReportCommError("Arduino SIO Write Error", dwCommError );

			__itt_task_end(pDomainArduinoThread);  // pshPicThreadWriteLoop
			continue;
		}
		else
		{
			if(SERIAL_CMD_SIZE != dwBytesWritten)
			{
				strStatus.Format( "Arduino SERIAL ERROR Bytes Written = %d!\n", dwBytesWritten );
				ROBOT_DISPLAY( TRUE,  (LPCTSTR)strStatus )
				__itt_task_end(pDomainArduinoThread);  // pshPicThreadWriteLoop
				continue;
			}

			ROBOT_LOG( SERIAL_DBG, "Sent %d bytes to Arduino.\n", dwBytesWritten )
			if( SERIAL_DBG )
			{
				ROBOT_LOG( TRUE,  " [TX] ");	// just show activity for each write, so we know Arduino is not dead.
				if( DbgDotCount++ > 30 )
				{
					ROBOT_LOG( TRUE,  "\n" );
					DbgDotCount = 0;
				}
			}

			if( Request == HW_RESET_CPU ) 
			{
				ROBOT_LOG( TRUE, ">>> Arduino RESET_CPU command SENT!\n")
			}
		}

		__itt_task_end(pDomainArduinoThread);  // pshPicThreadReadLoop
		Sleep(0); // force thread switch after each command
	}
	ROBOT_LOG( TRUE, "\nPIC COMM WRITE: Received WM_QUIT\n")

#endif // ROBOT_TYPE == LOKI or TURTLE_KOBUKI_WITH_ARDUINO

	return 0;
}




#endif // ( ROBOT_SERVER == 1 )	// This module used for Robot Server only
