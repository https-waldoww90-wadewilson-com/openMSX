// $Id$

#include "MSXGameCartridge.hh"
#include "SCC.hh"
#include "DACSound.hh"
#include "MSXCPU.hh"
#include "CPU.hh"
#include <map>

MSXGameCartridge::MSXGameCartridge(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time), MSXMemDevice(config, time), MSXRom(config, time)
{
	PRT_DEBUG("Creating an MSXGameCartridge object");
	try {
		try {
			romSize = deviceConfig->getParameterAsInt("filesize");
			if (romSize == 0) {
				// filesize zero is specifiying to autodetermine size
				throw MSXConfig::Exception("auto detect");
			}
			loadFile(&memoryBank, romSize);
		} catch(MSXConfig::Exception& e) {
			// filesize was not specified or 0
			romSize = loadFile(&memoryBank);
		}
	} catch (MSXException &e) {
		// TODO why isn't exception thrown beyond constructor
		PRT_ERROR(e.desc);
	}
	
	// Calculate mapperMask
	int nrblocks = romSize>>13;	//number of 8kB pages
	for (mapperMask=1; mapperMask<nrblocks; mapperMask<<=1);
	mapperMask--;
	
	mapperType = retrieveMapperType();

	// only instantiate SCC if needed
	if (mapperType==2) {
		short volume = (short)config->getParameterAsInt("volume");
		cartridgeSCC = new SCC(volume);
	} else {
		cartridgeSCC = NULL;
	}
	
	// only instantiate DACSound if needed
	if (mapperType==64) {
		short volume = (short)config->getParameterAsInt("volume");
		dac = new DACSound(volume, 16000, time);
	} else {
		dac = NULL;
	}

	reset(time);
}

MSXGameCartridge::~MSXGameCartridge()
{
	PRT_DEBUG("Destructing an MSXGameCartridge object");
	if (dac)
		delete dac;
	if (cartridgeSCC)
		delete cartridgeSCC;
}

void MSXGameCartridge::reset(const EmuTime &time)
{
	if (cartridgeSCC) {
		cartridgeSCC->reset();
	}
	enabledSCC = false;
	if (dac) {
		dac->reset(time);
	}
	if (mapperType < 128 ) {
		// TODO: mirror if number of 8kB blocks not fully filled ?
		setBank(0, 0);			// unused
		setBank(1, 0);			// unused
		setBank(2, memoryBank);		// 0x4000 - 0x5fff
		setBank(3, memoryBank+0x2000);	// 0x6000 - 0x7fff
		if (mapperType == 5) {
			setBank(4, memoryBank);	// 0x8000 - 0x9fff
			setBank(5, memoryBank+0x2000);	// 0xa000 - 0xbfff
		} else {
			setBank(4, memoryBank+0x4000);	// 0x8000 - 0x9fff
			setBank(5, memoryBank+0x6000);	// 0xa000 - 0xbfff
		}
		setBank(6, 0);			// unused
		setBank(7, 0);			// unused
	} else {
		// this is a simple gamerom less then 64 kB
		byte* ptr;
		switch (romSize>>14) { // blocks of 16kB
		case 0:
			// An 8 Kb game ????
			for (int i=0; i<8; i++) {
				setBank(i, memoryBank);
			}
			break;
		case 1:
			for (int i=0; i<8; i+=2) {
				setBank(i,   memoryBank);
				setBank(i+1, memoryBank+0x2000);
			}
			break;
		case 2:
			setBank(0, memoryBank);		// 0x0000 - 0x1fff
			setBank(1, memoryBank+0x2000);	// 0x2000 - 0x3fff
			setBank(2, memoryBank);		// 0x4000 - 0x5fff
			setBank(3, memoryBank+0x2000);	// 0x6000 - 0x7fff
			setBank(4, memoryBank+0x4000);	// 0x8000 - 0x9fff
			setBank(5, memoryBank+0x6000);	// 0xa000 - 0xbfff
			setBank(6, memoryBank+0x4000);	// 0xc000 - 0xdfff
			setBank(7, memoryBank+0x6000);	// 0xe000 - 0xffff
			break;
		case 3:
			// TODO 48kb, is this possible?
			//assert (false);
			//break;
		case 4:
			ptr = memoryBank;
			for (int i=0; i<8; i++) {
				setBank(i, ptr);
				ptr += 0x2000;
			}
			break;
		default:
			// not possible
			assert (false);
		}
	}
}

struct ltstr {
	bool operator()(const char* s1, const char* s2) const {
		return strcmp(s1, s2) < 0;
	}
};

int MSXGameCartridge::retrieveMapperType()
{
	try {
		if (deviceConfig->getParameterAsBool("automappertype")) {
			return guessMapperType();
		} else {
			std::string  type = deviceConfig->getParameter("mappertype");
			PRT_DEBUG("Using mapper type " << type);

			map<const char*, int, ltstr> mappertype;

			mappertype["0"]=0;
			mappertype["8kB"]=0;

			mappertype["1"]=1;
			mappertype["16kB"]=1;

			mappertype["2"]=2;
			mappertype["KONAMI5"]=2;
			mappertype["SCC"]=2;

			mappertype["3"]=3;
			mappertype["KONAMI4"]=3;

			mappertype["4"]=4;
			mappertype["ASCII8"]=4;

			mappertype["5"]=5;
			mappertype["ASCII16"]=5;

			mappertype["6"]=6;
			mappertype["GAMEMASTER2"]=6;

			mappertype["64"]=64;
			mappertype["KONAMIDAC"]=64;

			//TODO: catch wrong options passed
			return mappertype[type.c_str()];
		}
	} catch (MSXConfig::Exception& e) {
		// missing parameter
		return guessMapperType();
	}
}

int MSXGameCartridge::guessMapperType()
{
	//  GameCartridges do their bankswitching by using the Z80
	//  instruction ld(nn),a in the middle of program code. The
	//  adress nn depends upon the GameCartridge mappertype used.
	//  To guess which mapper it is, we will look how much writes
	//  with this instruction to the mapper-registers-addresses
	//  occure.

	if (romSize <= 0x10000) {
		if (romSize == 0x10000) {
			// There are some programs convert from tape to 64kB rom cartridge
			// these 'fake'roms are from the ASCII16 type
			return 5;
		} else if (romSize == 0x8000) {
			//TODO: Autodetermine for KonamiSynthesiser
			// works for now since most 32 KB cartridges don't
			// write to themselves
			return 64;
		} else {
			// if != 32kB it must be a simple rom so we return 128
			return 128;
		}
	} else {
		unsigned int typeGuess[]={0,0,0,0,0,0};
		for (int i=0; i<romSize-2; i++) {
			if (memoryBank[i] == 0x32) {
				int value = memoryBank[i+1]+(memoryBank[i+2]<<8);
				switch (value) {
				case 0x5000:
				case 0x9000:
				case 0xB000:
					typeGuess[2]++;
					break;
				case 0x4000:
				case 0x8000:
				case 0xA000:
					typeGuess[3]++;
					break;
				case 0x6800:
				case 0x7800:
					typeGuess[4]++;
					break;
				case 0x6000:
					typeGuess[3]++;
					typeGuess[4]++;
					typeGuess[5]++;
					break;
				case 0x7000:
					typeGuess[2]++;
					typeGuess[4]++;
					typeGuess[5]++;
					break;
				case 0x77FF:
					typeGuess[5]++;
				}
			}
		}
		if (typeGuess[4]) typeGuess[4]--; // -1 -> max_int
		int type = 0;
		for (int i=0; i<6; i++) {
			if ( (typeGuess[i]) && (typeGuess[i]>=typeGuess[type])){
			  type = i;
			}
		};
		// in case of doubt we go for type 0
		// in case of even type 5 and 4 we would prefer 5
		// but we would still prefer 0 above 4 or 5
		if ((type==5) && (typeGuess[0] == typeGuess[5] ) ){type=0;};
	for (int i=0; i<6; i++) {
	PRT_DEBUG("MSXGameCartridge: typeGuess["<<i<<"]="<<typeGuess[i]);
	}
		PRT_DEBUG("MSXGameCartridge: I Guess this is a nr " << type << " GameCartridge mapper type.")
		return type;
	}
}

byte MSXGameCartridge::readMem(word address, const EmuTime &time)
{
	//TODO optimize this!!!
	// One way to optimise would be to register an SCC supporting
	// device only if mapperType is 2 and only in 8000..BFFF.
	// That way, there is no SCC overhead in non-SCC pages.
	// If MSXMotherBoard would support hot-plugging of devices,
	// it would be possible to insert an SCC supporting device
	// only when the SCC is enabled.
	if (enabledSCC && 0x9800 <= address && address < 0xA000) {
		return cartridgeSCC->readMemInterface(address & 0xFF, time);
	}
	return internalMemoryBank[address>>13][address&0x1fff];
}

byte* MSXGameCartridge::getReadCacheLine(word start)
{
	if (enabledSCC && 0x9800<=start && start<0xA000) {
		// don't cache SCC
		return NULL;
	} else {
		// assumes CACHE_LINE_SIZE <= 0x2000
		return &internalMemoryBank[start>>13][start&0x1fff];
	}
}

void MSXGameCartridge::writeMem(word address, byte value, const EmuTime &time)
{
	/*
	cerr << std::hex << "GameCart[" << address << "] := "
		<< (int)value << std::dec << "\n";
	*/
	switch (mapperType) {
	byte region;
	case 0:
		//--==>> Generic 8kB cartridges <<==--
		// Unlike fMSX, our generic 8kB mapper does not have an SCC.
		// After all, such hardware does not exist in reality.
		// If you want an SCC, use the Konami5 mapper type.

		if (address<0x4000 || address>=0xC000) return;
		// change internal mapper
		value &= mapperMask;
		setBank(address>>13, memoryBank+(value<<13));
		break;
	case 1:
		//--==**>> Generic 16kB cartridges (MSXDOS2, Hole in one special) <<**==--
		if (address<0x4000 || address>=0xC000)
			return;
		region = (address&0xC000)>>13;	// 0, 2, 4, 6
		value = (2*value)&mapperMask;
		setBank(region,   memoryBank+(value<<13));
		setBank(region+1, memoryBank+(value<<13)+0x2000);
		break;
	case 2:
		//--==**>> KONAMI5 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do have an SCC and some others
		// examples of cartridges: Nemesis 2, Nemesis 3, King's Valley 2, Space Manbow
		// Solid Snake, Quarth, Ashguine 1, Animal, Arkanoid 2, ...
		//
		// The address to change banks:
		//  bank 1: 0x5000 - 0x57FF (0x5000 used)
		//  bank 2: 0x7000 - 0x77FF (0x7000 used)
		//  bank 3: 0x9000 - 0x97FF (0x9000 used)
		//  bank 4: 0xB000 - 0xB7FF (0xB000 used)

		if (address<0x5000 || address>=0xC000) return;
		// Write to SCC?
		if (enabledSCC && 0x9800<=address && address<0xA000) {
			cartridgeSCC->writeMemInterface(address & 0xFF, value , time);
			PRT_DEBUG("GameCartridge: writeMemInterface (" << address <<","<<(int)value<<")"<<time );
			// No page selection in this memory range.
			return;
		}
		// SCC enable/disable?
		if ((address & 0xF800) == 0x9000) {
			enabledSCC = ((value&0x3F)==0x3F);
			MSXCPU::instance()->invalidateCache(0x9800, 0x0800/CPU::CACHE_LINE_SIZE);
		}
		// Page selection?
		if ((address & 0x1800) != 0x1000) return;
		value &= mapperMask;
		setBank(address>>13, memoryBank+(value<<13));
		break;
	case 3:
		//--==**>> KONAMI4 8kB cartridges <<**==--
		// this type is used by Konami cartridges that do not have an SCC and some others
		// example of catrridges: Nemesis, Penguin Adventure, Usas, Metal Gear, Shalom,
		// The Maze of Galious, Aleste 1, 1942,Heaven, Mystery World, ...
		//
		// page at 4000 is fixed, other banks are switched
		// by writting at 0x6000,0x8000 and 0xA000

		if (address&0x1FFF || address<0x6000 || address>=0xC000) return;
		value &= mapperMask;
		setBank(address>>13, memoryBank+(value<<13));
		break;
	case 4:
		//--==**>> ASCII 8kB cartridges <<**==--
		// this type is used in many japanese-only cartridges.
		// example of cartridges: Valis(Fantasm Soldier), Dragon Slayer, Outrun, Ashguine 2, ...
		//
		// The address to change banks:
		//  bank 1: 0x6000 - 0x67FF (0x5000 used)
		//  bank 2: 0x6800 - 0x6FFF (0x7000 used)
		//  bank 3: 0x7000 - 0x77FF (0x9000 used)
		//  bank 4: 0x7800 - 0x7FFF (0xB000 used)

		if (address<0x6000 || address>=0x8000) return;
		region = ((address>>11)&3)+2;
		value &= mapperMask;
		setBank(region, memoryBank+(value<<13));
		break;
	case 5:
		//--==**>> ASCII 16kB cartridges <<**==--
		// this type is used in a few cartridges.
		// example of cartridges: Xevious, Fantasy Zone 2,
		// Return of Ishitar, Androgynus, Gallforce ...
		//
		// The address to change banks:
		//  first  16kb: 0x6000 - 0x67FF (0x6000 used)
		//  second 16kb: 0x7000 - 0x77FF (0x7000 and 0x77FF used)

		if (address<0x6000 || address>=0x7800 || (address&0x800)) return;
		region = ((address>>11)&2)+2;
		value = (2*value)&mapperMask;
		setBank(region,   memoryBank+(value<<13));
		setBank(region+1, memoryBank+(value<<13)+0x2000);
		break;
	case 6:
		//--==**>> GameMaster2+SRAM cartridge <<**==--
		//// GameMaster2 mayi become an independend MSXDevice
		assert(false);
		break;
	case 64:
		//--==**>> <<**==--
		// Konami Synthezier cartridge
		// Should be only for 0x4000, but since this isn't confirmed on a real
		// cratridge we will simply use every write.
		dac->writeDAC(value,time);
		// fall through:
	case 128:
		//--==**>> Simple romcartridge <= 64 KB <<**==--
		// No extra writable hardware
		break;
	default:
		//// Unknow mapper type for GameCartridge cartridge!!!
		assert(false);
	}
}

void MSXGameCartridge::setBank(int region, byte* value)
{
	internalMemoryBank[region] = value;
	MSXCPU::instance()->invalidateCache(region*0x2000, 0x2000/CPU::CACHE_LINE_SIZE);
}
