#include "Renderer.h"
#define SDL_MAIN_HANDLED // Don't use SDL's main impl
#include <SDL.h>

extern void DebugDrawAudio(SDL_Renderer* renderer);

namespace
{
	SDL_Window* g_mainWindow = nullptr;

	class BackBuffer
	{
	public:
		void Create(size_t width, size_t height, SDL_Renderer* renderer)
		{
			m_width = width;
			m_height = height;
			m_backbufferTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
			Lock();
		}

		void Clear(const Color4& color)
		{
			auto pCurrRow = reinterpret_cast<Uint32*>(m_backbuffer);
			for (int32 y = 0; y < m_height; ++y, pCurrRow += (m_pitch/4))
			{
				for (int32 x = 0; x < m_width; ++x)
					pCurrRow[x] = color.argb;
			}
		}

		void Flip(SDL_Renderer* renderer)
		{
			Unlock();
			SDL_RenderCopy(renderer, m_backbufferTexture, NULL, NULL);
			
			DebugDrawAudio(renderer);

			SDL_RenderPresent(renderer);
			Lock();
		}

		FORCEINLINE Uint32& operator()(int32 x, int32 y)
		{
			assert(x < m_width && y < m_height);
			return reinterpret_cast<Uint32&>(m_backbuffer[y * m_pitch + x * sizeof(Uint32)]);
		}

	private:
		void Lock()
		{
			SDL_LockTexture(m_backbufferTexture, NULL, (void**)(&m_backbuffer), &m_pitch);
		}
		void Unlock()
		{
			SDL_UnlockTexture(m_backbufferTexture);
		}

		SDL_Texture* m_backbufferTexture;
		Uint8* m_backbuffer;
		int32 m_width, m_height, m_pitch;
	};
}

struct Renderer::PIMPL
{
	PIMPL()
		: m_window(NULL)
		, m_renderer(NULL)
	{
	}

	SDL_Window* m_window;
	SDL_Renderer* m_renderer;
	BackBuffer m_backbuffer;
};

Renderer::Renderer()
	: m_impl(nullptr)
{
}

Renderer::~Renderer()
{
	Destroy();
}

void Renderer::SetWindowTitle(const char* title)
{
	if (g_mainWindow)
	{
		SDL_SetWindowTitle(g_mainWindow, title);
	}
}

void Renderer::Create(size_t screenWidth, size_t screenHeight)
{
	assert(!m_impl);
	m_impl = new PIMPL();

	if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
		FAIL("SDL_Init failed");

	const float windowScale = 3.0f;
	const size_t windowWidth = static_cast<size_t>(screenWidth * windowScale);
	const size_t windowHeight = static_cast<size_t>(screenHeight * windowScale);
	
	m_impl->m_window = SDL_CreateWindow("", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowWidth, windowHeight, SDL_WINDOW_SHOWN);
	if (!m_impl->m_window)
		FAIL("SDL_CreateWindow failed");

	m_impl->m_renderer = SDL_CreateRenderer(m_impl->m_window, -1, SDL_RENDERER_ACCELERATED);
	if (!m_impl->m_renderer)
		FAIL("SDL_CreateRenderer failed");

	m_impl->m_backbuffer.Create(screenWidth, screenHeight, m_impl->m_renderer);

	Clear();

	g_mainWindow = m_impl->m_window;
}

void Renderer::Destroy()
{
	if (m_impl)
	{
		SDL_DestroyRenderer(m_impl->m_renderer);
		SDL_DestroyWindow(m_impl->m_window);
		delete m_impl;
		m_impl = nullptr;
		g_mainWindow = nullptr;
	}
}

void Renderer::Clear(const Color4& color)
{
	m_impl->m_backbuffer.Clear(color);
}

void Renderer::DrawPixel(int32 x, int32 y, const Color4& color)
{
	m_impl->m_backbuffer(x, y) = color.argb;
}

void Renderer::Present()
{
	m_impl->m_backbuffer.Flip(m_impl->m_renderer);
}
