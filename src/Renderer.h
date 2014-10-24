#pragma once

#include "Base.h"

struct Color4
{
	Color4() {}

	Color4(uint32 rgba)
		: r(uint8((rgba & 0xFF000000)>>24))
		, g(uint8((rgba & 0x00FF0000)>>16))
		, b(uint8((rgba & 0x0000FF00)>>8))
		, a(uint8((rgba & 0x000000FF)))
	{
	}

	Color4(uint8 r, uint8 g, uint8 b, uint8 a)
		: r(r), g(g), b(b), a(a)
	{
	}

	uint8 r, g, b, a;

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

	void Create();
	void Destroy();

	void Clear();
	void Render();

	void DrawPixel(int x, int y, const Color4& color);

private:
	struct PIMPL;
	PIMPL* m_impl;
};
