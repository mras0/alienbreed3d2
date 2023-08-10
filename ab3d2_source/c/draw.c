#include "draw.h"
#include "system.h"

#include <SDI_compiler.h>
#include <cybergraphics/cybergraphics.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <proto/cybergraphics.h>
#include <proto/exec.h>
#include <string.h>  //memset

/**
 * TODO - dynamically allocate chunky buffers for RTG along with the main display.
 */

#define MULTIPLAYER_SLAVE  ((BYTE)'s')
#define MULTIPLAYER_MASTER ((BYTE)'m')

#define VID_FAST_BUFFER_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT + 4095)
#define PLANESIZE (SCREEN_WIDTH / 8 * SCREEN_HEIGHT)

extern void unLHA(REG(a0, void *dst), REG(d0, const void *src), REG(d1, ULONG length), REG(a1, void *workspace),
                  REG(a2, void *X));

/* Externally declared buffers */
extern ULONG Sys_Workspace_vl[];
extern const UBYTE draw_BorderPacked_vb[];
extern UBYTE draw_BorderChars_vb[];
static UBYTE draw_Border[SCREEN_WIDTH * SCREEN_HEIGHT];

/* These are the fixed with planar glyphs used for in-game messages */
extern UBYTE draw_ScrollChars_vb[];

/* Values used to track changes to the counters */
extern UWORD draw_DisplayEnergyCount_w;
extern UWORD draw_DisplayAmmoCount_w;
extern UWORD draw_LastDisplayAmmoCount_w;
extern UWORD draw_LastDisplayEnergyCount_w;
extern BYTE  Plr_MultiplayerType_b;
extern UBYTE Plr1_TmpGunSelected_b;
extern UBYTE Plr2_TmpGunSelected_b;
extern UWORD Plr1_Weapons_vb[DRAW_NUM_WEAPON_SLOTS];
extern UWORD Plr2_Weapons_vb[DRAW_NUM_WEAPON_SLOTS];
extern UBYTE draw_GlyphSpacing_vb[256];

/* Pointer to planar display */
extern APTR Vid_DisplayScreenPtr_l;

/**
 * Border digits when not low on ammo/health
 *
 * In RTG modes, this contains chunky data. In native modes, this contains planar data reorganised so that all data for
 * each digit is sequentially arranged.
 */
static UBYTE draw_BorderDigitsGood[DRAW_HUD_CHAR_W * DRAW_HUD_CHAR_H * 10];

/**
 * Border digits when low on ammo/health
 *
 * In RTG modes, this contains chunky data. In native modes, this contains planar data reorganised so that all data for
 * each digit is sequentially arranged.
 */
static UBYTE draw_BorderDigitsWarn[DRAW_HUD_CHAR_W * DRAW_HUD_CHAR_H * 10];

/**
 * Border digits for items not yet located
 *
 * In RTG modes, this contains chunky data. In native modes, this contains planar data reorganised so that all data for
 * each digit is sequentially arranged.
 */
static UBYTE draw_BorderDigitsItem[DRAW_HUD_CHAR_SMALL_W * DRAW_HUD_CHAR_SMALL_H * 10];

/**
 * Border digits for items located
 *
 * In RTG modes, this contains chunky data. In native modes, this contains planar data reorganised so that all data for
 * each digit is sequentially arranged.
 */
static UBYTE draw_BorderDigitsItemFound[DRAW_HUD_CHAR_SMALL_W * DRAW_HUD_CHAR_SMALL_H * 10];

/**
 * Border digits for item selected
 *
 * In RTG modes, this contains chunky data. In native modes, this contains planar data reorganised so that all data for
 * each digit is sequentially arranged.
 */
static UBYTE draw_BorderDigitsItemSelected[DRAW_HUD_CHAR_SMALL_W * DRAW_HUD_CHAR_SMALL_H * 10];

static UWORD draw_LastItemList      = 0xFFFF;
static UWORD draw_LastItemSelected  = 0xFFFF;

/* Small buffer for rendering digit displays into */
static UBYTE draw_BorderDigitsBuffer[DRAW_HUD_CHAR_SMALL_H * DRAW_HUD_CHAR_SMALL_W * 10];

static UBYTE *FastBufferAllocPtr;

/**********************************************************************************************************************/

static void draw_PlanarToChunky(UBYTE *chunkyPtr, const PLANEPTR *planePtrs, ULONG numPixels);
static void draw_ValueToDigits(UWORD value, UWORD digits[3]);
static void draw_ConvertBorderDigitsToChunky(UBYTE* chunkyPtr, const UBYTE *planarBasePtr, UWORD width, UWORD height);
static void draw_ReorderBorderDigits(UBYTE* toPlanarPtr, const UBYTE *planarBasePtr, UWORD width, UWORD height);

static void draw_ChunkyGlyph(UBYTE *drawPtr, UWORD drawSpan, UBYTE charGlyph, UBYTE pen);
static void draw_CalculateGlyphSpacing(void);

static void draw_UpdateCounter_RTG(
    APTR bmBaseAddress,
    ULONG bmBytesPerRow,
    UWORD count,
    UWORD limit,
    UWORD xPos,
    UWORD yPos
);

static void draw_UpdateCounter_Planar(
    APTR  planes,
    ULONG bytesPerRow,
    UWORD count,
    UWORD limit,
    UWORD xPos,
    UWORD yPos
);

static void draw_UpdateItems_RTG(
    APTR bmBaseAddress,
    ULONG bmBytesPerRow,
    const UWORD* itemSlots,
    UWORD itemSelected,
    UWORD xPos,
    UWORD yPos
);

static void draw_UpdateItems_Planar(
    APTR  planes,
    ULONG bytesPerRow,
    const UWORD* itemSlots,
    UWORD itemSelected,
    UWORD xPos,
    UWORD yPos
);

#include "draw_inline.h"

/**********************************************************************************************************************/

/**
 * Main initialisation. Get buffers, do any preconversion needed, etc.
 */
BOOL Draw_Init()
{
    if (!(FastBufferAllocPtr = AllocVec(VID_FAST_BUFFER_SIZE, MEMF_ANY))) {
        goto fail;
    }

    Vid_FastBufferPtr_l = (UBYTE *)(((ULONG)FastBufferAllocPtr + 15) & ~15);

    void (*border_convert)(UBYTE* to, const UBYTE *from, UWORD width, UWORD height);

    if (Vid_isRTG) {
        BitPlanes planes;
        unLHA(Vid_FastBufferPtr_l, draw_BorderPacked_vb, 0, Sys_Workspace_vl, NULL);

        for (int p = 0; p < SCREEN_DEPTH; ++p) {
            planes[p] = Vid_FastBufferPtr_l + PLANESIZE * p;
        };

        /* The image we have has a fixed size */
        draw_PlanarToChunky(draw_Border, planes, SCREEN_WIDTH * SCREEN_HEIGHT);

        border_convert = draw_ConvertBorderDigitsToChunky;
    } else {
        border_convert = draw_ReorderBorderDigits;
    }

    /* Convert the "low ammo/health" counter digits */
    border_convert(
        draw_BorderDigitsWarn,
        draw_BorderChars_vb + 15 * DRAW_HUD_CHAR_W * 10,
        DRAW_HUD_CHAR_W,
        DRAW_HUD_CHAR_H
    );

    /* Convert the normal counter digits */
    border_convert(
        draw_BorderDigitsGood,
        draw_BorderChars_vb + 15 * DRAW_HUD_CHAR_W * 10 + DRAW_HUD_CHAR_H * DRAW_HUD_CHAR_W * 10,
        DRAW_HUD_CHAR_W,
        DRAW_HUD_CHAR_H
    );

    /* Convert the unavailable item digits */
    border_convert(
        draw_BorderDigitsItem,
        draw_BorderChars_vb,
        DRAW_HUD_CHAR_SMALL_W,
        DRAW_HUD_CHAR_SMALL_H
    );

    /* Convert the available item digits */
    border_convert(
        draw_BorderDigitsItemFound,
        draw_BorderChars_vb + DRAW_HUD_CHAR_SMALL_H * 10 * DRAW_HUD_CHAR_SMALL_W,
        DRAW_HUD_CHAR_SMALL_W,
        DRAW_HUD_CHAR_SMALL_H
    );

    /* Convert the selected item digits */
    border_convert(
        draw_BorderDigitsItemSelected,
        draw_BorderChars_vb + DRAW_HUD_CHAR_SMALL_H * 10 * DRAW_HUD_CHAR_SMALL_W * 2,
        DRAW_HUD_CHAR_SMALL_W,
        DRAW_HUD_CHAR_SMALL_H
    );

    draw_CalculateGlyphSpacing();

    draw_ResetHUDCounters();
    return TRUE;

fail:
    Draw_Shutdown();
    return FALSE;
}

/**********************************************************************************************************************/

/**
 * All done.
 */
void Draw_Shutdown()
{
    if (FastBufferAllocPtr) {
        FreeVec(FastBufferAllocPtr);
        FastBufferAllocPtr = NULL;
    }
}

/**********************************************************************************************************************/

/**
 * Re initialise the game display, clear out the previous view, reset HUD, etc.
 */
void Draw_ResetGameDisplay()
{
    /* Retrigger the counters */
    draw_ResetHUDCounters();
    if (!Vid_isRTG) {
        unLHA(Vid_Screen1Ptr_l, draw_BorderPacked_vb, 0, Sys_Workspace_vl, NULL);
        unLHA(Vid_Screen2Ptr_l, draw_BorderPacked_vb, 0, Sys_Workspace_vl, NULL);
        Draw_UpdateBorder_Planar();
    } else {
        LOCAL_CYBERGFX();

        memset(Vid_FastBufferPtr_l, 0, SCREEN_WIDTH * SCREEN_HEIGHT);

        ULONG bmBytesPerRow;
        APTR bmBaseAddress;

        APTR bmHandle = LockBitMapTags(Vid_MainScreen_l->ViewPort.RasInfo->BitMap,
                                       LBMI_BYTESPERROW, (ULONG)&bmBytesPerRow,
                                       LBMI_BASEADDRESS, (ULONG)&bmBaseAddress,
                                       TAG_DONE);
        if (bmHandle) {
            const UBYTE *src = draw_Border;
            WORD height = Vid_ScreenHeight < SCREEN_HEIGHT ? Vid_ScreenHeight : SCREEN_HEIGHT;
            src += (SCREEN_HEIGHT - height) * SCREEN_WIDTH;

            if (bmBytesPerRow == SCREEN_WIDTH) {
                memcpy(bmBaseAddress, src, SCREEN_WIDTH * height);
            } else {
                for (WORD y = 0; y < height; ++y) {
                    memcpy(bmBaseAddress, src, SCREEN_WIDTH);
                    bmBaseAddress += bmBytesPerRow;
                    src += SCREEN_WIDTH;
                }
            }
            Draw_UpdateBorder_RTG(bmBaseAddress, bmBytesPerRow);
            UnLockBitMap(bmHandle);
        }
    }
}

/**********************************************************************************************************************/

static void draw_ChunkyGlyph(UBYTE *drawPtr, UWORD drawSpan, UBYTE charGlyph, UBYTE pen);

/**
 * Draw a length limited, null terminated string of fixed glyphs at a given coordinate.
 */
const char* Draw_ChunkyText(
    UBYTE *drawPtr,
    UWORD drawSpan,
    UWORD maxLen,
    const char *textPtr,
    UWORD xPos,
    UWORD yPos,
    UBYTE pen
) {
    drawPtr += drawSpan * yPos + xPos;
    UBYTE charGlyph;
    while ( (charGlyph = (UBYTE)*textPtr++) && maxLen-- > 0 ) {
        /* Skip over all non-printing or blank. Assume ECMA-94 Latin 1 8-bit for Amiga 3.x */
        if ( (charGlyph > 0x20 && charGlyph < 0x7F) || charGlyph > 0xA0) {
            draw_ChunkyGlyph(drawPtr, drawSpan, charGlyph, pen);
        }
        drawPtr += DRAW_MSG_CHAR_W;
    }
    return charGlyph ? textPtr : (const char*)NULL;
}

/**********************************************************************************************************************/

/**
 * Draw a length limited, null terminated string of proportional glyphs at a given coordinate.
 */
const char* Draw_ChunkyTextProp(
    UBYTE *drawPtr,
    UWORD drawSpan,
    UWORD maxLen,
    const char *textPtr,
    UWORD xPos,
    UWORD yPos,
    UBYTE pen
) {
    drawPtr += drawSpan * yPos + xPos;
    UBYTE charGlyph;
    while ( (charGlyph = (UBYTE)*textPtr++) && maxLen-- > 0 ) {
        UBYTE glyphSpacing = draw_GlyphSpacing_vb[charGlyph];
        /* Skip over all non-printing or blank. Assume ECMA-94 Latin 1 8-bit for Amiga 3.x */
        if ( (charGlyph > 0x20 && charGlyph < 0x7F) || charGlyph > 0xA0) {
            draw_ChunkyGlyph(drawPtr - (glyphSpacing & 0xF), drawSpan, charGlyph, pen);
        }
        drawPtr += glyphSpacing >> 4;
    }
    return charGlyph ? textPtr : (const char*)NULL;
}

/**********************************************************************************************************************/

/**
 * Calculate the pixel width of a string (up to maxLen or null, whichever comes first) when using proportional
 * rendering.
 */
ULONG Draw_CalcPropWidth(const char *textPtr, UWORD maxLen) {
    ULONG width = 0;
    UBYTE charGlyph;
    while ( (charGlyph = (UBYTE)*textPtr++) && maxLen-- > 0 ) {
        width += draw_GlyphSpacing_vb[charGlyph] >> 4;
    }
    return width;
}

/**********************************************************************************************************************/

/**
 * Draw a line of proportional text on the level intro screen
 */
void Draw_LineOfText(REG(a0, const char *ptr), REG(a1, APTR screenPointer), REG(d0,  ULONG xxxx))
{

}

/**********************************************************************************************************************/

/**
 * Called during Vid_Present on the RTG codepath to update the border within the main bitmap lock. Also called when
 * resizing the display.
 */
void Draw_UpdateBorder_RTG(APTR bmBaseAddress, ULONG bmBytesPerRow)
{
    INIT_ITEMS();

    if (itemSelected != draw_LastItemSelected || itemList != draw_LastItemList) {
        draw_LastItemSelected = itemSelected;
        draw_LastItemList     = itemList;

        /* Inventory */
        draw_UpdateItems_RTG(
            bmBaseAddress,
            bmBytesPerRow,
            itemSlots,
            itemSelected,
            draw_ScreenXPos(DRAW_HUD_ITEM_SLOTS_X),
            draw_ScreenYPos(DRAW_HUD_ITEM_SLOTS_Y)
        );
    }
    /* Ammunition */
    if (draw_LastDisplayAmmoCount_w != draw_DisplayAmmoCount_w) {
        draw_LastDisplayAmmoCount_w = draw_DisplayAmmoCount_w;
        draw_UpdateCounter_RTG(
            bmBaseAddress,
            bmBytesPerRow,
            draw_DisplayAmmoCount_w,
            LOW_AMMO_COUNT_WARN_LIMIT,
            draw_ScreenXPos(DRAW_HUD_AMMO_COUNT_X),
            draw_ScreenYPos(DRAW_HUD_AMMO_COUNT_Y)
        );
    }

    /* Energy */
    if (draw_LastDisplayEnergyCount_w != draw_DisplayEnergyCount_w) {
        draw_LastDisplayEnergyCount_w = draw_DisplayEnergyCount_w;
        draw_UpdateCounter_RTG(
            bmBaseAddress,
            bmBytesPerRow,
            draw_DisplayEnergyCount_w,
            LOW_ENERGY_COUNT_WARN_LIMIT,
            draw_ScreenXPos(DRAW_HUD_ENERGY_COUNT_X),
            draw_ScreenYPos(DRAW_HUD_ENERGY_COUNT_Y)
        );
    }
}

/**********************************************************************************************************************/

extern UWORD Vid_ScreenBufferIndex_w;

void Draw_UpdateBorder_Planar(void)
{
    INIT_ITEMS();

    /* TODO - Determine the correct render target and rely on the pointer. For now, we ignore it and render to both */

    if (itemSelected != draw_LastItemSelected || itemList != draw_LastItemList) {
        draw_LastItemSelected = itemSelected;
        draw_LastItemList     = itemList;

        /* Inventory */
        draw_UpdateItems_Planar(
            Vid_DisplayScreenPtr_l,
            SCREEN_WIDTH / SCREEN_DEPTH,
            itemSlots,
            itemSelected,
            draw_ScreenXPos(DRAW_HUD_ITEM_SLOTS_X),
            draw_ScreenYPos(DRAW_HUD_ITEM_SLOTS_Y)
        );
    }
    /* Ammunition */
    if (draw_LastDisplayAmmoCount_w != draw_DisplayAmmoCount_w) {
        draw_LastDisplayAmmoCount_w = draw_DisplayAmmoCount_w;
        draw_UpdateCounter_Planar(
            Vid_DisplayScreenPtr_l,
            SCREEN_WIDTH / SCREEN_DEPTH,
            draw_DisplayAmmoCount_w,
            LOW_AMMO_COUNT_WARN_LIMIT,
            draw_ScreenXPos(DRAW_HUD_AMMO_COUNT_X),
            draw_ScreenYPos(DRAW_HUD_AMMO_COUNT_Y)
        );
    }

    /* Energy */
    if (draw_LastDisplayEnergyCount_w != draw_DisplayEnergyCount_w) {
        draw_LastDisplayEnergyCount_w = draw_DisplayEnergyCount_w;
        draw_UpdateCounter_Planar(
            Vid_DisplayScreenPtr_l,
            SCREEN_WIDTH / SCREEN_DEPTH,
            draw_DisplayEnergyCount_w,
            LOW_ENERGY_COUNT_WARN_LIMIT,
            draw_ScreenXPos(DRAW_HUD_ENERGY_COUNT_X),
            draw_ScreenYPos(DRAW_HUD_ENERGY_COUNT_Y)
        );
    }
}

/**********************************************************************************************************************/

/**
 * Render a counter single digit
 */
static void draw_RenderCounterDigit_RTG(UBYTE *drawPtr, const UBYTE *glyphPtr, UWORD digit, UWORD span) {

#ifdef RTG_LONG_ALIGNED
    const ULONG *digitPtr = (ULONG*)&glyphPtr[digit * DRAW_HUD_CHAR_W * DRAW_HUD_CHAR_H];
    ULONG *drawPtr32 = (ULONG*)drawPtr;
    span >>= 2;
    for (UWORD y = 0; y < DRAW_HUD_CHAR_H; ++y) {
        for (UWORD x = 0; x < DRAW_HUD_CHAR_W / sizeof(ULONG); ++x) {
            drawPtr32[x] = *digitPtr++;
        }
        drawPtr32 += span;
    }
#else
    const UBYTE *digitPtr = &glyphPtr[digit * DRAW_HUD_CHAR_W * DRAW_HUD_CHAR_H];
    for (UWORD y = 0; y < DRAW_HUD_CHAR_H; ++y) {
        for (UWORD x = 0; x < DRAW_HUD_CHAR_W; ++x) {
            drawPtr[x] = *digitPtr++;
        }
        drawPtr += span;
    }
#endif
}

/**********************************************************************************************************************/

/**
 * Render a counter (3-digit) into
 */
static void draw_UpdateCounter_RTG(APTR bmBaseAddress, ULONG bmBytesPerRow, UWORD count, UWORD limit, UWORD xPos, UWORD yPos)
{
    UWORD digits[3];
    draw_ValueToDigits(count, digits);

    const UBYTE *glyphPtr = count > limit ?
        draw_BorderDigitsGood :
        draw_BorderDigitsWarn;

    /* Render the digits into the mini buffer */
    UBYTE* bufferPtr = draw_BorderDigitsBuffer;
    for (UWORD d = 0; d < 3; ++d, bufferPtr += DRAW_HUD_CHAR_W) {
        draw_RenderCounterDigit_RTG(bufferPtr, glyphPtr, digits[d], DRAW_COUNT_W);
    }

    /* Copy the mini buffer to the bitmap */
    UBYTE* drawPtr = ((UBYTE*)bmBaseAddress) + xPos + yPos * bmBytesPerRow;

    bufferPtr = draw_BorderDigitsBuffer;
    for (UWORD y = 0; y < DRAW_HUD_CHAR_H; ++y, drawPtr += bmBytesPerRow, bufferPtr += DRAW_COUNT_W) {

#ifdef RTG_LONG_ALIGNED
        CopyMemQuick(bufferPtr, drawPtr, DRAW_COUNT_W);
#else
        memcpy(drawPtr, bufferPtr, DRAW_COUNT_W);
#endif
    }
}

/**********************************************************************************************************************/

/**
 * Render a counter single digit
 */
static void draw_RenderItemDigit_RTG(UBYTE *drawPtr, const UBYTE *glyphPtr, UWORD digit, UWORD span) {

#ifdef RTG_LONG_ALIGNED
    const ULONG *digitPtr = (ULONG*)&glyphPtr[digit * DRAW_HUD_CHAR_SMALL_W * DRAW_HUD_CHAR_SMALL_H];
    ULONG *drawPtr32 = (ULONG*)drawPtr;
    span >>= 2;
    for (UWORD y = 0; y < DRAW_HUD_CHAR_SMALL_H; ++y) {
        for (UWORD x = 0; x < DRAW_HUD_CHAR_SMALL_W / sizeof(ULONG); ++x) {
            drawPtr32[x] = *digitPtr++;
        }
        drawPtr32 += span;
    }
#else
    const UBYTE *digitPtr = &glyphPtr[digit * DRAW_HUD_CHAR_SMALL_W * DRAW_HUD_CHAR_SMALL_H];
    for (UWORD y = 0; y < DRAW_HUD_CHAR_SMALL_H; ++y) {
        for (UWORD x = 0; x < DRAW_HUD_CHAR_SMALL_W; ++x) {
            drawPtr[x] = *digitPtr++;
        }
        drawPtr += span;
    }
#endif
}
static void draw_UpdateItems_RTG(APTR bmBaseAddress, ULONG bmBytesPerRow, const UWORD* itemSlots, UWORD itemSelected, UWORD xPos, UWORD yPos) {
    UBYTE* drawPtr = ((UBYTE*)bmBaseAddress) + xPos + yPos * bmBytesPerRow;

    /* Render into the minibuffer */
    UBYTE *bufferPtr = draw_BorderDigitsBuffer;
    for (UWORD i = 0; i < DRAW_NUM_WEAPON_SLOTS; ++i, bufferPtr += DRAW_HUD_CHAR_SMALL_W) {
        const UBYTE *glyphPtr = itemSelected == i ?
            draw_BorderDigitsItemSelected :
            itemSlots[i] ?
                draw_BorderDigitsItemFound :
                draw_BorderDigitsItem;

        draw_RenderItemDigit_RTG(bufferPtr, glyphPtr, i, DRAW_HUD_CHAR_SMALL_W * 10);
    }

    /* Copy the mini buffer to the bitmap */
    bufferPtr = draw_BorderDigitsBuffer;
    for (UWORD i = 0; i < DRAW_HUD_CHAR_SMALL_H; ++i, drawPtr += bmBytesPerRow, bufferPtr += DRAW_HUD_CHAR_SMALL_W * 10) {

#ifdef RTG_LONG_ALIGNED
        CopyMemQuick(bufferPtr, drawPtr, DRAW_HUD_CHAR_SMALL_W * 10);
#else
        memcpy(drawPtr, bufferPtr, DRAW_HUD_CHAR_SMALL_W * 10);
#endif
    }

}

/**********************************************************************************************************************/

#define PLANE_OFFSET(n) ((n) * SCREEN_WIDTH * SCREEN_HEIGHT / SCREEN_DEPTH)

/**
 * Render a single item digit
 */
static void draw_RenderItemDigit_Planar(ULONG offset, const UBYTE *glyphPtr, UWORD digit, ULONG bytesPerRow) {
    const UBYTE *digitPtrBase = &glyphPtr[digit * DRAW_HUD_CHAR_SMALL_W * DRAW_HUD_CHAR_SMALL_H];
    PLANEPTR planes[2] = {
        Vid_Screen1Ptr_l,
        Vid_Screen2Ptr_l
    };

    for (UWORD p = 0; p < 2; ++p) {
        PLANEPTR drawPtr = planes[p] + offset;
        const UBYTE *digitPtr = digitPtrBase;
        for (UWORD y = 0; y < DRAW_HUD_CHAR_SMALL_H; ++y) {
            drawPtr[PLANE_OFFSET(0)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(1)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(2)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(3)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(4)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(5)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(6)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(7)] = *digitPtr++;
            drawPtr += bytesPerRow;
        }
    }
}

/**********************************************************************************************************************/

static void draw_UpdateItems_Planar(APTR planes, ULONG bytesPerRow, const UWORD* itemSlots, UWORD itemSelected, UWORD xPos, UWORD yPos)
{
    /* TODO - fix up the buffer selection so that input planes is the required render target */

    ULONG offset = ((xPos + yPos * SCREEN_WIDTH) >> SCREEN_DEPTH_EXP);

    for (UWORD i = 0; i < DRAW_NUM_WEAPON_SLOTS; ++i, offset += (DRAW_HUD_CHAR_SMALL_W >> SCREEN_DEPTH_EXP)) {
        const UBYTE *glyphPtr = itemSelected == i ?
            draw_BorderDigitsItemSelected :
            itemSlots[i] ?
                draw_BorderDigitsItemFound :
                draw_BorderDigitsItem;

        draw_RenderItemDigit_Planar(offset, glyphPtr, i, bytesPerRow);
   }
}

/**********************************************************************************************************************/

/**
 * Render a single counter digit
 */
static void draw_RenderCounterDigit_Planar(ULONG offset, const UBYTE *glyphPtr, UWORD digit, ULONG bytesPerRow) {

    const UBYTE *digitPtrBase = &glyphPtr[digit * DRAW_HUD_CHAR_W * DRAW_HUD_CHAR_H];
    PLANEPTR planes[2] = {
        Vid_Screen1Ptr_l,
        Vid_Screen2Ptr_l
    };

    for (UWORD p = 0; p < 2; ++p) {
        PLANEPTR drawPtr = planes[p] + offset;
        const UBYTE *digitPtr = digitPtrBase;
        for (UWORD y = 0; y < DRAW_HUD_CHAR_H; ++y) {
            drawPtr[PLANE_OFFSET(0)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(1)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(2)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(3)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(4)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(5)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(6)] = *digitPtr++;
            drawPtr[PLANE_OFFSET(7)] = *digitPtr++;
            drawPtr += bytesPerRow;
        }
    }
}

static void draw_UpdateCounter_Planar(APTR planes, ULONG bytesPerRow, UWORD count, UWORD limit, UWORD xPos, UWORD yPos)
{
    UWORD digits[3];
    draw_ValueToDigits(count, digits);

    const UBYTE *glyphPtr = count > limit ?
        draw_BorderDigitsGood :
        draw_BorderDigitsWarn;

    ULONG offset = ((xPos + yPos * SCREEN_WIDTH) >> SCREEN_DEPTH_EXP);

    for (UWORD d = 0; d < 3; ++d, offset += DRAW_HUD_CHAR_W >> SCREEN_DEPTH_EXP) {
        draw_RenderCounterDigit_Planar(offset, glyphPtr, digits[d], bytesPerRow);
    }
}

/**********************************************************************************************************************/

/* Lower level utility functions */

/**
 * Decimate a display value into 3 digits for the display counter.
 */
static void draw_ValueToDigits(UWORD value, UWORD digits[3]) {
    if (value > DISPLAY_COUNT_LIMIT) {
        value = DISPLAY_COUNT_LIMIT;
    }
    // Simple, ugly, decimation.
    digits[2] = value % 10; value /= 10;
    digits[1] = value % 10; value /= 10;
    digits[0] = value;
}

/**********************************************************************************************************************/

/**
 * Draw a glyph into the target buffer, filling only the set pixels with the desired pen. There are probably lots
 * of ways this can be optimised.
 */
static void draw_ChunkyGlyph(UBYTE *drawPtr, UWORD drawSpan, UBYTE charGlyph, UBYTE pen)
{
    UBYTE *planarPtr = &draw_ScrollChars_vb[(UWORD)charGlyph << 3];
    for (UWORD row = 0; row < DRAW_MSG_CHAR_H; ++row) {
        UBYTE plane = *planarPtr++;
            if (plane) {
            if (plane & 128) drawPtr[0] = pen;
            if (plane & 64)  drawPtr[1] = pen;
            if (plane & 32)  drawPtr[2] = pen;
            if (plane & 16)  drawPtr[3] = pen;
            if (plane & 8)   drawPtr[4] = pen;
            if (plane & 4)   drawPtr[5] = pen;
            if (plane & 2)   drawPtr[6] = pen;
            if (plane & 1)   drawPtr[7] = pen;
        }
        drawPtr += drawSpan;
    }
}


/**
 * Very simple algorithm to scan the fixed with 8x8 font and determine some offset/width properties based on the glyph
 * bit patterns. We scan and populate draw_GlyphSpacing_vb with that data so that we can render the glyphs fixed or
 * proportionally.
 *
 * The algorithm isn't particularly optimised but we only do it once.
 */
static void draw_CalculateGlyphSpacing() {
    UBYTE *glyphPtr = draw_ScrollChars_vb;
    for (UWORD i = 0; i < 256; ++i, glyphPtr += 8) {
        UBYTE left  = 0;
        UBYTE width = 4;

        /* OR together the 8 planes to get a single value that has the largest width set */
        UBYTE mask  = glyphPtr[0] | glyphPtr[1] | glyphPtr[2] | glyphPtr[3] |
                      glyphPtr[4] | glyphPtr[5] | glyphPtr[6] | glyphPtr[7];

        /* If the mask is zero, it means the glyph is empty. Assume the same space as the space glyph */
        if (mask) {
            UBYTE tmp = mask;
            while (!(tmp & 0x80)) {
                ++left;
                tmp <<= 1;
            }
            tmp = 0;
            while (!(mask & 0x01)) {
                ++tmp;
                mask >>= 1;
            }
            width = 9 - left - tmp;
        }
        draw_GlyphSpacing_vb[i] = width << 4 | left;
    }
}

/**********************************************************************************************************************/

/**
 * Converts the border digits used for ammo/health from their initial planar representation to a chunky one
 */
static void draw_ConvertBorderDigitsToChunky(UBYTE* chunkyPtr, const UBYTE *planarBasePtr, UWORD width, UWORD height) {
    BitPlanes planes;
    const UBYTE *base_digit = planarBasePtr;
    UBYTE *out_digit  = chunkyPtr;
    for (UWORD d = 0; d < 10; ++d) {
        const UBYTE *digit = base_digit + d;
        for (UWORD p = 0; p < 8; ++p) {
            planes[p] = (PLANEPTR)(digit + p * 10);
        }

        for (UWORD y = 0; y <  height; ++y) {
            draw_PlanarToChunky(out_digit, planes, width);
            for (UWORD p = 0; p < 8; ++p) {
                planes[p] += width * 10;
            }
            out_digit += width;
        }
    }
}

/**
 * Reorganises the planar data so that the data for all digits are stored consecutively. Each digit is stored as
 * 8 bytes per row, each byte representing a single bitplane.
 */
static void draw_ReorderBorderDigits(UBYTE* toPlanarPtr, const UBYTE *planarBasePtr, UWORD width, UWORD height) {
    BitPlanes planes;
    const UBYTE *base_digit = planarBasePtr;
    for (UWORD d = 0; d < 10; ++d) {
        const UBYTE *digit = base_digit + d;
        for (UWORD p = 0; p < 8; ++p) {
            planes[p] = (PLANEPTR)(digit + p * 10);
        }

        for (UWORD y = 0; y <  height; ++y) {
            for (UWORD p = 0; p < 8; ++p) {
                *toPlanarPtr++ = *planes[p];
                planes[p] += width * 10;
            }
        }
    }
}

/**********************************************************************************************************************/

/**
 * Convert planar graphics to chunky
 */
static void draw_PlanarToChunky(UBYTE *chunkyPtr, const PLANEPTR *planePtrs, ULONG numPixels)
{
    BitPlanes pptr;
    for (UWORD p = 0; p < 8; ++p) {
        pptr[p] = planePtrs[p];
    }

    for (ULONG x = 0; x < numPixels / 8; ++x) {
        for (BYTE p = 0; p < 8; ++p) {
            chunkyPtr[p] = 0;
            UBYTE bit = 1 << (7 - p);
            for (BYTE b = 0; b < 8; ++b) {
                if (*pptr[b] & bit) {
                    chunkyPtr[p] |= 1 << b;
                }
            }
        }
        chunkyPtr += 8;
        for (UWORD p = 0; p < 8; ++p) {
            pptr[p]++;
        }
    }
}
