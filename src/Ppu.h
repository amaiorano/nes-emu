#pragma once
#include "Base.h"
#include "Memory.h"
#include "Bitfield.h"
#include <memory>

class Renderer;
class PpuMemoryBus;
class Nes;

class Ppu
{
public:
	Ppu();
	void Initialize(PpuMemoryBus& ppuMemoryBus, Nes& nes);

	void Reset();
	void Serialize(class Serializer& serializer);

	void Execute(uint32 cpuCycles, bool& completedFrame);
	void RenderFrame(); // Call when Execute() sets completedFrame to true

	uint8 HandleCpuRead(uint16 cpuAddress);
	void HandleCpuWrite(uint16 cpuAddress, uint8 value);
	uint8 HandlePpuRead(uint16 ppuAddress);
	void HandlePpuWrite(uint16 ppuAddress, uint8 value);

private:
	uint16 MapCpuToPpuRegister(uint16 cpuAddress);
	uint16 MapPpuToVRam(uint16 ppuAddress);
	uint16 MapPpuToPalette(uint16 ppuAddress);

	uint8 ReadPpuRegister(uint16 cpuAddress);
	void WritePpuRegister(uint16 cpuAddress, uint8 value);

	void ClearBackground();
	void FetchBackgroundTileData();
	
	void ClearOAM2(); // OAM2 = $FF
	void PerformSpriteEvaluation(uint32 x, uint32 y); // OAM -> OAM2
	void FetchSpriteData(uint32 y); // OAM2 -> render (shift) registers

	void RenderPixel(uint32 x, uint32 y);
	void SetVBlankFlag();
	void OnFrameComplete();

	PpuMemoryBus* m_ppuMemoryBus;
	Nes* m_nes;
	std::shared_ptr<Renderer> m_rendererHolder;
	Renderer* m_renderer;

	// Memory used to store name/attribute tables (aka CIRAM)
	typedef Memory<FixedSizeStorage<KB(2)>> NameTableMemory;
	NameTableMemory m_nameTables;

	typedef Memory<FixedSizeStorage<32>> PaletteMemory;
	PaletteMemory m_palette;

	static const size_t kMaxSprites = 64;
	static const size_t kSpriteDataSize = 4;
	static const size_t kSpriteMemorySize = kMaxSprites * kSpriteDataSize;
	typedef Memory<FixedSizeStorage<kSpriteMemorySize>> ObjectAttributeMemory; // Sprite memory
	ObjectAttributeMemory m_oam;

	typedef Memory<FixedSizeStorage<kSpriteDataSize * 8>> ObjectAttributeMemory2;
	ObjectAttributeMemory2 m_oam2;
	uint8 m_numSpritesToRender;
	bool m_renderSprite0;
	
	// Memory mapped registers
	typedef Memory<FixedSizeStorage<8>> PpuRegisterMemory; // $2000 - $2007
	PpuRegisterMemory m_ppuRegisters;

	// Memory-mapped registers
	Bitfield8* m_ppuControlReg1;	// $2000
	Bitfield8* m_ppuControlReg2;	// $2001
	Bitfield8* m_ppuStatusReg;		// $2002

	bool m_vramAndScrollFirstWrite;	// $2005/2006 flip-flop, "Loopy w"
	uint16 m_vramAddress;			// "Loopy v"
	uint16 m_tempVRamAddress;		// "Loopy t"
	uint8 m_fineX;					// Fine x scroll (3 bits), "Loopy x"
	uint8 m_vramBufferedValue;

	uint32 m_cycle;
	bool m_evenFrame;
	bool m_vblankFlagSetThisFrame;

	struct BgTileFetchData
	{
		uint8 bmpLow;
		uint8 bmpHigh;
		uint8 paletteHighBits;
	};
	BgTileFetchData m_bgTileFetchDataPipeline[2];

#if CONFIG_DEBUG
	struct BgTileFetchData_DEBUG
	{
		uint16 vramAddress;
		uint16 tileIndexAddress;
		uint16 attributeAddress;
		uint16 attributeShift;
		uint16 byte1Address;
	};
	BgTileFetchData_DEBUG m_bgTileFetchDataPipeline_DEBUG[2];
#endif

	struct SpriteFetchData
	{
		// Fetched from VRAM
		uint8 bmpLow;
		uint8 bmpHigh;
		
		// Copied from OAM2
		uint8 attributes;
		uint8 x;
	};
	SpriteFetchData m_spriteFetchData[8];
};
