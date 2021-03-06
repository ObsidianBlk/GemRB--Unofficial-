/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003-2012 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */

#include "SDL20Video.h"

#include "Interface.h"
#include "GUI/Console.h"
#include "GUI/GameControl.h" // for TargetMode (contextual information for touch inputs)

using namespace GemRB;

//touch gestures
#define MIN_GESTURE_DELTA_PIXELS 10
#define TOUCH_RC_NUM_TICKS 500

SDL20VideoDriver::SDL20VideoDriver(void)
{
	assert( core->NumFingScroll > 1 && core->NumFingKboard > 1 && core->NumFingInfo > 1);
	assert( core->NumFingScroll < 5 && core->NumFingKboard < 5 && core->NumFingInfo < 5);
	assert( core->NumFingScroll != core->NumFingKboard );

	renderer = NULL;
	window = NULL;
	videoPlayer = NULL;

	// touch input
	ignoreNextFingerUp = false;
	firstFingerDown = SDL_TouchFingerEvent();
	firstFingerDownTime = 0;
}

SDL20VideoDriver::~SDL20VideoDriver(void)
{
	SDL_DestroyTexture(videoPlayer);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}

int SDL20VideoDriver::CreateDisplay(int w, int h, int b, bool fs, const char* title)
{
	bpp=b;
	fullscreen=fs;
	Log(MESSAGE, "SDL 2 Driver", "Creating display");
	Uint32 winFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL;
#if TARGET_OS_IPHONE || ANDROID
	// this allows the user to flip the device upsidedown if they wish and have the game rotate.
	// it also for some unknown reason is required for retina displays
	winFlags |= SDL_WINDOW_RESIZABLE;
	// this hint is set in the wrapper for iPad at a higher priority. set it here for iPhone
	// don't know if Android makes use of this.
	SDL_SetHintWithPriority(SDL_HINT_ORIENTATIONS, "LandscapeRight LandscapeLeft", SDL_HINT_DEFAULT);
#endif
	if (fullscreen) {
		winFlags |= SDL_WINDOW_FULLSCREEN;
		//This is needed to remove the status bar on Android/iOS.
		//since we are in fullscreen this has no effect outside Android/iOS
		winFlags |= SDL_WINDOW_BORDERLESS;
	}
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, winFlags);
	renderer = SDL_CreateRenderer(window, -1, 0);

	if (renderer == NULL) {
		Log(ERROR, "SDL 2 Driver", "couldnt create renderer:%s", SDL_GetError());
		return GEM_ERROR;
	}

#if TARGET_OS_IPHONE
	// on iOS the window is always the entire screen
	// simulate a window by making the using passed in values for the backBuf size
	width = w;
	height = h;
#else
	SDL_GetWindowSize(window, &width, &height);
#endif
	Viewport.w = width;
	Viewport.h = height;

	Log(MESSAGE, "SDL 2 Driver", "Creating Main Surface...");
	SDL_Surface* tmp = SDL_CreateRGBSurface( 0, width, height,
											bpp, 0, 0, 0, 0 );

	backBuf = SDL_ConvertSurfaceFormat(tmp, SDL_GetWindowPixelFormat(window), 0);
	disp = backBuf;

	SDL_FreeSurface( tmp );
	return GEM_OK;
}

void SDL20VideoDriver::InitMovieScreen(int &w, int &h, bool yuv)
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	int winW, winH;
	SDL_GetWindowSize(window, &winW, &winH);

	if (videoPlayer) SDL_DestroyTexture(videoPlayer);
	if (yuv) {
		videoPlayer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, w, h);
	} else {
		videoPlayer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, winW, winH);
	}
	if (!videoPlayer) {
		Log(ERROR, "SDL 2 Driver", "Unable to create texture for video playback: %s", SDL_GetError());
	}
	w = winW;
	h = winH;
	//setting the subtitle region to the bottom 1/4th of the screen
	subtitleregion.w = w;
	subtitleregion.h = h/4;
	subtitleregion.x = 0;
	subtitleregion.y = h-h/4;
	//same for SDL
	subtitleregion_sdl.w = w;
	subtitleregion_sdl.h = h/4;
	subtitleregion_sdl.x = 0;
	subtitleregion_sdl.y = h-h/4;
}

void SDL20VideoDriver::showFrame(unsigned char* buf, unsigned int bufw,
							   unsigned int bufh, unsigned int sx, unsigned int sy, unsigned int w,
							   unsigned int h, unsigned int dstx, unsigned int dsty,
							   int g_truecolor, unsigned char *pal, ieDword titleref)
{
	assert( bufw == w && bufh == h );

	SDL_Rect srcRect = {sx, sy, w, h};
	SDL_Rect destRect = {dstx, dsty, w, h};

	Uint32 *dst;
	unsigned int row, col;
	void *pixels;
	int pitch;
	SDL_Color color = {0, 0, 0, 0};

	if(SDL_LockTexture(videoPlayer, NULL, &pixels, &pitch) != GEM_OK) {
		Log(ERROR, "SDL 2 driver", "Unable to lock video player: %s", SDL_GetError());
		return;
	}
	if (g_truecolor) {
		Uint16 *src = (Uint16*)buf;
		for (row = 0; row < bufh; ++row) {
			dst = (Uint32*)((Uint16*)pixels + row * pitch);
			for (col = 0; col < bufw; ++col) {
				color.r = ((*src & 0x7C00) >> 7) | ((*src & 0x7C00) >> 12);
				color.g = ((*src & 0x03E0) >> 2) | ((*src & 0x03E0) >> 8);
				color.b = ((*src & 0x001F) << 3) | ((*src & 0x001F) >> 2);
				color.unused = 0;
				// video player texture is of ARGB format. buf is RGB555
				*dst++ = (0xFF000000|(color.r << 16)|(color.g << 8)|(color.b));
				src++;
			}
		}
	} else {
		Uint8 *src = buf;
		SDL_Palette* palette;
		palette = SDL_AllocPalette(256);
		for (int i = 0; i < 256; i++) {
			palette->colors[i].r = ( *pal++ ) << 2;
			palette->colors[i].g = ( *pal++ ) << 2;
			palette->colors[i].b = ( *pal++ ) << 2;
			palette->colors[i].unused = 0;
		}
		for (row = 0; row < bufh; ++row) {
			dst = (Uint32*)((Uint8*)pixels + row * pitch);
			for (col = 0; col < bufw; ++col) {
				color = palette->colors[*src++];
				// video player texture is of ARGB format
				*dst++ = (0xFF000000|(color.r << 16)|(color.g << 8)|(color.b));
			}
		}
		SDL_FreePalette(palette);
	}
	SDL_UnlockTexture(videoPlayer);

	SDL_RenderFillRect(renderer, &subtitleregion_sdl);
	SDL_RenderCopy(renderer, videoPlayer, &srcRect, &destRect);

	if (titleref>0)
		DrawMovieSubtitle( titleref );

	SDL_RenderPresent(renderer);
}

void SDL20VideoDriver::showYUVFrame(unsigned char** buf, unsigned int *strides,
				  unsigned int /*bufw*/, unsigned int bufh,
				  unsigned int w, unsigned int h,
				  unsigned int dstx, unsigned int dsty,
				  ieDword /*titleref*/)
{
	SDL_Rect destRect;
	destRect.x = dstx;
	destRect.y = dsty;
	destRect.w = w;
	destRect.h = h;

	Uint8 *pixels;
	int pitch;

	if(SDL_LockTexture(videoPlayer, NULL, (void**)&pixels, &pitch) != GEM_OK) {
		Log(ERROR, "SDL 2 driver", "Unable to lock video player: %s", SDL_GetError());
		return;
	}
	pitch = w;
	if((unsigned int)pitch == strides[0]) {
		int size = pitch * bufh;
		memcpy(pixels, buf[0], size);
		memcpy(pixels + size, buf[2], size / 4);
		memcpy(pixels + size * 5 / 4, buf[1], size / 4);
	} else {
		unsigned char *Y,*U,*V,*iY,*iU,*iV;
		unsigned int i;
		Y = pixels;
		V = pixels + pitch * h;
		U = pixels + pitch * h * 5 / 4;

		iY = buf[0];
		iU = buf[1];
		iV = buf[2];

		for (i = 0; i < (h/2); i++) {
			memcpy(Y,iY,pitch);
			iY += strides[0];
			Y += pitch;

			memcpy(Y,iY,pitch);
			memcpy(U,iU,pitch / 2);
			memcpy(V,iV,pitch / 2);

			Y  += pitch;
			U  += pitch / 2;
			V  += pitch / 2;
			iY += strides[0];
			iU += strides[1];
			iV += strides[2];
		}
	}

	SDL_UnlockTexture(videoPlayer);
	SDL_RenderCopy(renderer, videoPlayer, NULL, &destRect);
	SDL_RenderPresent(renderer);
}

int SDL20VideoDriver::SwapBuffers(void)
{
	int ret = SDLVideoDriver::SwapBuffers();

	SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, backBuf);

	if (fadeColor.a) {
		SDL_Rect dst = {
			Viewport.x, Viewport.y, Viewport.w, Viewport.h
		};
		SDL_SetRenderDrawColor(renderer, fadeColor.r, fadeColor.g, fadeColor.b, fadeColor.a);
		SDL_RenderFillRect(renderer, &dst);
	}

	int w, h = 0;
	SDL_GetWindowSize(window, &w, &h);
	SDL_Rect dstRect = {0, 0, w, h};
#if TARGET_OS_IPHONE
	// our "window" is simulated
	if (!fullscreen) {
		dstRect.x = (w - backBuf->w) / 2;
		dstRect.y = (h - backBuf->h) / 2;
		dstRect.w = backBuf->w;
		dstRect.h = backBuf->h;
	}
#endif
	SDL_RenderCopy(renderer, tex, NULL, &dstRect);

	SDL_RenderPresent( renderer );
	SDL_DestroyTexture(tex);
	return ret;
}

int SDL20VideoDriver::PollEvents()
{
	if (firstFingerDownTime
		&& GetTickCount() - firstFingerDownTime >= TOUCH_RC_NUM_TICKS) {
		// enough time has passed to transform firstTouch into a right click event
		int x = firstFingerDown.x;
		int y = firstFingerDown.y;
		ProcessFirstTouch(GEM_MB_MENU);
		EvntManager->MouseUp( x, y, GEM_MB_MENU, GetModState(SDL_GetModState()));
		ignoreNextFingerUp = true;
	}

	return SDLVideoDriver::PollEvents();
}

void SDL20VideoDriver::ProcessFirstTouch( int mouseButton )
{
	if (!(MouseFlags & MOUSE_DISABLED) && firstFingerDown.fingerId) {
		lastMouseDownTime = EvntManager->GetRKDelay();
		if (lastMouseDownTime != (unsigned long) ~0) {
			lastMouseDownTime += lastMouseDownTime + lastTime;
		}

		// do an actual mouse move first! this is important for things such as ground piles to work!
		MouseMovement(firstFingerDown.x, firstFingerDown.y);

		if (CursorIndex != VID_CUR_DRAG)
			CursorIndex = VID_CUR_DOWN;
		// move cursor to ensure any referencing of the cursor is accurate
		CursorPos.x = firstFingerDown.x;
		CursorPos.y = firstFingerDown.y;

		// no need to scale these coordinates. they were scaled previously for us.
		EvntManager->MouseDown( firstFingerDown.x, firstFingerDown.y,
								mouseButton, GetModState(SDL_GetModState()) );

		firstFingerDown = SDL_TouchFingerEvent();
		ignoreNextFingerUp = false;
		firstFingerDownTime = 0;
	}
}

int SDL20VideoDriver::ProcessEvent(const SDL_Event & event)
{
	Control* focusCtrl = NULL; //used for contextual touch events.
	/*
	 the digitizer could have a higher resolution then the screen.
	 we need to get the scale factor to convert digitizer touch coordinates to screen pixel coordinates
	 */
	SDL_Touch *state = SDL_GetTouch(event.tfinger.touchId);
	float xScaleFactor, yScaleFactor = 1.0;
	int xOffset, yOffset = 0;
	int numFingers = 0;
	if(state){
		focusCtrl = EvntManager->GetMouseFocusedControl();
		numFingers = state->num_fingers;
#if TARGET_OS_IPHONE
		if (fullscreen) {
			// simulated window adjustment
			xScaleFactor = (state->xres / backBuf->w);
			yScaleFactor = (state->yres / backBuf->h);
		} else {
#endif
		int w, h;
		SDL_GetWindowSize(window, &w, &h);
		xScaleFactor = (state->xres / w);
		yScaleFactor = (state->yres / h);
#if TARGET_OS_IPHONE
			// now we need to offset the x/y coordinates
			xOffset = ((w - backBuf->w) / 2) * -1;
			yOffset = ((h - backBuf->h) / 2) * -1;
		}
#endif
	}

	bool ConsolePopped = core->ConsolePopped;

	switch (event.type) {
		//!!!!!!!!!!!!
		// !!!: currently SDL 2.0 brodcasts both mouse and touch events on touch for iOS
		//  there is no API to prevent the mouse events so we will be ignoring mouse events on iOS
		//  there is no way currently to use a mouse anyay.
		//  watch future SDL releases to see if/when disabling mouse events from the touchscreen is available
		//!!!!!!!!!!!!
#if TARGET_OS_IPHONE
		// don't include SDL_MOUSEWHEEL here
		// note that other platforms (non-iOS) needn't implement this
		// and should let SDLVideo handle mouse events
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			break;
#endif
		// For swipes only. gestures requireing pinch or rotate need to use SDL_MULTIGESTURE or SDL_DOLLARGESTURE
		case SDL_FINGERMOTION:
			ignoreNextFingerUp = true;

			if (numFingers == core->NumFingScroll
				|| (numFingers != core->NumFingKboard && (focusCtrl && focusCtrl->ControlType == IE_GUI_TEXTAREA))) {
				//any # of fingers != NumFingKBoard will scroll a text area
				if (focusCtrl && focusCtrl->ControlType == IE_GUI_TEXTAREA) {
					// if we are scrolling a text area we dont want the keyboard in the way
					HideSoftKeyboard();
				} else {
					// ensure the control we touched becomes focused before attempting to scroll it.
					ProcessFirstTouch(GEM_MB_ACTION);
				}
				// invert the coordinates such that dragging down scrolls up etc.
				int scrollX = (event.tfinger.dx / xScaleFactor) * -1;
				int scrollY = (event.tfinger.dy / yScaleFactor) * -1;

				EvntManager->MouseWheelScroll( scrollX, scrollY );
			} else if (numFingers == core->NumFingKboard) {
				if ((event.tfinger.dy / yScaleFactor) * -1 >= MIN_GESTURE_DELTA_PIXELS){
					// if the keyboard is already up interpret this gesture as console pop
					if( SDL_IsScreenKeyboardShown(window) && !ConsolePopped ) core->PopupConsole();
					else ShowSoftKeyboard();
				} else if((event.tfinger.dy / yScaleFactor) * -1 <= -MIN_GESTURE_DELTA_PIXELS){
					HideSoftKeyboard();
				}
			} else if (numFingers == 1) { //click and drag
				ProcessFirstTouch(GEM_MB_ACTION);
				ignoreNextFingerUp = false;
				// standard mouse movement
				MouseMovement((event.tfinger.x / xScaleFactor) + xOffset, (event.tfinger.y / yScaleFactor) + yOffset);
			}
			break;
		case SDL_FINGERDOWN:
			if (numFingers == 1) {
				// do not send a mouseDown event. we delay firstTouch until we know more about the context.
				firstFingerDown = event.tfinger;
				firstFingerDownTime = GetTickCount();
				firstFingerDown.x /= xScaleFactor;
				firstFingerDown.x += xOffset;
				firstFingerDown.y /= yScaleFactor;
				firstFingerDown.y += yOffset;
			} else if (EvntManager && numFingers == core->NumFingInfo) {
				ProcessFirstTouch(GEM_MB_ACTION);
				EvntManager->OnSpecialKeyPress( GEM_TAB );
				EvntManager->OnSpecialKeyPress( GEM_ALT );
			}
			break;
		case SDL_FINGERUP:
			{
				// we need to get mouseButton before calling ProcessFirstTouch
				int mouseButton = (firstFingerDown.fingerId) ? GEM_MB_ACTION : GEM_MB_MENU;
				ProcessFirstTouch(GEM_MB_ACTION);
				if (numFingers == 0) { // this event was the last finger that was in contact
					if (!ignoreNextFingerUp) {
						if (CursorIndex != VID_CUR_DRAG)
							CursorIndex = VID_CUR_UP;
						// move cursor to ensure any referencing of the cursor is accurate
						CursorPos.x = event.button.x;
						CursorPos.y = event.button.y;

						EvntManager->MouseUp((event.tfinger.x / xScaleFactor) + xOffset,
											 (event.tfinger.y / yScaleFactor) + yOffset,
											 mouseButton, GetModState(SDL_GetModState()) );
					}
				}
				if (numFingers != core->NumFingInfo) {
					// FIXME: this is "releasing" the ALT key even when it hadn't been previously "pushed"
					// this isn't causing a problem currently
					EvntManager->KeyRelease( GEM_ALT, 0 );
				}
			}
			break;
		case SDL_MULTIGESTURE:// use this for pinch or rotate gestures. see also SDL_DOLLARGESTURE
			// purposely ignore processing first touch here. I think users ould find it annoying
			// to attempt a gesture and accidently command a party movement etc
			if (firstFingerDown.fingerId && numFingers == 2
				&& focusCtrl && focusCtrl->ControlType == IE_GUI_GAMECONTROL) {
				/* formation rotation gesture:
				 first touch with a single finger to obtain the pivot
				 then touch and drag with a second finger (while maintaining contact with first)
				 to move the application point
				 */
				GameControl* gc = core->GetGameControl();
				if (gc && gc->GetTargetMode() == TARGET_MODE_NONE) {
					ProcessFirstTouch(GEM_MB_MENU);
					SDL_Finger* secondFinger = state->fingers[1];
					gc->OnMouseOver(secondFinger->x + Viewport.x, secondFinger->y + Viewport.y);
				}
			} else {
				ProcessFirstTouch(GEM_MB_ACTION);
			}
			break;
		case SDL_MOUSEWHEEL:
			/*
			 TODO: need a preference for inverting these
			 */
			short scrollX;
			scrollX= event.wheel.x * -1;
			short scrollY;
			scrollY= event.wheel.y * -1;
			EvntManager->MouseWheelScroll( scrollX, scrollY );
			break;
		/* not user input events */
		case SDL_TEXTINPUT:
			for (size_t i=0; i < strlen(event.text.text); i++) {
				if (core->ConsolePopped)
					core->console->OnKeyPress( event.text.text[i], GetModState(event.key.keysym.mod));
				else
					EvntManager->KeyPress( event.text.text[i], GetModState(event.key.keysym.mod));
			}
			break;
		/* not user input events */
		case SDL_WINDOWEVENT://SDL 1.2
			switch (event.window.event) {
				case SDL_WINDOWEVENT_MINIMIZED://SDL 1.3
					// We pause the game and audio when the window is minimized.
					// on iOS/Android this happens when leaving the application or when play is interrupted (ex phone call)
					// if win/mac/linux has a problem with this behavior we can work something out.
					core->GetAudioDrv()->Pause();//this is for ANDROID mostly
					core->SetPause(PAUSE_ON);
					break;
				case SDL_WINDOWEVENT_RESTORED: //SDL 1.3
					/*
					 reset all input variables as if no events have happened yet
					 restoring from "minimized state" should be a clean slate.
					 */
					ignoreNextFingerUp = false;
					firstFingerDown = SDL_TouchFingerEvent();
					// should we reset the lastMouseTime vars?
#if TARGET_OS_IPHONE
					// FIXME:
					// sleep for a short while to avoid some unknown Apple threading issue with OpenAL threads being suspended
					// even using Apple examples of how to properly suspend an OpenAL context and resume on iOS are falling flat
					// it could be this bug affects only the simulator.
					sleep(1);
#endif
					core->GetAudioDrv()->Resume();//this is for ANDROID mostly
					break;
					/*
				case SDL_WINDOWEVENT_RESIZED: //SDL 1.2
					// this event exists in SDL 1.2, but this handler is only getting compiled under 1.3+
					Log(WARNING, "SDL 2 Driver",  "Window resized so your window surface is now invalid.");
					break;
					 */
			}
			break;
		default:
			return SDLVideoDriver::ProcessEvent(event);
	}
	return GEM_OK;
}

/*
 This method is intended for devices with no physical keyboard or with an optional soft keyboard (iOS/Android)
 */
void SDL20VideoDriver::HideSoftKeyboard()
{
	if(core->UseSoftKeyboard){
		SDL_HideScreenKeyboard(window);
		if(core->ConsolePopped) core->PopupConsole();
	}
}

/*
 This method is intended for devices with no physical keyboard or with an optional soft keyboard (iOS/Android)
 */
void SDL20VideoDriver::ShowSoftKeyboard()
{
	if(core->UseSoftKeyboard){
		SDL_ShowScreenKeyboard(window);
	}
}

/* no idea how elaborate this should be*/
void SDL20VideoDriver::MoveMouse(unsigned int x, unsigned int y)
{
	SDL_WarpMouseInWindow(window, x, y);
}

void SDL20VideoDriver::SetGamma(int brightness, int /*contrast*/)
{
	SDL_SetWindowBrightness(window, (float)(brightness/40.0));
}

bool SDL20VideoDriver::SetFullscreenMode(bool set)
{
	if (SDL_SetWindowFullscreen(window, (SDL_bool)set) == GEM_OK) {
		fullscreen = set;
		return true;
	}
	return false;
}

bool SDL20VideoDriver::ToggleGrabInput()
{
	bool isGrabbed = SDL_GetWindowGrab(window);
	SDL_SetWindowGrab(window, (SDL_bool)!isGrabbed);
	return (isGrabbed != SDL_GetWindowGrab(window));
}

// Private methods

bool SDL20VideoDriver::SetSurfacePalette(SDL_Surface* surface, SDL_Color* colors, int ncolors)
{
	return (SDL_SetPaletteColors((surface)->format->palette, colors, 0, ncolors) == 0);
}

bool SDL20VideoDriver::SetSurfaceAlpha(SDL_Surface* surface, unsigned short alpha)
{
	return (SDL_SetSurfaceAlphaMod(surface, alpha) == 0);
}

#include "plugindef.h"

GEMRB_PLUGIN(0xDBAAB50, "SDL Video Driver")
PLUGIN_DRIVER(SDL20VideoDriver, "sdl")
END_PLUGIN()
