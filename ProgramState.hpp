// ProgramState.hpp
#ifndef PROGRAM_STATE_HPP
#define PROGRAM_STATE_HPP

#include "eve_l0b.hpp"
#include "LogFileWriter.hpp"

// There is only one programState structure, and it is defined in main.cpp as a global to pass info to the imgui instance.
struct ProgramState {
	struct Args {
		std::atomic<bool> fileSpecified{false};
		std::string filename;
		std::atomic<bool> skipESP{false};
		std::atomic<bool> skipMP{false};
		std::atomic<bool> skipRecord{false};
		std::atomic<bool> slowReplay{false};
		std::atomic<bool> writeBinaryRxBuff{false};
		std::atomic<bool> readBinAsUSB{false};
	} args;
	bool guiEnabled = false;
	std::atomic<int8_t> slowReplayWaitTime{1};
	std::atomic<uint16_t> FPGA_reg0{0};
	std::atomic<uint16_t> FPGA_reg1{0};
	std::atomic<uint16_t> FPGA_reg2{0};
	std::atomic<uint16_t> FPGA_reg3{0};
	std::atomic<uint32_t> totalReadCounter{0}; // Counter of number of packets read between ESP packets
	std::atomic<uint32_t> readsPerSecond{0}; // Counter of packets read between ESP packets
	// readsPerSecond is the totalReadCounter value when an ESP packet is received

	std::atomic<uint32_t> packetsPerSecond{0}; // Number of packets read between ESP packets
	std::atomic<uint32_t> shortPacketCounter{0};

    std::atomic<bool> running{true}; // Whether the program is still running
	bool initComplete = false;
	MEGS_IMAGE_REC megsa; 
	uint8_t megsAPayloadBytes[STANDARD_MEGSAB_PACKET_LENGTH+1];
	std::atomic<bool> megsAUpdated{true};
	std::atomic<bool> isFirstMAImage{true};
	std::atomic<uint32_t> megsAImageCount{0};
	std::atomic<bool> isMATestPattern{false};
	std::atomic<int> MAypos{0};
	MEGS_IMAGE_REC megsb;
	uint8_t megsBPayloadBytes[STANDARD_MEGSAB_PACKET_LENGTH+1];
	std::atomic<bool> megsBUpdated{true};
	std::atomic<bool> isFirstMBImage{true};
	std::atomic<uint32_t> megsBImageCount{0};
	std::atomic<bool> isMBTestPattern{false};
	std::atomic<int> MBypos{0};
	PKT_COUNT_REC packetsReceived;
	std::atomic<int64_t> parityErrorsMA{0};
	std::atomic<int64_t> parityErrorsMB{0};
	std::atomic<int64_t> dataGapsMA{0};
	std::atomic<int64_t> dataGapsMB{0};
	std::atomic<int64_t> dataGapsMP{0};
	std::atomic<int64_t> dataGapsESP{0};
	std::atomic<int64_t> dataGapsSHK{0};
	std::atomic<uint32_t> saturatedPixelsMATop{0};
	std::atomic<uint32_t> saturatedPixelsMABottom{0};
	std::atomic<uint32_t> saturatedPixelsMBTop{0};
	std::atomic<uint32_t> saturatedPixelsMBBottom{0};
	ESP_PACKET esp;
	std::atomic<uint16_t> espIndex{0};
	uint8_t espPayloadBytes[STANDARD_ESP_PACKET_LENGTH+1];
	MEGSP_PACKET megsp;
	uint8_t megsPPayloadBytes[STANDARD_MEGSP_PACKET_LENGTH+1];
	SHK_PACKET shk;
	uint8_t shkPayloadBytes[STANDARD_SHK_PACKET_LENGTH+1];
};

// Extern declaration
extern ProgramState globalState;
extern std::mutex mtx;

// Initialization function
void globalStateInit();

#endif // PROGRAM_STATE_HPP
