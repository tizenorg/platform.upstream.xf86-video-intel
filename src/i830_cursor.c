/* -*- c-basic-offset: 3 -*- */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright © 2002 David Dawes
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/
/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/i810/i830_cursor.c,v 1.6 2002/12/18 15:49:01 dawes Exp $ */

/*
 * Reformatted with GNU indent (2.2.8), using the following options:
 *
 *    -bad -bap -c41 -cd0 -ncdb -ci6 -cli0 -cp0 -ncs -d0 -di3 -i3 -ip3 -l78
 *    -lp -npcs -psl -sob -ss -br -ce -sc -hnl
 *
 * This provides a good match with the original i810 code and preferred
 * XFree86 formatting conventions.
 *
 * When editing this driver, please follow the existing formatting, and edit
 * with <TAB> characters expanded at 8-column intervals.
 */

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   David Dawes <dawes@xfree86.org>
 *
 * Updated for Dual Head capabilities:
 *   Alan Hourihane <alanh@tungstengraphics.com>
 *
 * Add ARGB HW cursor support:
 *   Alan Hourihane <alanh@tungstengraphics.com>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "xf86.h"
#include "xf86_OSproc.h"
#include "compiler.h"

#include "xf86fbman.h"

#include "i830.h"

static void I830LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src);
static void I830ShowCursor(ScrnInfoPtr pScrn);
static void I830HideCursor(ScrnInfoPtr pScrn);
static void I830SetCursorColors(ScrnInfoPtr pScrn, int bg, int fb);
static void I830SetCursorPosition(ScrnInfoPtr pScrn, int x, int y);
static Bool I830UseHWCursor(ScreenPtr pScrn, CursorPtr pCurs);
#ifdef ARGB_CURSOR
static void I830LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs);
static Bool I830UseHWCursorARGB(ScreenPtr pScrn, CursorPtr pCurs);
#endif

static void
I830SetPipeCursorBase (xf86CrtcPtr crtc)
{
    ScrnInfoPtr		pScrn = crtc->scrn;
    I830CrtcPrivatePtr	intel_crtc = crtc->driver_private;
    int			pipe = intel_crtc->pipe;
    I830Ptr		pI830 = I830PTR(pScrn);
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int			cursor_base = (pipe == 0 ? CURSOR_A_BASE : CURSOR_B_BASE);
    I830MemRange	*cursor_mem;

    if (pipe >= xf86_config->num_crtc)
	FatalError("Bad pipe number for cursor base setting\n");

    if (pI830->CursorIsARGB)
	cursor_mem = &intel_crtc->cursor_mem_argb;
    else
	cursor_mem = &intel_crtc->cursor_mem;

    if (pI830->CursorNeedsPhysical) {
	OUTREG(cursor_base, cursor_mem->Physical);
    } else {
	OUTREG(cursor_base, cursor_mem->Start);
    }
}

void
I830SetPipeCursor (xf86CrtcPtr crtc, Bool force)
{
    ScrnInfoPtr		pScrn = crtc->scrn;
    I830CrtcPrivatePtr	intel_crtc = crtc->driver_private;
    int			pipe = intel_crtc->pipe;
    I830Ptr		pI830 = I830PTR(pScrn);
    CARD32		temp;
    Bool		show;
    
    if (!crtc->enabled)
	return;

    show = pI830->cursorOn && crtc->cursorInRange;
    if (show && (force || !crtc->cursorShown))
    {
	if (IS_MOBILE(pI830) || IS_I9XX(pI830)) {
	    int	cursor_control;
	    if (pipe == 0)
		cursor_control = CURSOR_A_CONTROL;
	    else
		cursor_control = CURSOR_B_CONTROL;
	    temp = INREG(cursor_control);
	    temp &= ~(CURSOR_MODE | MCURSOR_PIPE_SELECT);
	    if (pI830->CursorIsARGB) {
		temp |= CURSOR_MODE_64_ARGB_AX | MCURSOR_GAMMA_ENABLE;
	    } else
		temp |= CURSOR_MODE_64_4C_AX;
	    
	    temp |= (pipe << 28); /* Connect to correct pipe */
	    /* Need to set mode, then address. */
	    OUTREG(cursor_control, temp);
	} else {
	    temp = INREG(CURSOR_CONTROL);
	    temp &= ~(CURSOR_FORMAT_MASK);
	    temp |= CURSOR_ENABLE;
	    if (pI830->CursorIsARGB) {
		temp |= CURSOR_FORMAT_ARGB | CURSOR_GAMMA_ENABLE;
	    } else
		temp |= CURSOR_FORMAT_3C;
	    OUTREG(CURSOR_CONTROL, temp);
	}
	crtc->cursorShown = TRUE;
    }
    else if (!show && (force || crtc->cursorShown))
    {
	if (IS_MOBILE(pI830) || IS_I9XX(pI830)) 
	{
	    int	cursor_control;
	    if (pipe == 0)
		cursor_control = CURSOR_A_CONTROL;
	    else
		cursor_control = CURSOR_B_CONTROL;
	    temp = INREG(cursor_control);
	    temp &= ~(CURSOR_MODE|MCURSOR_GAMMA_ENABLE);
	    temp |= CURSOR_MODE_DISABLE;
	    OUTREG(cursor_control, temp);
	} else {
	    temp = INREG(CURSOR_CONTROL);
	    temp &= ~(CURSOR_ENABLE|CURSOR_GAMMA_ENABLE);
	    OUTREG(CURSOR_CONTROL, temp);
	}
	crtc->cursorShown = FALSE;
    }

    /* Flush cursor changes. */
    I830SetPipeCursorBase(crtc);
}

void
I830InitHWCursor(ScrnInfoPtr pScrn)
{
   xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
   I830Ptr pI830 = I830PTR(pScrn);
   CARD32 temp;
   int i;

   DPRINTF(PFX, "I830InitHWCursor\n");
   for (i = 0; i < xf86_config->num_crtc; i++) 
      xf86_config->crtc[i]->cursorShown = FALSE;

   /* Initialise the HW cursor registers, leaving the cursor hidden. */
   if (IS_MOBILE(pI830) || IS_I9XX(pI830)) {
      for (i = 0; i < xf86_config->num_crtc; i++)
      {
	 int   cursor_control = i == 0 ? CURSOR_A_CONTROL : CURSOR_B_CONTROL;
	 temp = INREG(cursor_control);
	 temp &= ~(CURSOR_MODE | MCURSOR_GAMMA_ENABLE |
		   MCURSOR_MEM_TYPE_LOCAL |
		   MCURSOR_PIPE_SELECT);
	 temp |= (i << 28);
	 if (pI830->CursorIsARGB)
	    temp |= CURSOR_MODE_64_ARGB_AX | MCURSOR_GAMMA_ENABLE;
	 else
	    temp |= CURSOR_MODE_64_4C_AX;
	 /* Need to set control, then address. */
	 OUTREG(cursor_control, temp);
	 I830SetPipeCursorBase(xf86_config->crtc[i]);
      }
   } else {
      temp = INREG(CURSOR_CONTROL);
      temp &= ~(CURSOR_FORMAT_MASK | CURSOR_GAMMA_ENABLE |
		CURSOR_ENABLE  | CURSOR_STRIDE_MASK);
      if (pI830->CursorIsARGB)
         temp |= CURSOR_FORMAT_ARGB | CURSOR_GAMMA_ENABLE;
      else 
         temp |= CURSOR_FORMAT_3C;
      /* This initialises the format and leave the cursor disabled. */
      OUTREG(CURSOR_CONTROL, temp);
      /* Need to set address and size after disabling. */
      I830SetPipeCursorBase(xf86_config->crtc[0]);
      temp = ((I810_CURSOR_X & CURSOR_SIZE_MASK) << CURSOR_SIZE_HSHIFT) |
	     ((I810_CURSOR_Y & CURSOR_SIZE_MASK) << CURSOR_SIZE_VSHIFT);
      OUTREG(CURSOR_SIZE, temp);
   }
}

Bool
I830CursorInit(ScreenPtr pScreen)
{
   ScrnInfoPtr pScrn;
   I830Ptr pI830;
   xf86CursorInfoPtr infoPtr;

   DPRINTF(PFX, "I830CursorInit\n");
   pScrn = xf86Screens[pScreen->myNum];
   pI830 = I830PTR(pScrn);
   pI830->CursorInfoRec = infoPtr = xf86CreateCursorInfoRec();
   if (!infoPtr)
      return FALSE;

   infoPtr->MaxWidth = I810_CURSOR_X;
   infoPtr->MaxHeight = I810_CURSOR_Y;
   infoPtr->Flags = (HARDWARE_CURSOR_TRUECOLOR_AT_8BPP |
		     HARDWARE_CURSOR_BIT_ORDER_MSBFIRST |
		     HARDWARE_CURSOR_INVERT_MASK |
		     HARDWARE_CURSOR_SWAP_SOURCE_AND_MASK |
		     HARDWARE_CURSOR_AND_SOURCE_WITH_MASK |
		     HARDWARE_CURSOR_SOURCE_MASK_INTERLEAVE_64 | 0);

   infoPtr->SetCursorColors = I830SetCursorColors;
   infoPtr->SetCursorPosition = I830SetCursorPosition;
   infoPtr->LoadCursorImage = I830LoadCursorImage;
   infoPtr->HideCursor = I830HideCursor;
   infoPtr->ShowCursor = I830ShowCursor;
   infoPtr->UseHWCursor = I830UseHWCursor;
#ifdef ARGB_CURSOR
   infoPtr->UseHWCursorARGB = I830UseHWCursorARGB;
   infoPtr->LoadCursorARGB = I830LoadCursorARGB;
#endif

   pI830->pCurs = NULL;


   I830HideCursor(pScrn);

   return xf86InitCursor(pScreen, infoPtr);
}

static Bool
I830UseHWCursor(ScreenPtr pScreen, CursorPtr pCurs)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);

   pI830->pCurs = pCurs;

   return TRUE;
}

static void
I830CRTCLoadCursorImage(xf86CrtcPtr crtc, unsigned char *src)
{
   ScrnInfoPtr pScrn = crtc->scrn;
   I830Ptr pI830 = I830PTR(pScrn);
   I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
   CARD8 *pcurs = (CARD8 *) (pI830->FbBase + intel_crtc->cursor_mem.Start);
   int x, y;

   DPRINTF(PFX, "I830LoadCursorImage\n");

#ifdef ARGB_CURSOR
   pI830->CursorIsARGB = FALSE;
#endif
 
   memset(pcurs, 0, 64 * 64 / 4);

#define GetBit(image, x, y)\
    ((int)((*(image + ((x) / 8) + ((y) * (128/8))) &\
	    (1 << ( 7 -((x) % 8) ))) ? 1 : 0))

#define SetBit(image, x, y)\
    (*(image + (x) / 8 + (y) * (128/8)) |=\
     (int) (1 <<  (7-((x) % 8))))

   switch (crtc->rotation) {
      case RR_Rotate_90:
         for (y = 0; y < 64; y++) {
            for (x = 0; x < 64; x++) {
               if (GetBit(src, 64 - y - 1, x))
                  SetBit(pcurs, x, y);
               if (GetBit(src, 128 - y - 1, x))
                  SetBit(pcurs, x + 64, y);
            }
         }

         return;
      case RR_Rotate_180:
         for (y = 0; y < 64; y++) {
            for (x = 0; x < 64; x++) {
               if (GetBit(src, 64 - x - 1, 64 - y - 1))
                  SetBit(pcurs, x, y);
               if (GetBit(src, 128 - x - 1, 64 - y - 1))
                  SetBit(pcurs, x + 64, y);
            }
         }

         return;
      case RR_Rotate_270:
         for (y = 0; y < 64; y++) {
            for (x = 0; x < 64; x++) {
               if (GetBit(src, y, 64 - x - 1))
                  SetBit(pcurs, x, y);
               if (GetBit(src, y + 64, 64 - x - 1))
                  SetBit(pcurs, x + 64, y);
            }
         }

         return;
   }

   for (y = 0; y < 64; y++) {
      for (x = 0; x < 64 / 4; x++) {
	 *pcurs++ = *src++;
      }
   }
}

static void
I830LoadCursorImage(ScrnInfoPtr pScrn, unsigned char *src)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int pipe;

    for (pipe = 0; pipe < xf86_config->num_crtc; pipe++) {
       I830CRTCLoadCursorImage(xf86_config->crtc[pipe], src);
    }
}

#ifdef ARGB_CURSOR
#include "cursorstr.h"

static Bool I830UseHWCursorARGB (ScreenPtr pScreen, CursorPtr pCurs)
{
   ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
   I830Ptr pI830 = I830PTR(pScrn);
   xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
   int i;

   DPRINTF(PFX, "I830UseHWCursorARGB\n");

   pI830->pCurs = pCurs;

   /* Check that our ARGB allocations succeeded */
   for (i = 0; i < xf86_config->num_crtc; i++) {
      I830CrtcPrivatePtr intel_crtc = xf86_config->crtc[i]->driver_private;

      if (!intel_crtc->cursor_mem_argb.Start)
	 return FALSE;
   }

   if (pScrn->bitsPerPixel == 8)
      return FALSE;

   if (pCurs->bits->height <= 64 && pCurs->bits->width <= 64) 
	return TRUE;

   return FALSE;
}

static void I830CRTCLoadCursorARGB (xf86CrtcPtr crtc, CursorPtr pCurs)
{
   I830Ptr pI830 = I830PTR(crtc->scrn);
   I830CrtcPrivatePtr intel_crtc = crtc->driver_private;
   CARD32 *dst = (CARD32 *) (pI830->FbBase + intel_crtc->cursor_mem.Start);
   CARD32 *image = (CARD32 *)pCurs->bits->argb;
   int x, y, w, h;

   DPRINTF(PFX, "I830LoadCursorARGB\n");

   if (!image)
	return;	/* XXX can't happen */
    
   pI830->CursorIsARGB = TRUE;

   w = pCurs->bits->width;
   h = pCurs->bits->height;

   switch (crtc->rotation) {
      case RR_Rotate_90:
         for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++)
               dst[(y) + ((64 - x - 1) * 64)] = *image++;
            for(; x < 64; x++)
               dst[(y) + ((64 - x - 1) * 64)] = 0;
         }
         for(; y < 64; y++) {
   	    for(x = 0; x < 64; x++)
               dst[(y) + ((64 - x - 1) * 64)] = 0;
         }
         return;

      case RR_Rotate_180:
         for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++)
               dst[(64 - x - 1) + ((64 - y - 1) * 64)] = *image++;
            for(; x < 64; x++)
               dst[(64 - x - 1) + ((64 - y - 1) * 64)] = 0;
         }
         for(; y < 64; y++) {
            for(x = 0; x < 64; x++)
               dst[(64 - x - 1) + ((64 - y - 1) * 64)] = 0;
         }
         return;

      case RR_Rotate_270:
         for (y = 0; y < h; y++) {
            for (x = 0; x < w; x++)
               dst[(64 - y - 1) + (x * 64)] = *image++;
            for(; x < 64; x++)
               dst[(64 - y - 1) + (x * 64)] = 0;
         }
         for(; y < 64; y++) {
            for(x = 0; x < 64; x++)
               dst[(64 - y - 1) + (x * 64)] = 0;
         }
         return;
   }

   for(y = 0; y < h; y++) {
      for(x = 0; x < w; x++)
          *dst++ = *image++;
      for(; x < 64; x++)
          *dst++ = 0;
   }

   for(; y < 64; y++) {
      for(x = 0; x < 64; x++)
          *dst++ = 0;
   }
}

static void
I830LoadCursorARGB(ScrnInfoPtr pScrn, CursorPtr pCurs)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    int pipe;

    for (pipe = 0; pipe < xf86_config->num_crtc; pipe++) {
       I830CRTCLoadCursorARGB(xf86_config->crtc[pipe], pCurs);
    }
}
#endif

static void
I830SetCursorPosition(ScrnInfoPtr pScrn, int x, int y)
{
    xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    I830Ptr pI830 = I830PTR(pScrn);
    CARD32 temp;
    Bool inrange;
    int root_x = x, root_y = y;
    int pipe;

    root_x = x + pScrn->frameX0; /* undo what xf86HWCurs did */
    root_y = y + pScrn->frameY0;

    for (pipe = 0; pipe < xf86_config->num_crtc; pipe++)
    {
	xf86CrtcPtr	    crtc = xf86_config->crtc[pipe];
	DisplayModePtr	    mode = &crtc->mode;
	int		    thisx;
	int		    thisy;
	int hotspotx = 0, hotspoty = 0;

	if (!crtc->enabled)
	    continue;

	switch (crtc->rotation) {
	case RR_Rotate_0:
	    thisx = (root_x - crtc->x);
	    thisy = (root_y - crtc->y);
	    break;
	case RR_Rotate_90:
	    thisx = (root_y - crtc->y);
	    thisy = mode->VDisplay - (root_x - crtc->x);
	    hotspoty = I810_CURSOR_X;
	    break;
	case RR_Rotate_180:
	    thisx = mode->HDisplay - (root_x - crtc->x);
	    thisy = mode->VDisplay - (root_y - crtc->y);
	    hotspotx = I810_CURSOR_X;
	    hotspoty = I810_CURSOR_Y;
	    break;
	case RR_Rotate_270:
	    thisx = mode->VDisplay - (root_y - crtc->y);
	    thisy = (root_x - crtc->x);
	    hotspotx = I810_CURSOR_Y;
	    break;
	}

	thisx -= hotspotx;
	thisy -= hotspoty;

	/*
	 * There is a screen display problem when the cursor position is set
	 * wholely outside of the viewport.  We trap that here, turning the
	 * cursor off when that happens, and back on when it comes back into
	 * the viewport.
	 */
	inrange = TRUE;
	if (thisx >= mode->HDisplay ||
	    thisy >= mode->VDisplay ||
	    thisx <= -I810_CURSOR_X || thisy <= -I810_CURSOR_Y) 
	{
	    inrange = FALSE;
	    thisx = 0;
	    thisy = 0;
	}

	temp = 0;
	if (thisx < 0) {
	    temp |= (CURSOR_POS_SIGN << CURSOR_X_SHIFT);
	    thisx = -thisx;
	}
	if (thisy < 0) {
	    temp |= (CURSOR_POS_SIGN << CURSOR_Y_SHIFT);
	    thisy = -thisy;
	}
	temp |= ((thisx & CURSOR_POS_MASK) << CURSOR_X_SHIFT);
	temp |= ((thisy & CURSOR_POS_MASK) << CURSOR_Y_SHIFT);

	if (pipe == 0)
	    OUTREG(CURSOR_A_POSITION, temp);
	if (pipe == 1)
	    OUTREG(CURSOR_B_POSITION, temp);

	crtc->cursorInRange = inrange;
	
        I830SetPipeCursor (crtc, FALSE);
    }
}

static void
I830ShowCursor(ScrnInfoPtr pScrn)
{
    xf86CrtcConfigPtr xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
    I830Ptr pI830 = I830PTR(pScrn);
    int pipe;

    DPRINTF(PFX, "I830ShowCursor\n");

    pI830->cursorOn = TRUE;
    for (pipe = 0; pipe < xf86_config->num_crtc; pipe++)
	I830SetPipeCursor (xf86_config->crtc[pipe], TRUE);
}

static void
I830HideCursor(ScrnInfoPtr pScrn)
{
   xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
   I830Ptr pI830 = I830PTR(pScrn);
    int pipe;

   DPRINTF(PFX, "I830HideCursor\n");

   pI830->cursorOn = FALSE;
    for (pipe = 0; pipe < xf86_config->num_crtc; pipe++)
	I830SetPipeCursor (xf86_config->crtc[pipe], TRUE);
}

static void
I830SetCursorColors(ScrnInfoPtr pScrn, int bg, int fg)
{
   xf86CrtcConfigPtr   xf86_config = XF86_CRTC_CONFIG_PTR(pScrn);
   I830Ptr pI830 = I830PTR(pScrn);
   int pipe; 

#ifdef ARGB_CURSOR
    /* Don't recolour cursors set with SetCursorARGB. */
    if (pI830->CursorIsARGB)
       return;
#endif

   DPRINTF(PFX, "I830SetCursorColors\n");

   for (pipe = 0; pipe < xf86_config->num_crtc; pipe++)
   {
      xf86CrtcPtr	crtc = xf86_config->crtc[pipe];
      int		pal0 = pipe == 0 ? CURSOR_A_PALETTE0 : CURSOR_B_PALETTE0;

      if (crtc->enabled)
      {
	 OUTREG(pal0 +  0, bg & 0x00ffffff);
	 OUTREG(pal0 +  4, fg & 0x00ffffff);
	 OUTREG(pal0 +  8, fg & 0x00ffffff);
	 OUTREG(pal0 + 12, bg & 0x00ffffff);
      }
   }
}
