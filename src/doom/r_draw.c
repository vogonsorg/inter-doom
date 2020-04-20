//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2016-2019 Julian Nechaevsky
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	The actual span/column drawing functions.
//	Here find the main potential for optimization,
//	 e.g. inline assembly, different algorithms.
//



#include "doomdef.h"
#include "deh_main.h"

#include "i_system.h"
#include "z_zone.h"
#include "w_wad.h"

#include "r_local.h"

// Needs access to LFB (guess what).
#include "v_video.h"
#include "v_trans.h"

// State.
#include "doomstat.h"

#include "jn.h"

// ?
// #define MAXWIDTH    1120
// #define MAXHEIGHT   832

// status bar height at bottom of screen
#define SBARHEIGHT  (32 << hires)

// [JN] Jaguar: use different height of status bar
// For some reason, value 40 leaving one pixel trash line on bezel, so 39 here.
#define SBARHEIGHT_JAG (39 << hires)

//
// All drawing to the view buffer is accomplished in this file.
// The other refresh files only know about ccordinates,
//  not the architecture of the frame buffer.
// Conveniently, the frame buffer is a linear one,
//  and we need only the base address,
//  and the total size == width*height*depth/8.,
//

byte*   viewimage; 
int     viewwidth;
int     scaledviewwidth;
int     viewheight;
int     scaledviewheight;
int     viewwindowx;
int     viewwindowy; 
byte*   ylookup[MAXHEIGHT]; 
int     columnofs[WIDEMAXWIDTH]; 


// Color tables for different players,
//  translate a limited part to another
//  (color ramps used for  suit colors).
//
byte    translations[3][256];	
 
// Backing buffer containing the bezel drawn around the screen and 
// surrounding background.

static byte *background_buffer = NULL;


//
// R_DrawColumn
// Source is the top of the column to scale.
//

lighttable_t*   dc_colormap; 
int             dc_x; 
int             dc_yl; 
int             dc_yh; 
fixed_t         dc_iscale; 
fixed_t         dc_texturemid;
int             dc_texheight;


// first pixel in a column (possibly virtual) 
byte*   dc_source;		

// just for profiling 
int dccount;

// [JN] External data for new fuzz effect
extern boolean inhelpscreens;
extern int Crispy_Random(void);


//
// A column is a vertical slice/span from a wall texture that,
//  given the DOOM style restrictions on the view orientation,
//  will always have constant z depth.
// Thus a special case loop for very fast rendering can
//  be used. It has also been used with Wolfenstein 3D.
// 
void R_DrawColumn (void) 
{ 
    int                 count;
    register byte       *dest;     // killough
    register fixed_t    frac;      // killough
    fixed_t             fracstep;

    count = dc_yh - dc_yl + 1;

    if (count <= 0)    // Zero length, column does not exceed a pixel.
    return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawColumn: %i to %i at %i" :
                 "R_DrawColumn: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif

    // Framebuffer destination address.
    // Use ylookup LUT to avoid multiply with ScreenWidth.
    // Use columnofs LUT for subwindows?

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];

    // Determine scaling, which is the only mapping to be done.

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    // Inner loop that does the actual texture mapping,
    //  e.g. a DDA-lile scaling.
    // This is as fast as it gets.       (Yeah, right!!! -- killough)
    //
    // killough 2/1/98: more performance tuning

    {
        register const byte *source = dc_source;
        register const lighttable_t *colormap = dc_colormap;
        register int heightmask = dc_texheight-1;

        if (dc_texheight & heightmask)   // not a power of 2 -- killough
        {
            heightmask++;
            heightmask <<= FRACBITS;

            if (frac < 0)
            while ((frac += heightmask) < 0);
            else
            while (frac >= heightmask)
            frac -= heightmask;

            do
            {
                // Re-map color indices from wall texture column
                //  using a lighting/special effects LUT.

                // heightmask is the Tutti-Frutti fix -- killough

                *dest = colormap[source[frac>>FRACBITS]];
                dest += screenwidth;                     // killough 11/98
                if ((frac += fracstep) >= heightmask)
                frac -= heightmask;
            }
            while (--count);
        }
        else
        {
            while ((count-=2)>=0)   // texture height is a power of 2 -- killough
            {
                *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
                dest += screenwidth;   // killough 11/98
                frac += fracstep;
                *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
                dest += screenwidth;   // killough 11/98
                frac += fracstep;
            }
            if (count & 1)
            *dest = colormap[source[(frac>>FRACBITS) & heightmask]];
        }
    }
}


void R_DrawColumnLow (void) 
{ 
    int         count; 
    byte*       dest; 
    byte*       dest2;
    byte*       dest3;
    byte*       dest4;
    fixed_t     frac;
    fixed_t     fracstep;	 
    int         x;
    int         heightmask = dc_texheight - 1;

    count = dc_yh - dc_yl; 

    // Zero length.
    if (count < 0) 
    return; 

#ifdef RANGECHECK 
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawColumn: %i to %i at %i" :
                 "R_DrawColumn: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
    //	dccount++; 
#endif 

    // Blocky mode, need to multiply by 2.
    x = dc_x << 1;

    dest = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];

    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    // heightmask is the Tutti-Frutti fix -- killough
    if (dc_texheight & heightmask) // not a power of 2 -- killough
    {
        heightmask++;
        heightmask <<= FRACBITS;

        if (frac < 0)
        while ((frac += heightmask) < 0);
        else
        while (frac >= heightmask)
        frac -= heightmask;

        do
        {
            *dest2 = *dest = dc_colormap[dc_source[frac>>FRACBITS]];

            dest += screenwidth << hires;
            dest2 += screenwidth << hires;

            if (hires)
            {
                *dest4 = *dest3 = dc_colormap[dc_source[frac>>FRACBITS]];
                dest3 += screenwidth << hires;
                dest4 += screenwidth << hires;
            }

            if ((frac += fracstep) >= heightmask)
            frac -= heightmask;
        } while (count--);
    }
    else // texture height is a power of 2 -- killough
    {
        do 
        {
            // Hack. Does not work corretly.
            *dest2 = *dest = dc_colormap[dc_source[(frac>>FRACBITS)&heightmask]];
            dest += screenwidth << hires;
            dest2 += screenwidth << hires;

            if (hires)
            {
                *dest4 = *dest3 = dc_colormap[dc_source[(frac>>FRACBITS)&heightmask]];
                dest3 += screenwidth << hires;
                dest4 += screenwidth << hires;
            }

            frac += fracstep; 

        } while (count--);
    }
}


//
// Spectre/Invisibility.
//
#define FUZZTABLE   50 
#define FUZZOFF     (SCREENWIDTH)
#define WFUZZOFF    (WIDESCREENWIDTH)

int	fuzzoffset[FUZZTABLE] =
{
    FUZZOFF, -FUZZOFF, FUZZOFF,-FUZZOFF, FUZZOFF, FUZZOFF,-FUZZOFF,
    FUZZOFF,  FUZZOFF,-FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF,-FUZZOFF,
    FUZZOFF,  FUZZOFF, FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF,
    FUZZOFF, -FUZZOFF,-FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF, FUZZOFF,-FUZZOFF,
    FUZZOFF, -FUZZOFF, FUZZOFF, FUZZOFF,-FUZZOFF,-FUZZOFF, FUZZOFF,
    FUZZOFF, -FUZZOFF,-FUZZOFF,-FUZZOFF,-FUZZOFF, FUZZOFF, FUZZOFF,
    FUZZOFF,  FUZZOFF,-FUZZOFF, FUZZOFF, FUZZOFF,-FUZZOFF, FUZZOFF 
};

int	wfuzzoffset[FUZZTABLE] =
{
    WFUZZOFF,-WFUZZOFF,WFUZZOFF,-WFUZZOFF,WFUZZOFF,WFUZZOFF,-WFUZZOFF,
    WFUZZOFF,WFUZZOFF,-WFUZZOFF,WFUZZOFF,WFUZZOFF,WFUZZOFF,-WFUZZOFF,
    WFUZZOFF,WFUZZOFF,WFUZZOFF,-WFUZZOFF,-WFUZZOFF,-WFUZZOFF,-WFUZZOFF,
    WFUZZOFF,-WFUZZOFF,-WFUZZOFF,WFUZZOFF,WFUZZOFF,WFUZZOFF,WFUZZOFF,-WFUZZOFF,
    WFUZZOFF,-WFUZZOFF,WFUZZOFF,WFUZZOFF,-WFUZZOFF,-WFUZZOFF,WFUZZOFF,
    WFUZZOFF,-WFUZZOFF,-WFUZZOFF,-WFUZZOFF,-WFUZZOFF,WFUZZOFF,WFUZZOFF,
    WFUZZOFF,WFUZZOFF,-WFUZZOFF,WFUZZOFF,WFUZZOFF,-WFUZZOFF,WFUZZOFF 
}; 

int	fuzzpos = 0; 

// [crispy] draw fuzz effect independent of rendering frame rate
static int fuzzpos_tic;
void R_SetFuzzPosTic (void)
{
	fuzzpos_tic = fuzzpos;
}
void R_SetFuzzPosDraw (void)
{
	fuzzpos = fuzzpos_tic;
}

//
// Framebuffer postprocessing.
// Creates a fuzzy image by copying pixels
//  from adjacent ones to left and right.
// Used with an all black colormap, this
//  could create the SHADOW effect,
//  i.e. spectres and invisible players.
//

// -----------------------------------------------------------------------------
// [JN] Fuzz effect, original version (improved_fuzz = 0)
// -----------------------------------------------------------------------------

void R_DrawFuzzColumn (void) 
{ 
    int     count; 
    byte*   dest; 
    fixed_t frac;
    fixed_t fracstep;	 
    boolean cutoff = false;
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    // Adjust borders. Low... 
    if (!dc_yl) 
    dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight-1) 
    {
    dc_yh = viewheight - 2; 
    cutoff = true;
    }

    count = dc_yh - dc_yl; 

    // Zero length.
    if (count < 0) 
    return; 

#ifdef RANGECHECK 
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumn: %i to %i at %i" :
                 "R_DrawFuzzColumn: %i к %i у %i", dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
        // Lookup framebuffer, and retrieve
        //  a pixel that is either one column
        //  left or right of the current one.
        // Add index from colormap to index.
        *dest = colormaps[6*256+dest[afuzzoffset[fuzzpos]]]; 

        // Clamp table lookup index.
        if (++fuzzpos == FUZZTABLE) 
        fuzzpos = 0;

        dest += screenwidth;

        frac += fracstep; 
    } while (count--); 

    // [crispy] if the line at the bottom had to be cut off,
    // draw one extra line using only pixels of that line and the one above
    if (cutoff)
    {
        if (widescreen)
        {
            *dest = colormaps[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = colormaps[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }
    }
} 


// low detail mode version
void R_DrawFuzzColumnLow (void) 
{ 
    int     count; 
    byte*   dest; 
    byte*   dest2; 
    byte*   dest3;
    byte*   dest4;
    fixed_t frac;
    fixed_t fracstep;	 
    int     x;
    boolean cutoff = false;
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    // Adjust borders. Low... 
    if (!dc_yl) 
    dc_yl = 1;

    // .. and high.
    if (dc_yh == viewheight-1)
    {
    dc_yh = viewheight - 2; 
    cutoff = true;
    }

    count = dc_yh - dc_yl; 

    // Zero length.
    if (count < 0) 
    return; 

    // low detail mode, need to multiply by 2
    x = dc_x << 1;

#ifdef RANGECHECK 
    if ((unsigned)x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnLow: %i to %i at %i" :
                 "R_DrawFuzzColumnLow: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif

    dest  = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Looks like an attempt at dithering,
    //  using the colormap #6 (of 0-31, a bit
    //  brighter than average).
    do 
    {
        // Lookup framebuffer, and retrieve
        //  a pixel that is either one column
        //  left or right of the current one.
        // Add index from colormap to index.
        *dest = colormaps[6*256+dest[afuzzoffset[fuzzpos]]]; 
        *dest2 = colormaps[6*256+dest2[afuzzoffset[fuzzpos]]]; 
        if (hires)
        {
            *dest3 = colormaps[6*256+dest[afuzzoffset[fuzzpos]]];
            *dest4 = colormaps[6*256+dest2[afuzzoffset[fuzzpos]]];
            dest3 += screenwidth << hires;
            dest4 += screenwidth << hires;
        }

        // Clamp table lookup index.
        if (++fuzzpos == FUZZTABLE) 
        fuzzpos = 0;

        dest += screenwidth << hires;
        dest2 += screenwidth << hires;

        frac += fracstep; 
    } while (count--); 

    // [crispy] if the line at the bottom had to be cut off,
    // draw one extra line using only pixels of that line and the one above
    if (cutoff)
    {
        if (widescreen)
        {
            *dest = colormaps[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
            *dest2 = colormaps[6*256+dest2[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = colormaps[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
            *dest2 = colormaps[6*256+dest2[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }


        if (hires)
        {
            *dest3 = *dest;
            *dest4 = *dest2;
        }
    }
} 

// -----------------------------------------------------------------------------
// [JN] Fuzz effect, original version + black and white (improved_fuzz = 1)
// -----------------------------------------------------------------------------

void R_DrawFuzzColumnBW (void) 
{ 
    int     count; 
    byte*   dest; 
    fixed_t frac;
    fixed_t fracstep;	 
    boolean cutoff = false;
    boolean greenfuzz = infragreen_visor && players[consoleplayer].powers[pw_infrared];
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    if (!dc_yl) 
    dc_yl = 1;

    if (dc_yh == viewheight-1) 
    {
        dc_yh = viewheight - 2; 
        cutoff = true;
    }

    count = dc_yh - dc_yl; 

    if (count < 0) 
    return; 

#ifdef RANGECHECK 
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnBW: %i to %i at %i" :
                 "R_DrawFuzzColumnBW: %i к %i у %i", dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    do 
    {
        *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[afuzzoffset[fuzzpos]]]; 

        if (++fuzzpos == FUZZTABLE) 
        fuzzpos = 0;

        dest += screenwidth;

        frac += fracstep; 
    } while (count--); 

    if (cutoff)
    {
        if (widescreen)
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }
    }
} 

void R_DrawFuzzColumnLowBW (void) 
{ 
    int     count; 
    byte*   dest; 
    byte*   dest2; 
    byte*   dest3;
    byte*   dest4;
    fixed_t frac;
    fixed_t fracstep;	 
    int     x;
    boolean cutoff = false;
    boolean greenfuzz = infragreen_visor && players[consoleplayer].powers[pw_infrared];
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    if (!dc_yl) 
    dc_yl = 1;

    if (dc_yh == viewheight-1)
    {
        dc_yh = viewheight - 2; 
        cutoff = true;
    }

    count = dc_yh - dc_yl; 

    if (count < 0) 
    return; 

    x = dc_x << 1;

#ifdef RANGECHECK 
    if ((unsigned)x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnLowBW: %i to %i at %i" :
                 "R_DrawFuzzColumnLowBW: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif

    dest  = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    do 
    {
        *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[afuzzoffset[fuzzpos]]]; 
        *dest2 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[afuzzoffset[fuzzpos]]]; 
        if (hires)
        {
            *dest3 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[afuzzoffset[fuzzpos]]];
            *dest4 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[afuzzoffset[fuzzpos]]];
            dest3 += screenwidth << hires;
            dest4 += screenwidth << hires;
        }

        if (++fuzzpos == FUZZTABLE) 
        fuzzpos = 0;

        dest += screenwidth << hires;
        dest2 += screenwidth << hires;

        frac += fracstep; 
    } while (count--); 

    if (cutoff)
    {
        if (widescreen)
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
            *dest2 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
            *dest2 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }

        if (hires)
        {
            *dest3 = *dest;
            *dest4 = *dest2;
        }
    }
}

// -----------------------------------------------------------------------------
// [JN] Fuzz effect, improved version (improved_fuzz = 2)
// -----------------------------------------------------------------------------

void R_DrawFuzzColumnImproved (void)
{ 
    int     count; 
    byte*   dest; 
    fixed_t frac;
    fixed_t fracstep;	 
    boolean cutoff = false;
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    if (!dc_yl) 
    dc_yl = 1;

    if (dc_yh == viewheight-1) 
    {
        dc_yh = viewheight - 2; 
        cutoff = true;
    }

    count = dc_yh - dc_yl; 

    if (count < 0) 
    return; 

#ifdef RANGECHECK 
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnImproved: %i to %i at %i" :
                 "R_DrawFuzzColumnImproved: %i к %i у %i", dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    do 
    {
        *dest = colormaps[6*256+dest[afuzzoffset[fuzzpos]]]; 

        if (++fuzzpos == FUZZTABLE) 
        {
            fuzzpos = paused || menuactive || inhelpscreens ? 0 : Crispy_Random()%49;
        }

        dest += screenwidth;

        frac += fracstep; 
    } while (count--); 

    if (cutoff)
    {
        if (widescreen)
        {
            *dest = colormaps[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = colormaps[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }
    }
} 

void R_DrawFuzzColumnLowImproved (void) 
{ 
    int     count; 
    byte*   dest; 
    byte*   dest2; 
    byte*   dest3;
    byte*   dest4;
    fixed_t frac;
    fixed_t fracstep;	 
    int     x;
    boolean cutoff = false;
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    if (!dc_yl) 
    dc_yl = 1;

    if (dc_yh == viewheight-1)
    {
        dc_yh = viewheight - 2; 
        cutoff = true;
    }

    count = dc_yh - dc_yl; 

    if (count < 0) 
    return; 

    x = dc_x << 1;

#ifdef RANGECHECK 
    if ((unsigned)x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnLowImproved: %i to %i at %i" :
                 "R_DrawFuzzColumnLowImproved: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif

    dest  = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    do 
    {
        *dest = colormaps[6*256+dest[afuzzoffset[fuzzpos]]]; 
        *dest2 = colormaps[6*256+dest2[afuzzoffset[fuzzpos]]]; 
        if (hires)
        {
            *dest3 = colormaps[6*256+dest[afuzzoffset[fuzzpos]]];
            *dest4 = colormaps[6*256+dest2[afuzzoffset[fuzzpos]]];
            dest3 += screenwidth << hires;
            dest4 += screenwidth << hires;
        }

        if (++fuzzpos == FUZZTABLE) 
        {
            fuzzpos = paused || menuactive || inhelpscreens ? 0 : Crispy_Random()%49;
        }

        dest += screenwidth << hires;
        dest2 += screenwidth << hires;

        frac += fracstep; 
    } while (count--); 

    if (cutoff)
    {
        if (widescreen)
        {
            *dest = colormaps[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
            *dest2 = colormaps[6*256+dest2[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = colormaps[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
            *dest2 = colormaps[6*256+dest2[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }

        if (hires)
        {
            *dest3 = *dest;
            *dest4 = *dest2;
        }
    }
}

// -----------------------------------------------------------------------------
// [JN] Fuzz effect, improved version + black and white (improved_fuzz = 3)
// -----------------------------------------------------------------------------

void R_DrawFuzzColumnImprovedBW (void)
{ 
    int     count; 
    byte*   dest; 
    fixed_t frac;
    fixed_t fracstep;	 
    boolean cutoff = false;
    boolean greenfuzz = infragreen_visor && players[consoleplayer].powers[pw_infrared];
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    if (!dc_yl) 
    dc_yl = 1;

    if (dc_yh == viewheight-1) 
    {
        dc_yh = viewheight - 2; 
        cutoff = true;
    }

    count = dc_yh - dc_yl; 

    if (count < 0) 
    return; 

#ifdef RANGECHECK 
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnImprovedBW: %i to %i at %i" :
                 "R_DrawFuzzColumnImprovedBW: %i к %i у %i", dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    do 
    {
        *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[afuzzoffset[fuzzpos]]]; 

        if (++fuzzpos == FUZZTABLE) 
        {
            fuzzpos = paused || menuactive || inhelpscreens ? 0 : Crispy_Random()%49;
        }

        dest += screenwidth;

        frac += fracstep; 
    } while (count--); 

    if (cutoff)
    {
        if (widescreen)
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }
    }
} 

void R_DrawFuzzColumnLowImprovedBW (void) 
{ 
    int     count; 
    byte*   dest; 
    byte*   dest2; 
    byte*   dest3;
    byte*   dest4;
    fixed_t frac;
    fixed_t fracstep;	 
    int     x;
    boolean cutoff = false;
    boolean greenfuzz = infragreen_visor && players[consoleplayer].powers[pw_infrared];
    int* afuzzoffset = widescreen ? wfuzzoffset : fuzzoffset; // [RUDE] actual offset

    if (!dc_yl) 
    dc_yl = 1;

    if (dc_yh == viewheight-1)
    {
        dc_yh = viewheight - 2; 
        cutoff = true;
    }

    count = dc_yh - dc_yl; 

    if (count < 0) 
    return; 

    x = dc_x << 1;

#ifdef RANGECHECK 
    if ((unsigned)x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawFuzzColumnLowImprovedBW: %i to %i at %i" :
                 "R_DrawFuzzColumnLowImprovedBW: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif

    dest  = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    do 
    {
        *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[afuzzoffset[fuzzpos]]]; 
        *dest2 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[afuzzoffset[fuzzpos]]]; 
        if (hires)
        {
            *dest3 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[afuzzoffset[fuzzpos]]];
            *dest4 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[afuzzoffset[fuzzpos]]];
            dest3 += screenwidth << hires;
            dest4 += screenwidth << hires;
        }

        if (++fuzzpos == FUZZTABLE) 
        {
            fuzzpos = paused || menuactive || inhelpscreens ? 0 : Crispy_Random()%49;
        }

        dest += screenwidth << hires;
        dest2 += screenwidth << hires;

        frac += fracstep; 
    } while (count--); 

    if (cutoff)
    {
        if (widescreen)
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
            *dest2 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[(wfuzzoffset[fuzzpos]-WFUZZOFF)/2]];
        }
        else
        {
            *dest = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
            *dest2 = (greenfuzz ? colormaps_beta : colormaps_bw)[6*256+dest2[(fuzzoffset[fuzzpos]-FUZZOFF)/2]];
        }

        if (hires)
        {
            *dest3 = *dest;
            *dest4 = *dest2;
        }
    }
}


//
// R_DrawTranslatedColumn
// Used to draw player sprites
//  with the green colorramp mapped to others.
// Could be used with different translation
//  tables, e.g. the lighter colored version
//  of the BaronOfHell, the HellKnight, uses
//  identical sprites, kinda brightened up.
//
byte*   dc_translation;
byte*   translationtables;

void R_DrawTranslatedColumn (void) 
{ 
    int         count; 
    byte*       dest; 
    fixed_t     frac;
    fixed_t     fracstep;	 

    count = dc_yh - dc_yl; 
    if (count < 0) 
    return; 

#ifdef RANGECHECK 
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawColumn: %i to %i at %i" :
                 "R_DrawColumn: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }    
#endif 

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.
    do 
    {
        // Translation tables are used
        //  to map certain colorramps to other ones,
        //  used with PLAY sprites.
        // Thus the "green" ramp of the player 0 sprite
        //  is mapped to gray, red, black/indigo. 
        *dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
        dest += screenwidth;
	
        frac += fracstep; 
    } while (count--); 
} 


void R_DrawTranslatedColumnLow (void) 
{ 
    int     count; 
    byte*   dest; 
    byte*   dest2; 
    byte*   dest3;
    byte*   dest4;
    fixed_t frac;
    fixed_t fracstep;	 
    int     x;

    count = dc_yh - dc_yl; 
    if (count < 0) 
    return; 

    // low detail, need to scale by 2
    x = dc_x << 1;

#ifdef RANGECHECK 
    if ((unsigned)x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawColumn: %i to %i at %i" :
                 "R_DrawColumn: %i к %i у %i",
                 dc_yl, dc_yh, x);
    }
#endif 

    dest  = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];

    // Looks familiar.
    fracstep = dc_iscale; 
    frac = dc_texturemid + (dc_yl-centery)*fracstep; 

    // Here we do an additional index re-mapping.
    do 
    {
        // Translation tables are used
        //  to map certain colorramps to other ones,
        //  used with PLAY sprites.
        // Thus the "green" ramp of the player 0 sprite
        //  is mapped to gray, red, black/indigo. 
        *dest = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
        *dest2 = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
        dest += screenwidth << hires;
        dest2 += screenwidth << hires;
        if (hires)
        {
            *dest3 = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
            *dest4 = dc_colormap[dc_translation[dc_source[frac>>FRACBITS]]];
            dest3 += screenwidth << hires;
            dest4 += screenwidth << hires;
        }

        frac += fracstep; 
    } while (count--); 
}


void R_DrawTLColumn (void)
{
    int count;
    byte*   dest;
    fixed_t frac;
    fixed_t fracstep;

    count = dc_yh - dc_yl;
    if (count < 0)
    return;

#ifdef RANGECHECK
    if ((unsigned)dc_x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawColumn: %i to %i at %i" :
                 "R_DrawColumn: %i к %i у %i",
                 dc_yl, dc_yh, dc_x);
    }
#endif

    dest = ylookup[dc_yl] + columnofs[flipwidth[dc_x]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
        *dest = tranmap[(*dest<<8)+dc_colormap[dc_source[frac>>FRACBITS]]];
        dest += screenwidth;

        frac += fracstep;
    } while (count--);
}


// [crispy] draw translucent column, low-resolution version
void R_DrawTLColumnLow (void)
{
    int count;
    byte*   dest;
    byte*   dest2;
    byte*   dest3;
    byte*   dest4;
    fixed_t frac;
    fixed_t fracstep;
    int     x;

    count = dc_yh - dc_yl;
    if (count < 0)
    return;

    x = dc_x << 1;

#ifdef RANGECHECK
    if ((unsigned)x >= screenwidth || dc_yl < 0 || dc_yh >= SCREENHEIGHT)
    {
        I_Error (english_language ?
                 "R_DrawColumn: %i to %i at %i" :
                 "R_DrawColumn: %i к %i у %i",
                 dc_yl, dc_yh, x);
    }
#endif

    dest  = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x]];
    dest2 = ylookup[(dc_yl << hires)] + columnofs[flipwidth[x+1]];
    dest3 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x]];
    dest4 = ylookup[(dc_yl << hires) + 1] + columnofs[flipwidth[x+1]];

    fracstep = dc_iscale;
    frac = dc_texturemid + (dc_yl-centery)*fracstep;

    do
    {
        *dest = tranmap[(*dest<<8)+dc_colormap[dc_source[frac>>FRACBITS]]];
        *dest2 = tranmap[(*dest2<<8)+dc_colormap[dc_source[frac>>FRACBITS]]];
        dest += screenwidth << hires;
        dest2 += screenwidth << hires;

        if (hires)
        {
            *dest3 = tranmap[(*dest3<<8)+dc_colormap[dc_source[frac>>FRACBITS]]];
            *dest4 = tranmap[(*dest4<<8)+dc_colormap[dc_source[frac>>FRACBITS]]];
            dest3 += screenwidth << hires;
            dest4 += screenwidth << hires;
        }

        frac += fracstep;
    } while (count--);
}


//
// R_InitTranslationTables
// Creates the translation tables to map
//  the green color ramp to gray, brown, red.
// Assumes a given structure of the PLAYPAL.
// Could be read from a lump instead.
//
void R_InitTranslationTables (void)
{
    int i;

    // [JN] Set a tint map for shadowed text
    V_LoadTintMap();

    translationtables = Z_Malloc (256*3, PU_STATIC, 0);

    // translate just the 16 green colors
    for (i=0 ; i<256 ; i++)
    {
        if (i >= 0x70 && i<= 0x7f)
        {
            // map green ramp to gray, brown, red
            translationtables[i] = 0x60 + (i&0xf);
            translationtables [i+256] = 0x40 + (i&0xf);
            translationtables [i+512] = 0x20 + (i&0xf);
        }
        else
        {
            // Keep all other colors as is.
            translationtables[i] = translationtables[i+256] 
            = translationtables[i+512] = i;
        }
    }
}


//
// R_DrawSpan 
// With DOOM style restrictions on view orientation,
//  the floors and ceilings consist of horizontal slices
//  or spans with constant z depth.
// However, rotation around the world z axis is possible,
//  thus this mapping, while simpler and faster than
//  perspective correct texture mapping, has to traverse
//  the texture at an angle in all but a few cases.
// In consequence, flats are not stored by column (like walls),
//  and the inner loop has to step in texture space u and v.
//
int	    ds_y; 
int	    ds_x1; 
int	    ds_x2;

lighttable_t*   ds_colormap; 

fixed_t     ds_xfrac; 
fixed_t     ds_yfrac; 
fixed_t     ds_xstep; 
fixed_t     ds_ystep;

// start of a 64*64 tile image 
byte*   ds_source;	

// just for profiling
int dscount;


//
// Draws the actual span.
void R_DrawSpan (void) 
{ 
    // unsigned int position, step;
    byte    *dest;
    int     count;
    int     spot;
    unsigned int xtemp, ytemp;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=screenwidth || (unsigned)ds_y>SCREENHEIGHT)
    {
        I_Error(english_language ?
                "R_DrawSpan: %i to %i at %i" :
                "R_DrawSpan: %i к %i у %i",
                ds_x1,ds_x2,ds_y);
    }
    //	dscount++;
#endif

    // Pack position and step variables into a single 32-bit integer,
    // with x in the top 16 bits and y in the bottom 16 bits.  For
    // each 16-bit part, the top 6 bits are the integer part and the
    // bottom 10 bits are the fractional part of the pixel position.
    // dest = ylookup[ds_y] + columnofs[ds_x1];

    // We do not check for zero spans here?
    count = ds_x2 - ds_x1;

    do
    {
        // Calculate current texture index in u,v.
        // [crispy] fix flats getting more distorted the closer they are to the right
        ytemp = (ds_yfrac >> 10) & 0x0fc0;
        xtemp = (ds_xfrac >> 16) & 0x3f;
        spot = xtemp | ytemp;

        // Lookup pixel from flat texture tile,
        //  re-index using light/colormap.
        //*dest++ = ds_colormap[ds_source[spot]];
        dest = ylookup[ds_y] + columnofs[flipwidth[ds_x1++]];
        *dest = ds_colormap[ds_source[spot]];

        // position += step;
        ds_xfrac += ds_xstep;
        ds_yfrac += ds_ystep;
    } while (count--);
}


//
// Again..
//
void R_DrawSpanLow (void)
{
    unsigned int xtemp, ytemp;
    byte    *dest, *dest2;
    int     count;
    int     spot;

#ifdef RANGECHECK
    if (ds_x2 < ds_x1 || ds_x1<0 || ds_x2>=screenwidth || (unsigned)ds_y>SCREENHEIGHT)
    {
        I_Error(english_language ?
                "R_DrawSpan: %i to %i at %i" :
                "R_DrawSpan: %i к %i у %i",
                ds_x1,ds_x2,ds_y);
    }
#endif

    count = (ds_x2 - ds_x1);

    // Blocky mode, need to multiply by 2.
    ds_x1 <<= 1;
    ds_x2 <<= 1;

    // dest = ylookup[(ds_y << hires)] + columnofs[ds_x1];
    // dest2 = ylookup[(ds_y << hires) + 1] + columnofs[ds_x1];

    do
    {
        // Calculate current texture index in u,v.
        // [crispy] fix flats getting more distorted the closer they are to the right
        ytemp = (ds_yfrac >> 10) & 0x0fc0;
        xtemp = (ds_xfrac >> 16) & 0x3f;
        spot = xtemp | ytemp;

        // Lowres/blocky mode does it twice,
        //  while scale is adjusted appropriately.
        dest = ylookup[(ds_y << hires)] + columnofs[flipwidth[ds_x1]];
        *dest = ds_colormap[ds_source[spot]];
        dest2 = ylookup[(ds_y << hires) + 1] + columnofs[flipwidth[ds_x1++]];
        *dest2 = ds_colormap[ds_source[spot]];
        dest = ylookup[(ds_y << hires)] + columnofs[flipwidth[ds_x1]];
        *dest = ds_colormap[ds_source[spot]];
        dest2 = ylookup[(ds_y << hires) + 1] + columnofs[flipwidth[ds_x1++]];
        *dest2 = ds_colormap[ds_source[spot]];

    // position += step;
    ds_xfrac += ds_xstep;
    ds_yfrac += ds_ystep;

    } while (count--);
}


//
// R_InitBuffer 
// Creats lookup tables that avoid
//  multiplies and other hazzles
//  for getting the framebuffer address
//  of a pixel to draw.
//
void R_InitBuffer (int width, int height) 
{ 
    int i; 

    // Handle resize,
    //  e.g. smaller view windows
    //  with border and/or status bar.
    viewwindowx = (screenwidth-width) >> 1; 

    // Column offset. For windows.
    for (i=0 ; i<width ; i++) 
    columnofs[i] = viewwindowx + i;

    // Samw with base row offset.
    if (width == screenwidth) 
    viewwindowy = 0; 
    else 
    viewwindowy = (SCREENHEIGHT - (gamemission == jaguar ? 
                                   SBARHEIGHT_JAG : SBARHEIGHT)
                                  - height) >> 1; 

    // Preclaculate all row offsets.
    for (i=0 ; i<height ; i++) 
    ylookup[i] = I_VideoBuffer + (i+viewwindowy)*screenwidth; 
}


//
// R_FillBackScreen
// Fills the back screen with a pattern
//  for variable screen sizes
// Also draws a beveled edge.
//
void R_FillBackScreen (void) 
{ 
    byte*       src;
    byte*       dest; 
    int         x;
    int         y; 
    patch_t*    patch;

    // DOOM border patch.
    char *name1 = DEH_String("FLOOR7_2");

    // DOOM II border patch.
    char *name2 = DEH_String("GRNROCK");

    // [JN] Atari Jaguar border patch.
    char *name3 = DEH_String("FLOOR7_1");

    char *name;

    // [JN] Function not used in widescreen rendering.
    if (widescreen)
    {
        return;
    }

    // If we are running full screen, there is no need to do any of this,
    // and the background buffer can be freed if it was previously in use.

    if (scaledviewwidth == screenwidth)
    {
        if (background_buffer != NULL)
        {
            Z_Free(background_buffer);
            background_buffer = NULL;
        }

        return;
    }

    // Allocate the background buffer if necessary

    if (background_buffer == NULL)
    {
        background_buffer = Z_Malloc(screenwidth * (SCREENHEIGHT - (gamemission == jaguar ? 
                                                    SBARHEIGHT_JAG : SBARHEIGHT))
                                                    *sizeof(*background_buffer),
                                                    PU_STATIC, NULL);
    }

    if (gamemode == commercial)
    name = name2;
    else
    name = name1;

    if (gamemission == jaguar)
    name = name3;  

    src = W_CacheLumpName(name, PU_CACHE); 
    dest = background_buffer;

    for (y=0 ; y<SCREENHEIGHT - (gamemission == jaguar ? SBARHEIGHT_JAG : SBARHEIGHT) ; y++) 
    { 
        for (x=0 ; x<screenwidth/64 ; x++) 
        { 
            memcpy (dest, src+((y&63)<<6), 64); 
            dest += 64; 
        } 

        if (screenwidth&63) 
        { 
            memcpy (dest, src+((y&63)<<6), screenwidth&63); 
            dest += (screenwidth&63); 
        } 
    } 

    // Draw screen and bezel; this is done to a separate screen buffer.

    V_UseBuffer(background_buffer);

    patch = W_CacheLumpName(DEH_String("brdr_t"),PU_CACHE);

    for (x=0 ; x<(scaledviewwidth >> hires) ; x+=8)
    V_DrawPatch((viewwindowx >> hires)+x, (viewwindowy >> hires)-8, patch);
    patch = W_CacheLumpName(DEH_String("brdr_b"),PU_CACHE);

    for (x=0 ; x<(scaledviewwidth >> hires) ; x+=8)
    V_DrawPatch((viewwindowx >> hires)+x, (viewwindowy >> hires)+(scaledviewheight >> hires), patch);
    patch = W_CacheLumpName(DEH_String("brdr_l"),PU_CACHE);

    for (y=0 ; y<(scaledviewheight >> hires) ; y+=8)
    V_DrawPatch((viewwindowx >> hires)-8, (viewwindowy >> hires)+y, patch);
    patch = W_CacheLumpName(DEH_String("brdr_r"),PU_CACHE);

    for (y=0 ; y<(scaledviewheight >> hires); y+=8)
    V_DrawPatch((viewwindowx >> hires)+(scaledviewwidth >> hires), (viewwindowy >> hires)+y, patch);

    // Draw beveled edge. 
    V_DrawPatch((viewwindowx >> hires)-8,
                (viewwindowy >> hires)-8,
                W_CacheLumpName(DEH_String("brdr_tl"),PU_CACHE));

    V_DrawPatch((viewwindowx >> hires)+(scaledviewwidth >> hires),
                (viewwindowy >> hires)-8,
                W_CacheLumpName(DEH_String("brdr_tr"),PU_CACHE));

    V_DrawPatch((viewwindowx >> hires)-8,
                (viewwindowy >> hires)+(scaledviewheight >> hires),
                W_CacheLumpName(DEH_String("brdr_bl"),PU_CACHE));

    V_DrawPatch((viewwindowx >> hires)+(scaledviewwidth >> hires),
                (viewwindowy >> hires)+(scaledviewheight >> hires),
                W_CacheLumpName(DEH_String("brdr_br"),PU_CACHE));

    V_RestoreBuffer();
}


//
// Copy a screen buffer.
//
void R_VideoErase (unsigned ofs, int count)
{ 
    // LFB copy.
    // This might not be a good idea if memcpy
    //  is not optiomal, e.g. byte by byte on
    //  a 32bit CPU, as GNU GCC/Linux libc did
    //  at one point.

    if (background_buffer != NULL)
    {
        memcpy(I_VideoBuffer + ofs, background_buffer + ofs, count * sizeof(*I_VideoBuffer));
    }
}


//
// R_DrawViewBorder
// Draws the border around the view
//  for different size windows?
//
void R_DrawViewBorder (void) 
{ 
    int	    top;
    int	    side;
    int	    ofs;
    int	    i; 

    if (scaledviewwidth == screenwidth) 
    return; 

    top = ((SCREENHEIGHT - (gamemission == jaguar ? SBARHEIGHT_JAG : SBARHEIGHT))-scaledviewheight)/2; 
    side = (screenwidth-scaledviewwidth)/2; 

    // copy top and one line of left side 
    R_VideoErase (0, top*screenwidth+side); 

    // copy one line of right side and bottom 
    ofs = (scaledviewheight+top)*screenwidth-side;
    R_VideoErase (ofs, top*screenwidth+side); 

    // copy sides using wraparound 
    ofs = top*screenwidth + screenwidth-side; 
    side <<= 1;

    for (i=1 ; i<scaledviewheight ; i++) 
    { 
        R_VideoErase (ofs, side); 
        ofs += screenwidth; 
    } 

    // ? 
    V_MarkRect (0,0,screenwidth, SCREENHEIGHT - (gamemission == jaguar ? SBARHEIGHT_JAG : SBARHEIGHT)); 
}

