#include "stdafx.h"
#include <Windows.h>
#include <iostream>
 
COLORREF rgb_to_bgr(DWORD aRGB)
// Fancier methods seem prone to problems due to byte alignment or compiler issues.
{
	return RGB(GetBValue(aRGB), GetGValue(aRGB), GetRValue(aRGB));
}
 
LPCOLORREF getbits(HBITMAP ahImage, HDC hdc, LONG &aWidth, LONG &aHeight, bool &aIs16Bit, int aMinColorDepth = 8)
// Helper function used by PixelSearch below.
// Returns an array of pixels to the caller, which it must free when done.  Returns NULL on failure,
// in which case the contents of the output parameters is indeterminate.
{
	HDC tdc = CreateCompatibleDC(hdc);
	if (!tdc)
		return NULL;
 
	// From this point on, "goto end" will assume tdc is non-NULL, but that the below
	// might still be NULL.  Therefore, all of the following must be initialized so that the "end"
	// label can detect them:
	HGDIOBJ tdc_orig_select = NULL;
	LPCOLORREF image_pixel = NULL;
	bool success = false;
 
	// Confirmed:
	// Needs extra memory to prevent buffer overflow due to: "A bottom-up DIB is specified by setting
	// the height to a positive number, while a top-down DIB is specified by setting the height to a
	// negative number. THE BITMAP COLOR TABLE WILL BE APPENDED to the BITMAPINFO structure."
	// Maybe this applies only to negative height, in which case the second call to GetDIBits()
	// below uses one.
	struct BITMAPINFO3
	{
		BITMAPINFOHEADER    bmiHeader;
		RGBQUAD             bmiColors[260];  // v1.0.40.10: 260 vs. 3 to allow room for color table when color depth is 8-bit or less.
	} bmi;
 
	// Initialize variables so VS2017 wont complain
	int image_pixel_count;
	bool is_8bit;
 
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biBitCount = 0; // i.e. "query bitmap attributes" only.
	if (!GetDIBits(tdc, ahImage, 0, 0, NULL, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS)
		|| bmi.bmiHeader.biBitCount < aMinColorDepth) // Relies on short-circuit boolean order.
		goto end;
 
	// Set output parameters for caller:
	aIs16Bit = (bmi.bmiHeader.biBitCount == 16);
	aWidth = bmi.bmiHeader.biWidth;
	aHeight = bmi.bmiHeader.biHeight;
 
	image_pixel_count = aWidth * aHeight;
	if (!(image_pixel = (LPCOLORREF)malloc(image_pixel_count * sizeof(COLORREF))))
		goto end;
 
	// v1.0.40.10: To preserve compatibility with callers who check for transparency in icons, don't do any
	// of the extra color table handling for 1-bpp images.  Update: For code simplification, support only
	// 8-bpp images.  If ever support lower color depths, use something like "bmi.bmiHeader.biBitCount > 1
	// && bmi.bmiHeader.biBitCount < 9";
	is_8bit = (bmi.bmiHeader.biBitCount == 8);
	if (!is_8bit)
		bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biHeight = -bmi.bmiHeader.biHeight; // Storing a negative inside the bmiHeader struct is a signal for GetDIBits().
 
													  // Must be done only after GetDIBits() because: "The bitmap identified by the hbmp parameter
													  // must not be selected into a device context when the application calls GetDIBits()."
													  // (Although testing shows it works anyway, perhaps because GetDIBits() above is being
													  // called in its informational mode only).
													  // Note that this seems to return NULL sometimes even though everything still works.
													  // Perhaps that is normal.
	tdc_orig_select = SelectObject(tdc, ahImage); // Returns NULL when we're called the second time?
 
												  // Apparently there is no need to specify DIB_PAL_COLORS below when color depth is 8-bit because
												  // DIB_RGB_COLORS also retrieves the color indices.
	if (!(GetDIBits(tdc, ahImage, 0, aHeight, image_pixel, (LPBITMAPINFO)&bmi, DIB_RGB_COLORS)))
		goto end;
 
	if (is_8bit) // This section added in v1.0.40.10.
	{
		// Convert the color indices to RGB colors by going through the array in reverse order.
		// Reverse order allows an in-place conversion of each 8-bit color index to its corresponding
		// 32-bit RGB color.
		LPDWORD palette = (LPDWORD)_alloca(256 * sizeof(PALETTEENTRY));
		GetSystemPaletteEntries(tdc, 0, 256, (LPPALETTEENTRY)palette); // Even if failure can realistically happen, consequences of using uninitialized palette seem acceptable.
																	   // Above: GetSystemPaletteEntries() is the only approach that provided the correct palette.
																	   // The following other approaches didn't give the right one:
																	   // GetDIBits(): The palette it stores in bmi.bmiColors seems completely wrong.
																	   // GetPaletteEntries()+GetCurrentObject(hdc, OBJ_PAL): Returned only 20 entries rather than the expected 256.
																	   // GetDIBColorTable(): I think same as above or maybe it returns 0.
 
																	   // The following section is necessary because apparently each new row in the region starts on
																	   // a DWORD boundary.  So if the number of pixels in each row isn't an exact multiple of 4, there
																	   // are between 1 and 3 zero-bytes at the end of each row.
		int remainder = aWidth % 4;
		int empty_bytes_at_end_of_each_row = remainder ? (4 - remainder) : 0;
 
		// Start at the last RGB slot and the last color index slot:
		BYTE *byte = (BYTE *)image_pixel + image_pixel_count - 1 + (aHeight * empty_bytes_at_end_of_each_row); // Pointer to 8-bit color indices.
		DWORD *pixel = image_pixel + image_pixel_count - 1; // Pointer to 32-bit RGB entries.
 
		int row, col;
		for (row = 0; row < aHeight; ++row) // For each row.
		{
			byte -= empty_bytes_at_end_of_each_row;
			for (col = 0; col < aWidth; ++col) // For each column.
				*pixel-- = rgb_to_bgr(palette[*byte--]); // Caller always wants RGB vs. BGR format.
		}
	}
 
	// Since above didn't "goto end", indicate success:
	success = true;
 
end:
	if (tdc_orig_select) // i.e. the original call to SelectObject() didn't fail.
		SelectObject(tdc, tdc_orig_select); // Probably necessary to prevent memory leak.
	DeleteDC(tdc);
	if (!success && image_pixel)
	{
		free(image_pixel);
		image_pixel = NULL;
	}
	return image_pixel;
}
 
// { -1, -1 } = Not Found
// { -2, -2 } = Error
POINT PixelSearch(int aLeft, int aTop, int aRight, int aBottom, COLORREF aColorRGB, int aVariation)
// Caller has ensured that aColor is in BGR format unless caller passed true for aUseRGB, in which case
// it's in RGB format.
// Author: The fast-mode PixelSearch was created by Aurelian Maga.
{
	// For maintainability, get options and RGB/BGR conversion out of the way early.
	COLORREF aColorBGR = rgb_to_bgr(aColorRGB); // rgb_to_bgr() also converts in the reverse direction, i.e. bgr_to_rgb().
 
												// Many of the following sections are similar to those in ImageSearch(), so they should be
												// maintained together.
 
	if (aVariation < 0)
		aVariation = 0;
	if (aVariation > 255)
		aVariation = 255;
 
	// Allow colors to vary within the spectrum of intensity, rather than having them
	// wrap around (which doesn't seem to make much sense).  For example, if the user specified
	// a variation of 5, but the red component of aColorBGR is only 0x01, we don't want red_low to go
	// below zero, which would cause it to wrap around to a very intense red color:
	COLORREF pixel; // Used much further down.
	BYTE red, green, blue; // Used much further down.
	BYTE search_red, search_green, search_blue;
	BYTE red_low, green_low, blue_low, red_high, green_high, blue_high;
	if (aVariation > 0)
	{
		search_red = GetRValue(aColorBGR);
		search_green = GetGValue(aColorBGR);
		search_blue = GetBValue(aColorBGR);
	}
	//else leave uninitialized since they won't be used.
 
	HDC hdc = GetDC(NULL);
	if (!hdc)
		return { -2, -2 };
 
	bool found = false; // Must init here for use by "goto fast_end" and for use by both fast and slow modes.
 
 
						// From this point on, "goto fast_end" will assume hdc is non-NULL but that the below might still be NULL.
						// Therefore, all of the following must be initialized so that the "fast_end" label can detect them:
	HDC sdc = NULL;
	HBITMAP hbitmap_screen = NULL;
	LPCOLORREF screen_pixel = NULL;
	HGDIOBJ sdc_orig_select = NULL;
 
	// Some explanation for the method below is contained in this quote from the newsgroups:
	// "you shouldn't really be getting the current bitmap from the GetDC DC. This might
	// have weird effects like returning the entire screen or not working. Create yourself
	// a memory DC first of the correct size. Then BitBlt into it and then GetDIBits on
	// that instead. This way, the provider of the DC (the video driver) can make sure that
	// the correct pixels are copied across."
 
	// Initialize screen_pixel_count so VS2017 wont complain
	LONG screen_pixel_count;
 
	// Create an empty bitmap to hold all the pixels currently visible on the screen (within the search area):
	int search_width = aRight - aLeft + 1;
	int search_height = aBottom - aTop + 1;
	if (!(sdc = CreateCompatibleDC(hdc)) || !(hbitmap_screen = CreateCompatibleBitmap(hdc, search_width, search_height)))
		goto fast_end;
 
	if (!(sdc_orig_select = SelectObject(sdc, hbitmap_screen)))
		goto fast_end;
 
	// Copy the pixels in the search-area of the screen into the DC to be searched:
	if (!(BitBlt(sdc, 0, 0, search_width, search_height, hdc, aLeft, aTop, SRCCOPY)))
		goto fast_end;
 
	LONG screen_width, screen_height;
	bool screen_is_16bit;
	if (!(screen_pixel = getbits(hbitmap_screen, sdc, screen_width, screen_height, screen_is_16bit)))
		goto fast_end;
 
	// Concerning 0xF8F8F8F8: "On 16bit and 15 bit color the first 5 bits in each byte are valid
	// (in 16bit there is an extra bit but i forgot for which color). And this will explain the
	// second problem [in the test script], since GetPixel even in 16bit will return some "valid"
	// data in the last 3bits of each byte."
	register int i;
	screen_pixel_count = screen_width * screen_height;
	if (screen_is_16bit)
		for (i = 0; i < screen_pixel_count; ++i)
			screen_pixel[i] &= 0xF8F8F8F8;
 
	if (aVariation < 1) // Caller wants an exact match on one particular color.
	{
		if (screen_is_16bit)
			aColorRGB &= 0xF8F8F8F8;
		for (i = 0; i < screen_pixel_count; ++i)
		{
			// Note that screen pixels sometimes have a non-zero high-order byte.  That's why
			// bit-and with 0x00FFFFFF is done.  Otherwise, reddish/orangish colors are not properly
			// found:
			if ((screen_pixel[i] & 0x00FFFFFF) == aColorRGB)
			{
				found = true;
				break;
			}
		}
	}
	else
	{
		// It seems more appropriate to do the 16-bit conversion prior to SET_COLOR_RANGE,
		// rather than applying 0xF8 to each of the high/low values individually.
		if (screen_is_16bit)
		{
			search_red &= 0xF8;
			search_green &= 0xF8;
			search_blue &= 0xF8;
		}
 
#define SET_COLOR_RANGE \
{\
	red_low = (aVariation > search_red) ? 0 : search_red - aVariation;\
	green_low = (aVariation > search_green) ? 0 : search_green - aVariation;\
	blue_low = (aVariation > search_blue) ? 0 : search_blue - aVariation;\
	red_high = (aVariation > 0xFF - search_red) ? 0xFF : search_red + aVariation;\
	green_high = (aVariation > 0xFF - search_green) ? 0xFF : search_green + aVariation;\
	blue_high = (aVariation > 0xFF - search_blue) ? 0xFF : search_blue + aVariation;\
}
 
		SET_COLOR_RANGE
 
			for (i = 0; i < screen_pixel_count; ++i)
			{
				// Note that screen pixels sometimes have a non-zero high-order byte.  But it doesn't
				// matter with the below approach, since that byte is not checked in the comparison.
				pixel = screen_pixel[i];
				red = GetBValue(pixel);   // Because pixel is in RGB vs. BGR format, red is retrieved with
				green = GetGValue(pixel); // GetBValue() and blue is retrieved with GetRValue().
				blue = GetRValue(pixel);
				if (red >= red_low && red <= red_high
					&& green >= green_low && green <= green_high
					&& blue >= blue_low && blue <= blue_high)
				{
					found = true;
					break;
				}
			}
	}
 
fast_end:
	// If found==false when execution reaches here, ErrorLevel is already set to the right value, so just
	// clean up then return.
	ReleaseDC(NULL, hdc);
	if (sdc)
	{
		if (sdc_orig_select) // i.e. the original call to SelectObject() didn't fail.
			SelectObject(sdc, sdc_orig_select); // Probably necessary to prevent memory leak.
		DeleteDC(sdc);
	}
	if (hbitmap_screen)
		DeleteObject(hbitmap_screen);
	if (screen_pixel)
		free(screen_pixel);
	else // One of the GDI calls failed and the search wasn't carried out.
		return { -2, -2 };
 
	// Otherwise, success.  Calculate xpos and ypos of where the match was found and adjust
	// coords to make them relative to the position of the target window (rect will contain
	// zeroes if this doesn't need to be done):
	if (found)
		return{ aLeft + i % screen_width , aTop + i / screen_width };
 
	return{ -1,-1 };
}
 
int main()
{
	for (int i = 0; i < 1000; i++) {
		POINT p = PixelSearch(0, 0, 1920, 1080, RGB(255, 255, 255), 0);
		std::cout << "X:" << p.x << " Y:" << p.y << std::endl;
	}
	system("pause");
	return 0;
}
