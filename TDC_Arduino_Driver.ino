/*------------------------------------

		DRIVER FOR GP22 TDC CHIP

		See README.md for more information

		Current version:			*/

#define PROG_IDN "GP22_DRIVER"
#define PROG_VER "0.6"

		/*----------------------------------*/


#define TDC_CS A1
#define TDC_INT A0

// #define DEBUG

#include <math.h>
#include <SPI.h>

#include "CommandHandler\CommandHandler.h"

// Setup the GP22 with the currently stored config
void updateTDC(const uint32_t * registers);

// Read the registers from the TDC
void readTDC();

// Registers all the above functions with the command handler, thus defining the commands required to call them
bool registerCommands(CommandHandler<13>& h);

// Create a command handler
CommandHandler<13> handler;

// Hold TDC settings and define methods to access them
#include "GP22_Register_Access\GP22_reg.h"
uint32_t GP22::registers_data[7] = { 0 };

// Is the TDC set to automatically calibrate its results?
bool autoCalibrate = true;

#define TDC_WRITE_TO_REGISTER 0x80
#define TDC_READ_FROM_REGISTER 0xB0
#define TDC_REG0 0x00
#define TDC_REG1 0x01
#define TDC_REG2 0x02
#define TDC_REG3 0x03
#define TDC_REG4 0x04
#define TDC_REG5 0x05
#define TDC_REG6 0x06
#define TDC_STATUS 0x04
#define TDC_RESULT1 0x00
#define TDC_RESULT2 0x01
#define TDC_RESULT3 0x02
#define TDC_RESULT4 0x03

#define TDC_READ_CONFIG_FROM_EEPROM 0xF0
#define TDC_INIT 0x70
#define TDC_RESET 0x50
#define TDC_START_CAL 0x04
#define TDC_START_CAL_RES 0x03

// Function to reset the arduino:
void(*resetFunc) (void) = 0;

void setup() {

	// Default values for TDC settings
	regWrite(GP22::REG0, 0xF3076000);
	regWrite(GP22::REG1, 0x12420000);
	regWrite(GP22::REG2, 0xA0000000);
	regWrite(GP22::REG3, 0x00000000);
	regWrite(GP22::REG4, 0x10000000);
	regWrite(GP22::REG5, 0x00000000);
	regWrite(GP22::REG6, 0x400010CB);

	// Serial connection
	Serial.begin(250000);

	// Setup the interrupt pin for input
	pinMode(TDC_INT, INPUT);

	// Setup the Chip Select pin for output
	pinMode(TDC_CS, OUTPUT);
	digitalWrite(TDC_CS, HIGH);

	// Load the possible serial commands
	if (!registerCommands(handler)) {
    Serial.println(F("Error in command registration"));
	}

	// Initialize the SPI connection for the GP22 TDC:

	SPI.begin();
	/* " The TDC-GP22 does only support the following SPI mode (Motorola specification):
	Clock Phase Bit = 1
	Clock Polarity Bit = 0 "    =>          */
	SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE1));

	// Load all the values stored in reg into the TDC's registers
	updateTDC(GP22::registers_data);

	// Setup complete. Move to waiting for a command
}

void loop() {
	if (handler.commandWaiting()) {

#ifdef DEBUG
		Serial.println(F("Command received."));
#endif

		CommandHandlerReturn out = handler.executeCommand();

		if (out) {
			Serial.print(F("Error in command: CommandHandlerReturn code "));
			Serial.println(out);
		}
	}
}

void serialEvent() {
	while (Serial.available()) {
		char c = Serial.read();

#ifdef DEBUG
		Serial.print(F("Adding: ["));
		Serial.print(c, DEC);
		Serial.println(F("]"));
#endif

		handler.addCommandChar(c);
	}
}

void reset(const ParameterLookup& params) {
	Serial.println(F("Resetting"));
	Serial.flush();
	resetFunc();
}

void identity(const ParameterLookup& params) {
	Serial.print(F(PROG_IDN));
	Serial.print(F(" - "));
	Serial.println(F(PROG_VER));
}

void timedMeasure(const ParameterLookup& params) {

	// Number of ms to read for
	uint32_t timePeriod = atoi(params[1]);

	// Calculate stop time
	uint32_t stop = millis() + timePeriod;

	// Loop until stoptime
	while (millis() < stop) {

		// Do the measurement
		uint32_t result;
		int stat = measure(result);

		// Check we didn't timeout
		if (stat == 0) {
			// Report result
			Serial.print(result);
			Serial.print('\t');
		}
	}

	// newline to terminate
	Serial.println("");
}

// Command: "SETUp REG1 REG2 REG3 REG4 REG5 REG6 REG7" Registers as base 10 numbers
void setupRegisters(const ParameterLookup& params) {

	// Reset the TDC
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_RESET);
	digitalWrite(TDC_CS, HIGH);

	// Iterate over param list and reg list
	for (int i = 1; i < params.size() && i <= sizeof(GP22::registers_data) / sizeof(GP22::registers_data[0]); i++) {

		// Get the next String, convert to a long and save in the appropriate register
    uint32_t newRegVal = strtoul(params[i], NULL, 0);
	GP22::regWrite(GP22::registers(i-1), newRegVal);

#ifdef DEBUG
    Serial.print(F("Setting REG "));
    Serial.print(i-1);
    Serial.println(F(": "));
    Serial.print(newRegVal);
    Serial.print(F(" = 0x"));
    Serial.print(newRegVal, HEX);
    Serial.print(F(", parsed from : "));
    Serial.println(params[i]);
#endif
	}

	// Decide if we've been asked for calibration mode or not (bit 13 in reg 0)
	autoCalibrate = (bool)bitmaskRead(GP22::REG0, GP22::REG0_CALIBRATE);

	// Write the values to the TDC's registers
	updateTDC(GP22::registers_data);

	delay(1);

	// Read back from the Most Significant 8 bits of register 1 (should match GP22::registers_data[1])
	// Command:
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_READ_FROM_REGISTER | TDC_REG5);
	// Data:
	byte commsCheck = SPI.transfer(0x00);
	digitalWrite(TDC_CS, HIGH);

	byte shouldBe = (regRead(GP22::REG1) & 0xFF000000) >> 24;

	if (commsCheck == shouldBe) {
		Serial.print("DONE - CALIBRATION MODE ");
		if (autoCalibrate)
			Serial.print("ON - ");
		else
			Serial.print("OFF - ");

		Serial.println(commsCheck, HEX);

	}
	else {
		Serial.print("Error. Read: 0x");
		Serial.print(commsCheck, HEX);
		Serial.print(" instead of 0x");
		Serial.print(shouldBe, HEX);
		Serial.print(" from reg[1] == ");
		Serial.println(regRead(GP22::REG1), HEX);

	}
}

void getRegisters(const ParameterLookup& params) {

	// Output the current register state
	for (int i = 0; i < 7; i++) {
		Serial.print("0x");
		Serial.print(regRead(GP22::registers(i)), HEX);
		Serial.print('\t');
	}

	Serial.println("");

}

void singleMeasure(const ParameterLookup& params) {

	// Do the measurement
	uint32_t result;
	int stat = measure(result);

	// Report result
	if (0 == stat)
	  Serial.println(result);
  else
    Serial.println(F("TIMEOUT"));

}

// Calibrate the TDC against the reference 32kHz clock and report the result
void calibrateTDC(const ParameterLookup& params) {

	// Do the calibration
	uint16_t calib = calibrate();

	// Report result
	Serial.println(calib);

}

// Calibrate the highspeed clock against the TDC and report the result
// (i.e. number of high speed clock cycles in `ANZ_PER_CALRES` cycles of the ref clock)
void calibrateResonator(const ParameterLookup& params) {

	// Do the calibration
	uint32_t calib = calibrateHF();

	// Report result
	Serial.println(calib);

}

void testConnection(const ParameterLookup& params) {

	// Run the test
	uint8_t testResult = testTDC();

	// Report the result
	if (testResult) {
		Serial.print("FAILED with code returned ");
		Serial.println(testResult);
	}
	else {
		Serial.println("PASSED");
	}
}

void getStatus(const ParameterLookup& params) {

  Serial.print(F("0x"));
	Serial.println(readStatus(), HEX);

}

// ******* END OF COMMANDS ******* //

// ******* START OF INTERNAL FUNCTIONS ******* //

// Write to the GP22 then read to check comms
uint8_t testTDC() {

	// Send reset
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_RESET);
	digitalWrite(TDC_CS, HIGH);

	// Wait 100ms
	delay(100);

	// Get previous value of reg1's top 8 bits
	uint8_t prevReg1 = GP22::bitmaskRead(GP22::REG1, GP22::REG1_TEST_DATA);

	// Write test data (0xAB) into reg1
	const uint8_t testdata = 0xAB;
	GP22::bitmaskWrite(GP22::REG1, GP22::REG1_TEST_DATA, testdata);
	updateTDC(GP22::registers_data);

	// Read back from the first 8 bits of register 1 (should match testdata)
	// Command:
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_READ_FROM_REGISTER | TDC_REG5);
	// Data:
	uint8_t commsTest = SPI.transfer(0x00);
	digitalWrite(TDC_CS, HIGH);


// Restore settings to previous
	GP22::bitmaskWrite(GP22::REG1, GP22::REG1_TEST_DATA, prevReg1);
	updateTDC(GP22::registers_data);

	// Return 0 for success
	if (commsTest == testdata)
		return 0;

	// If we failed, return the value we just read, unless that value is 0 in which case return "0xFF"
	else
		if (commsTest != 0)
			return commsTest;
		else
			return 0xFF;
}

// Setup the GP22 with the currently stored config
// registers should be a 7 element array
void updateTDC(const uint32_t * registers) {

	// Send reset
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_RESET);
	digitalWrite(TDC_CS, HIGH);

	// Wait 100ms
	delay(100);

	// Set defaults
	// Write the values to the TDC's registers
	for (int i = 0; i < 7; i++) {
		writeConfigReg(i, registers[i]);
	}

	// Wait 100ms
	delay(100);
}

// Perform a single measurement & output to the passed variable
// Return 1 for timeout
// 0 for success
int measure(uint32_t& out) {

	// Send the INIT opcode to start waiting for a timing event
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_INIT);
	digitalWrite(TDC_CS, HIGH);

	// Wait until interrupt goes low indicating a successful read
	uint32_t start = millis();
	while (HIGH == digitalRead(TDC_INT)) {
		if (millis() - start > 500) { return 1; } // Give up if we've been waiting 500ms
	}

	// Read the result
	out = read_bytes(TDC_RESULT1, !autoCalibrate);

	return 0;
}

// Perform a calibration routine and then return the number of LSBs in 2 clock cycles
// The default clock setting is 4 MHz, so a measurement of x LSBs in 2 clock cycles corresponds to
//	a precision of 1/4MHz * 2 cycles / x
uint16_t calibrate() {

	// This sequence is adapted from the ACAM eval software source code (in Labview)

	// Backup registers
	uint32_t backup[7];
	for (int i = 0; i < 7; i++) {
		backup[i] = regRead(GP22::registers(i));
	}

	// Goto single res. mode
	//writeConfigReg(TDC_REG6, reg6 & 0xFFFFCFFF);

	// Goto double res. mode
	//writeConfigReg(TDC_REG6, (reg6 & 0xFFFFCFFF) | 0x1000);

	// Goto quad res. mode
	GP22::bitmaskWrite(GP22::REG0, GP22::REG0_MESSB2, true); // Mes. mode. 2
	GP22::bitmaskWrite(GP22::REG0, GP22::REG0_CALIBRATE, false); // No auto cal
	GP22::bitmaskWrite(GP22::REG6, GP22::REG6_QUAD_RES, true); // Quad res on
	GP22::bitmaskWrite(GP22::REG6, GP22::REG6_DOUBLE_RES, false); // Double res off
	updateTDC(GP22::registers_data);

	// Send INIT so that the TDC is ready to give a response
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_INIT);
	digitalWrite(TDC_CS, HIGH);

	// Send the START_CAL_TDC opcode to measure the calibration data
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_START_CAL);
	digitalWrite(TDC_CS, HIGH);

	// Request that the ALU calculates the calibration difference by writing
	// into register 1. This tells the ALU what to calculate and also triggers the calculation
	// See p.52 of the ACAM manual
	// Our calculation is CALI2 - CALI1 == T_ref
	GP22::bitmaskWrite(GP22::REG1, GP22::REG1_HIT1, 7); // Request Cal2...
	GP22::bitmaskWrite(GP22::REG1, GP22::REG1_HIT2, 6); // ...minus Cal1
	
	GP22::bitmaskWrite(GP22::REG1, GP22::REG1_HITIN1, 0); // Expect 0 hits
	GP22::bitmaskWrite(GP22::REG1, GP22::REG1_HITIN2, 0);

	writeConfigReg(GP22::REG1, regRead(GP22::REG1)); // Go!
	delay(1);

	// Read ALU_PTR from the status and subtract 1 to get the location of the most recently written
	//   measurement (the calibration)
	uint8_t storageLocation = (readStatus() & 0x7) - 1;

	// Read result
	uint16_t calibration;

	// Check that we actually took a measurement. If ALU_PTR was 0, ALU_PTR-1 == 0xFF and we failed
	if (storageLocation == 0xFF) { calibration = 0xFFFF; } // Return error
	else { // Read and return data
		calibration = read_bytes(storageLocation, true);
	}

	// Restore registers
	for (int i = 0; i < 7; i++) {
		regWrite(GP22::registers(i), backup[i]);
	}
	updateTDC(GP22::registers_data);

	return calibration;
}

uint32_t calibrateHF() {

	// Backup registers
	uint32_t backupReg3;
	backupReg3 = regRead(GP22::REG3);

	// Set EN_AUTOCALC=0
	GP22::bitmaskWrite(GP22::REG3, GP22::REG3_EN_AUTOCALC_MB2, false);
	writeConfigReg(GP22::REG3, regRead(GP22::REG3));

	// Init
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_INIT);
	digitalWrite(TDC_CS, HIGH);

	// Start the calibration
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_START_CAL_RES);
	digitalWrite(TDC_CS, HIGH);

	// Wait until interrupt goes low indicating a successful read
	uint32_t start = millis();
	while (HIGH == digitalRead(TDC_INT)) {
		if (millis() - start > 500) { return 0xFFFFFFFF; } // Give up if we've been waiting 500ms
	}

	//The time interval to be measured is set by ANZ_PER_CALRES
	//which defines the number of periods of the 32.768 kHz clock:
	//2 periods = 61.03515625 us
	// But labview / the user will handle this, we just output the raw data
	uint32_t result = read_bytes(TDC_RESULT1, false);

	// Restore reg3
	writeConfigReg(GP22::REG3, backupReg3);

	return result;
}

// Read status
// The device's format is a 16 bit number
// See p.36 of the ACAM datasheet
uint16_t readStatus() {

	// Read in the data
	uint16_t status = read_bytes(TDC_STATUS, true);

	return status;
}

// Write the given data into the target register
void writeConfigReg(uint8_t targetReg, uint32_t data) {

	// Store the 32 bit int in the same memory as 4 bytes
	union {
		uint32_t raw;
		byte bytes[4];
	} conversion;

	conversion.raw = data;

	// Command to start transfer into the given register
	digitalWrite(TDC_CS, LOW);
	SPI.transfer(TDC_WRITE_TO_REGISTER | targetReg);

	// The data to write
	SPI.transfer(conversion.bytes[3]);
	SPI.transfer(conversion.bytes[2]);
	SPI.transfer(conversion.bytes[1]);
	SPI.transfer(conversion.bytes[0]);
	digitalWrite(TDC_CS, HIGH);

}

// Read result
// The device's format is a 32 bit fixed point number with 16 bits for the 
// fractional part. The following reads either 16 or 32 bits, MSBs first. 
// If it reads 16, the return value will be 0x0000RRRR where RRRR is the result bytes
// If the 16 bits are 0xFFFF, the return value will be 0xFFFFFFFF
uint32_t read_bytes(uint8_t reg, bool read16bits) {

	union {
		byte raw[4];
		uint32_t proc32;
		uint16_t proc16[2];
	} result;

	// Select slave
	digitalWrite(TDC_CS, LOW);

	SPI.transfer(TDC_READ_FROM_REGISTER | reg);

	result.raw[3] = SPI.transfer(0x00);
	result.raw[2] = SPI.transfer(0x00);

	if (read16bits) { // If only reading 16 bits, stop here

		// Deselect slave
		digitalWrite(TDC_CS, HIGH);

		// Check for timeout
		if (result.proc16[1] == 0xFFFF)
			return 0xFFFFFFFF;

		// Else return the result
		// N.B we're only interested in proc16[1]
		//    proc16[0] was never read into and will contain zeros / garbage
		return result.proc16[1];
	}

	// Otherwise, read all 32 bits
	result.raw[1] = SPI.transfer(0x00);
	result.raw[0] = SPI.transfer(0x00);

	// Deselect slave
	digitalWrite(TDC_CS, HIGH);

	return result.proc32;
}

// this function will return the number of bytes currently free in RAM
// written by David A. Mellis
// based on code by Rob Faludi http://www.faludi.com
void availableMemory(const ParameterLookup& params) {
	//int size = 1024; // Use 2048 with ATmega328
	int size = 2048;
	byte *buf;

	while ((buf = (byte *)malloc(--size)) == NULL)
		;

	free(buf);

	Serial.print(size);
	Serial.println(F(" bytes remain"));
}


bool registerCommands(CommandHandler<13>& h) {
	// N.B. commands are not case sensitive

  bool error = false;
  
	error |= h.registerCommand("*IDN", 0, &identity);
 error |= h.registerCommand("*IDN?", 0, &identity);
error |=   h.registerCommand("*TST", 0, &testConnection);
error |=   h.registerCommand("*TST?", 0, &testConnection);
	error |= h.registerCommand("*RST", 0, &reset);
	error |= h.registerCommand("MEAS", 1, &timedMeasure);
	error |= h.registerCommand("SING", 0, &singleMeasure);
	error |= h.registerCommand("STAT", 0, &getStatus);
	error |= h.registerCommand("SETU", -1, &setupRegisters);
	error |= h.registerCommand("SETU?", 0, &getRegisters);
	error |= h.registerCommand("*MEM", 0, &availableMemory);
	error |= h.registerCommand("HCAL", 0, &calibrateResonator);
	error |= h.registerCommand("CALI", 0, &calibrateTDC);

 return !error;

}
