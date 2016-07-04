#pragma once

#define COMMAND_SIZE_MAX 128 // num chars
#define DEFAULT_NUM_COMMANDS_MAX 10 // Number of commands that need to be stored

// Storage locations in EEPROM for commands
#include <EEPROM.h>
#define EEPROM_STORED_COMMAND_FLAG_LOCATION 0
#define EEPROM_STORED_COMMAND_LOCATION EEPROM_STORED_COMMAND_FLAG_LOCATION + sizeof(bool)

// To disable EEPROM features, set this flag:
// #define EEPROM_DISABLED

#include "basicList.h"
#include "shared_ptr_d.h"

//////////////////////  COMMAND LOOKUP  //////////////////////

	// Template for the functions we'll be calling
	typedef void commandFunction (const List<shared_ptr_d<String>>& params);

	// Structure of the data to be stored for each command
	struct dataStruct {
		char * key; // Keyword for this command
		int n; // Number of params this function takes
		commandFunction* f; // Pointer to this function
	};

	// Error messages for executing a command
	enum ExecuteError {
		NO_ERROR = 0,
		COMMAND_NOT_FOUND,
		WRONG_NUM_OF_PARAMS,
		ERROR_PARSING_COMMAND,
		EMPTY_COMMAND_STRING,
		NO_COMMAND_WAITING,
		MALLOC_ERROR, 
		OUT_OF_MEM
	};

	class CommandLookup
	{
	public:

		CommandLookup() {}

		// Add a new command to the list
		void registerCommand(const char* command, int num_of_parameters,
			commandFunction* pointer_to_function);

		// Search the list of commands for the given command and execute it with the given parameter array
		ExecuteError callStoredCommand(const char* command, const List<shared_ptr_d<String>>& params);

	protected:

		List<dataStruct> _commandList;
	};

//////////////////////  COMMAND HANDLER  //////////////////////

	class CommandHandler
	{
	public:
		// Constuctor
		// Initialise private members and queue any stored command in the EEPROM
		CommandHandler();

		// Wipe any char*s left in the queue or buffer
		~CommandHandler() {
			free(_inputBuffer);
		}

		// Is a command waiting in the queue?
		bool commandWaiting() {
			return !_commandQueue.isEmpty();
		}

		// How long is the queue?
		int queueLength() {
			return _commandQueue.size();
		}

		// Dump the whole command queue for debugging
		void debug() {
			_commandQueue.debug();
		}

#ifndef EEPROM_DISABLED
		// Store a command to be executed on startup in the EEPROM
		// This command should not include newlines: it will be copied verbatim into the
		// buffer and then executed as a normal command would be
		// Multiple commands can be seperated by ';' chars
		// Max length is COMMAND_LENGTH_MAX - 2 (1 char to append a newline, 1 for the null term)
		// Returns false on fail
		bool storeStartupCommand(const String& command);

		// Store a command to be executed on startup in the EEPROM
		// This command should not include newlines: it will be copied verbatim into the
		// buffer and then executed as a normal command would be
		// Multiple commands can be seperated by ';' chars
		// Max length is COMMAND_LENGTH_MAX - 2 (1 char to append a newline, 1 for the null term)
		// Returns false on fail
		bool storeStartupCommand(const char* command);
		
		// Remove any startup commands from the EEPROM
		bool wipeStartupCommand();

		// Return any stored startup command. Returns "" if no command stored
		String getStartupCommand();

		// Return any stored startup command by copying into buf.
		// This has the same functionality as the other form and 
		// avoids memory allocations on the heap, but places responsibility
		// for memory management on the user
		// buf must point to a buffer of at least COMMAND_SIZE_MAX chars
		void getStartupCommand(char * buf);

		// Queue the startup command stored in the EEPROM
		// Returns true on success, false on failure or if no command is stored
		bool queueStartupCommand();
#endif

		// Execute the next command in the queue
		ExecuteError executeCommand();

		// Register a command
		void registerCommand(const char* command, int num_of_parameters,
				commandFunction* pointer_to_function) {
			_lookupList.registerCommand(command, num_of_parameters,
					pointer_to_function);
		}

		// Add a char from the serial connection to be processed and added to the queue
		void addCommandChar(const char c) ;

	private:
		CommandLookup _lookupList;

		// A queue of commands (Strings)
		List <String> _commandQueue;

		// A buffer for receiving new commands
		char _inputBuffer[COMMAND_SIZE_MAX + 1];
		int _bufferLength;

		// A flag to report that the command currently being received has overrun
		bool _command_too_long;

		// Find the location in a command string where the command starts
		int findStartOfCommand(const char* str);

		// Find the location in a command string where the command ends and the params start
		int findEndOfCommand(const char* str, int startPoint = 0);

		// Loop from the first space onwards, counting the params
		int numParamsInCommandStr(const char* str, int endOfCommand) ;

		// Parse a string to extract the parameters and store them in destList
		int readParamsFromStr(const char* str, int endOfCommand, List<shared_ptr_d<String>>& destList);

		
	};

