#pragma once

#include "Base.h"

const size_t kScreenWidth = 256;
const size_t kScreenHeight = 240;

struct Color4
{
	uint32 argb;

	Color4() {}
	Color4(uint32 argb) : argb(argb) {}
	Color4(uint8 r, uint8 g, uint8 b, uint8 a) { SetRGBA(r, g, b, a); }

	void SetRGBA(uint8 r, uint8 g, uint8 b, uint8 a)
	{
		argb = (a << 24) | (r << 16) | (g << 8) | b;
	}

	uint8 A() const { return uint8((argb & 0xFF000000)>>24); }
	uint8 R() const { return uint8((argb & 0x00FF0000)>>16); }
	uint8 G() const { return uint8((argb & 0x0000FF00)>>8); }
	uint8 B() const { return uint8((argb & 0x000000FF)); }

	static Color4& Black()	{ static Color4 c(0x00, 0x00, 0x00, 0xFF); return c; }
	static Color4& White()	{ static Color4 c(0xFF, 0xFF, 0xFF, 0xFF); return c; }
	static Color4& Red()	{ static Color4 c(0xFF, 0x00, 0x00, 0xFF); return c; }
	static Color4& Green()	{ static Color4 c(0x00, 0xFF, 0x00, 0xFF); return c; }
	static Color4& Blue()	{ static Color4 c(0x00, 0x00, 0xFF, 0xFF); return c; }
};


class Renderer
{
public:
	Renderer();
	~Renderer();

	static void SetWindowTitle(const char* title);

	void Create();
	void Destroy();

	void Clear();
	void Render();

	void DrawPixel(int x, int y, const Color4& color);

private:
	struct PIMPL;
	PIMPL* m_impl;
};
