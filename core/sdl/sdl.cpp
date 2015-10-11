#if defined(USE_SDL)
#include "types.h"
#include "cfg/cfg.h"
#include "linux-dist/main.h"
#include "sdl/sdl.h"
#ifdef GLES
	#include <EGL/egl.h>
#else
	#ifndef USE_SDL2
		#error "Our SDL1.2 implementation only supports GLES. You need SDL2 for OpenGL 3 support!"
	#endif
	#include "khronos/GL3/gl3w.h"
#endif

#ifdef USE_SDL2
	static SDL_Window* window = NULL;
	static SDL_GLContext glcontext;
#else
	SDL_Surface *screen = NULL;
#endif

#ifdef TARGET_PANDORA
	#define WINDOW_WIDTH  800
#else
	#define WINDOW_WIDTH  640
#endif
#define WINDOW_HEIGHT  480

static SDL_Joystick *JoySDL = 0;

extern bool FrameSkipping;
extern void dc_term();
extern bool KillTex;

#ifdef TARGET_PANDORA
	extern char OSD_Info[128];
	extern int OSD_Delay;
	extern char OSD_Counters[256];
	extern int OSD_Counter;
#endif

#define SDL_MAP_SIZE 32

const u32 sdl_map_btn_usb[SDL_MAP_SIZE] =
	{ DC_BTN_Y, DC_BTN_B, DC_BTN_A, DC_BTN_X, 0, 0, 0, 0, 0, DC_BTN_START };

const u32 sdl_map_axis_usb[SDL_MAP_SIZE] =
	{ DC_AXIS_X, DC_AXIS_Y, 0, 0, 0, 0, 0, 0, 0, 0 };

const u32 sdl_map_btn_xbox360[SDL_MAP_SIZE] =
	{ DC_BTN_A, DC_BTN_B, DC_BTN_X, DC_BTN_Y, 0, 0, 0, DC_BTN_START, 0, 0 };

const u32 sdl_map_axis_xbox360[SDL_MAP_SIZE] =
	{ DC_AXIS_X, DC_AXIS_Y, DC_AXIS_LT, 0, 0, DC_AXIS_RT, DC_DPAD_LEFT, DC_DPAD_UP, 0, 0 };

const u32* sdl_map_btn  = sdl_map_btn_usb;
const u32* sdl_map_axis = sdl_map_axis_usb;

#ifdef TARGET_PANDORA
u32  JSensitivity[256];  // To have less sensitive value on nubs
#endif

void input_sdl_init()
{
	if (SDL_WasInit(SDL_INIT_JOYSTICK) == 0)
	{
		if(SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
		{
			die("error initializing SDL Joystick subsystem");
		}
	}
	// Open joystick device
	int numjoys = SDL_NumJoysticks();
	printf("Number of Joysticks found = %i\n", numjoys);
	if (numjoys > 0)
	{
		JoySDL = SDL_JoystickOpen(0);
	}
	
	printf("Joystick opened\n");  
	
	if(JoySDL)
	{
		int AxisCount,ButtonCount;
		const char* Name;

		AxisCount   = 0;
		ButtonCount = 0;
		//Name[0]     = '\0';

		AxisCount = SDL_JoystickNumAxes(JoySDL);
		ButtonCount = SDL_JoystickNumButtons(JoySDL);
		#ifdef USE_SDL2
			Name = SDL_JoystickName(JoySDL);
		#else
			Name = SDL_JoystickName(0);
		#endif

		printf("SDK: Found '%s' joystick with %d axes and %d buttons\n", Name, AxisCount, ButtonCount);

		if (Name != NULL && strcmp(Name,"Microsoft X-Box 360 pad")==0)
		{
			sdl_map_btn  = sdl_map_btn_xbox360;
			sdl_map_axis = sdl_map_axis_xbox360;
			printf("Using Xbox 360 map\n");
		}
	}
	else
	{
		printf("SDK: No Joystick Found\n");
	}

	#ifdef TARGET_PANDORA
		float v;
		int j;
		for (int i=0; i<128; i++)
		{
			v = ((float)i)/127.0f;
			v = (v+v*v)/2.0f;
			j = (int)(v*127.0f);
			if (j > 127)
			{
				j = 127;
			}
			JSensitivity[128-i] = -j;
			JSensitivity[128+i] = j;
		}
	#endif
	
	#ifndef USE_SDL2
		SDL_ShowCursor(0);

		if (SDL_WM_GrabInput( SDL_GRAB_ON ) != SDL_GRAB_ON)
		{
			printf("SDL: Error while grabbing mouse\n");
		}
	#else
		SDL_SetRelativeMouseMode(SDL_TRUE);
	#endif
}

void input_sdl_handle(u32 port)
{
	static int keys[13];
	static int mouse_use = 0;
	SDL_Event event;
	int k, value;
	int xx, yy;
	const char *num_mode[] = {"Off", "Up/Down => RT/LT", "Left/Right => LT/RT", "U/D/L/R => A/B/X/Y"};
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
			case SDL_QUIT:
							 die("death by SDL request");
							 break;
			case SDL_KEYDOWN:
			case SDL_KEYUP:
				k = event.key.keysym.sym;
				value = (event.type == SDL_KEYDOWN) ? 1 : 0;
				 //printf("type %i key %i \n", event.type, k);
				switch (k) {
					//TODO: Better keymaps for non-pandora platforms
					case SDLK_SPACE:    keys[0] = value; break;
					case SDLK_UP:       keys[1] = value; break;
					case SDLK_DOWN:     keys[2] = value; break;
					case SDLK_LEFT:     keys[3] = value; break;
					case SDLK_RIGHT:    keys[4] = value; break;
					case SDLK_PAGEUP:   keys[5] = value; break;
					case SDLK_PAGEDOWN: keys[6] = value; break;
					case SDLK_END:      keys[7] = value; break;
					case SDLK_HOME:     keys[8] = value; break;
					case SDLK_MENU:
					case SDLK_ESCAPE:   keys[9]  = value; break;
					case SDLK_RSHIFT:   keys[11] = value; break;
					case SDLK_RCTRL:    keys[10] = value; break;
					case SDLK_LALT:     keys[12] = value; break;
					case SDLK_k:        KillTex = true; break;
				#if defined(TARGET_PANDORA)
					case SDLK_n:
						if (value)
						{
							mouse_use = (mouse_use + 1) % 4;
							snprintf(OSD_Info, 128, "Right Nub mode: %s\n", num_mode[mouse_use]);
							OSD_Delay=300;
						}
						break;  
					case SDLK_s:
						if (value)
						{
							settings.aica.NoSound = !settings.aica.NoSound;
							snprintf(OSD_Info, 128, "Sound %s\n", (settings.aica.NoSound) ? "Off" : "On");
							OSD_Delay=300;
						}
						break;
					case SDLK_f:
						if (value)
						{
							FrameSkipping = !FrameSkipping;
							snprintf(OSD_Info, 128, "FrameSkipping %s\n", (FrameSkipping) ? "On" : "Off");
							OSD_Delay = 300;
						}
						break;  
					case SDLK_c:
						if (value)
						{
							OSD_Counter = 1 - OSD_Counter;
						}
						break;
				#endif
				}
				break;
			case SDL_JOYBUTTONDOWN:
			case SDL_JOYBUTTONUP:
				value = (event.type == SDL_JOYBUTTONDOWN) ? 1 : 0;
				k = event.jbutton.button;
				{
					u32 mt = sdl_map_btn[k] >> 16;
					u32 mo = sdl_map_btn[k] & 0xFFFF;
					
					// printf("BUTTON %d,%d\n",JE.number,JE.value);
					
					if (mt == 0)
					{
						// printf("Mapped to %d\n",mo);
						if (value)
						kcode[port] &= ~mo;
						else
						kcode[port] |= mo;
					}
					else if (mt == 1)
					{
						// printf("Mapped to %d %d\n",mo,JE.value?255:0);
						if (mo == 0)
						{
							lt[port] = value ? 255 : 0;
						}
						else if (mo == 1)
						{
							rt[port] = value ? 255 : 0;
						}
					}
					
				}
				break;
			case SDL_JOYAXISMOTION:
				k = event.jaxis.axis;
				value = event.jaxis.value;
				{
					u32 mt = sdl_map_axis[k] >> 16;
					u32 mo = sdl_map_axis[k] & 0xFFFF;
					
					//printf("AXIS %d,%d\n",JE.number,JE.value);
					s8 v=(s8)(value/256); //-127 ... + 127 range
					#ifdef TARGET_PANDORA
						v = JSensitivity[128+v];
					#endif
					
					if (mt == 0)
					{
						kcode[port] |= mo;
						kcode[port] |= mo*2;
						if (v < -64)
						{
							kcode[port] &= ~mo;
						}
						else if (v > 64)
						{
							kcode[port] &= ~(mo*2);
						}
						
						// printf("Mapped to %d %d %d\n",mo,kcode[port]&mo,kcode[port]&(mo*2));
					}
					else if (mt == 1)
					{
						if (v >= 0) v++;  //up to 255
						
						//   printf("AXIS %d,%d Mapped to %d %d %d\n",JE.number,JE.value,mo,v,v+127);
						
						if (mo == 0)
						{
							lt[port] = v + 127;
						}
						else if (mo == 1)
						{
							rt[port] = v + 127;
						}
					}
					else if (mt == 2)
					{
						//  printf("AXIS %d,%d Mapped to %d %d [%d]",JE.number,JE.value,mo,v);
						if (mo == 0)
						{
							joyx[port] = v;
						}
						else if (mo==1)
						{
							joyy[port] = v;
						}
					}
				}
				break;
			case SDL_MOUSEMOTION:
					xx = event.motion.xrel;
					yy = event.motion.yrel;
					
					// some caping and dead zone...
					if (abs(xx) < 4)
					{
						xx = 0;
					}
					if (abs(yy) < 4)
					{
						yy = 0;
					}
					
					xx = xx * 255 / 20;
					yy = yy * 255 / 20;
					
					if (xx > 255)
					{
						xx = 255;
					}
					else if (xx<-255)
					{
						xx = -255;
					}
					
					if (yy > 255)
					{
						yy = 255;
					}
					else if (yy<-255)
					{
						yy = -255;
					}
					
					//if (abs(xx)>0 || abs(yy)>0) printf("mouse %i, %i\n", xx, yy);
					switch (mouse_use)
					{
						case 0:  // nothing
							break;
						case 1:  // Up=RT, Down=LT
							if (yy<0)
							{
								rt[port] = -yy;
							}
							else if (yy>0)
							{
								lt[port] = yy;
							}
							break;
						case 2:  // Left=LT, Right=RT
							if (xx < 0)
							{
								lt[port] = -xx;
							}
							else if (xx > 0)
							{
								rt[port] = xx;
							}
							break;
						case 3:  // Nub = ABXY
							if (xx < -127)
							{
								kcode[port] &= ~DC_BTN_X;
							}
							else if (xx > 127)
							{
								kcode[port] &= ~DC_BTN_B;
							}
							if (yy < -127)
							{
								kcode[port] &= ~DC_BTN_Y;
							}
							else if (yy > 127)
							{
								kcode[port] &= ~DC_BTN_A;
							}
							break;
					}
				break;
		}
	}
			
	if (keys[0]) { kcode[port] &= ~DC_BTN_C; }
	if (keys[6]) { kcode[port] &= ~DC_BTN_A; }
	if (keys[7]) { kcode[port] &= ~DC_BTN_B; }
	if (keys[5]) { kcode[port] &= ~DC_BTN_Y; }
	if (keys[8]) { kcode[port] &= ~DC_BTN_X; }
	if (keys[1]) { kcode[port] &= ~DC_DPAD_UP; }
	if (keys[2]) { kcode[port] &= ~DC_DPAD_DOWN; }
	if (keys[3]) { kcode[port] &= ~DC_DPAD_LEFT; }
	if (keys[4]) { kcode[port] &= ~DC_DPAD_RIGHT; }
	if (keys[12]){ kcode[port] &= ~DC_BTN_START; }
	if (keys[9])
	{ 
		dc_term();
	
		// is there a proper way to exit? dc_term() doesn't end the dc_run() loop it seems
		die("death by escape key"); 
	} 
	if (keys[10])
	{
		rt[port] = 255;
	}
	if (keys[11])
	{
		lt[port] = 255;
	}
}

void sdl_window_set_text(const char* text)
{
	#ifdef TARGET_PANDORA
		strncpy(OSD_Counters, text, 256);
	#else
		#ifdef USE_SDL2
			if(window)
			{
				SDL_SetWindowTitle(window, text);    // *TODO*  Set Icon also...
			}
		#else
			SDL_WM_SetCaption(text, NULL);
		#endif
	#endif
}

int ndcid = 0;

/* Workaround for using OpenGL with an SDL_Window created from HWND
 *
 * As of SDL 2.0.3, there seems to be no way to create an OpenGL-capable
 * SDL window from a HWND/XWindowID. The SDL_WINDOW_OPENGL flags is not set
 * when using SDL_CreateWindowFrom and the flags member is private and cannot
 * be changed.
 *
 * To circumvent that, we define our own SDL_Window struct where the flags
 * member is not private and add the SDL_WINDOW_OPENGL flags manually
 * afterwards.
 *
 * Credit goes to user "perilsensitive" from the libsdl forum for this
 * great idea: https://forums.libsdl.org/viewtopic.php?p=44620#44620
 */

struct SDL_Window
{
	const void *magic;
	Uint32 id;
	char *title;
	SDL_Surface *icon;
	int x, y;
	int w, h;
	int min_w, min_h;
	int max_w, max_h;
	Uint32 flags;
};
typedef struct SDL_Window SDL_Window;

static void* sdl_hwnd = NULL;

void sdl_set_hwnd(void* hwnd)
{
	sdl_hwnd = hwnd;
}

void sdl_window_create()
{
	if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
	{
		if(SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
		{
			die("error initializing SDL Joystick subsystem");
		}
	}

	int window_width  = cfgLoadInt("x11","width", WINDOW_WIDTH);
	int window_height = cfgLoadInt("x11","height", WINDOW_HEIGHT);

	#ifdef TARGET_PANDORA
		int flags = SDL_FULLSCREEN;
	#else
		int flags = SDL_SWSURFACE;
	#endif

	#if !defined(GLES) && defined(USE_SDL2)
		flags |= SDL_WINDOW_OPENGL;

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
		SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

		if (sdl_hwnd != NULL)
		{
			printf("SDL: Trying to create a Window from %d\n", sdl_hwnd);
			window = SDL_CreateWindowFrom((void*)sdl_hwnd);
			window->flags |= SDL_WINDOW_OPENGL;
			SDL_GL_LoadLibrary(NULL);
			printf("SDL: Window from %d created!\n", sdl_hwnd);
		}
		else
		{
			window = SDL_CreateWindow("Reicast Emulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,	window_width, window_height, flags);
		}

		if (!window)
		{
			die("error creating SDL window");
		}

		glcontext = SDL_GL_CreateContext(window);
		if (!glcontext)
		{
			die("Error creating SDL GL context");
		}
		SDL_GL_MakeCurrent(window, NULL);
	#else
		screen = SDL_SetVideoMode(window_width, window_height, 0, flags);
		if (!screen)
		{
			die("error creating SDL screen");
		}

		x11_disp = EGL_DEFAULT_DISPLAY;
	#endif

	printf("Created SDL Window (%ix%i) and GL Context successfully\n", window_width, window_height);
}
#endif

#if !defined(GLES) && defined(USE_SDL2)
	extern int screen_width, screen_height;

	bool gl_init(void* wind, void* disp)
	{
		SDL_GL_MakeCurrent(window, glcontext);
		return gl3wInit() != -1 && gl3wIsSupported(3, 1);
	}

	void gl_swap()
	{
		SDL_GL_SwapWindow(window);

		/* Check if drawable has been resized */
		int new_width, new_height;
		SDL_GL_GetDrawableSize(window, &new_width, &new_height);

		if (new_width != screen_width || new_height != screen_height)
		{
			screen_width = new_width;
			screen_height = new_height;
		}
	}

	void gl_term()
	{
		SDL_GL_DeleteContext(glcontext);
	}
#endif