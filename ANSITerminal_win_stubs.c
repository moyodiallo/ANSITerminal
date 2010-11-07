/* ANSITerminal; windows API calls

   Allow colors, cursor movements, erasing,... under Unix and DOS shells.
   *********************************************************************

   Copyright 2010 by Christophe Troestler <Christophe.Troestler@umons.ac.be>
   http://math.umons.ac.be/an/software/

   Copyright 2010 by Vincent Hugot
   vincent.hugot@gmail.com
   www.vincent-hugot.com

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   version 2.1 as published by the Free Software Foundation, with the
   special exception on linking described in file LICENSE.

   This library is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the file
   LICENSE for more details.
*/

#include <stdio.h>
#include <caml/mlvalues.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <windows.h>

#include "io.h"

/* From otherlibs/win32unix/channels.c */
extern long _get_osfhandle(int);
#define HANDLE_OF_CHAN(vchan) ((HANDLE) _get_osfhandle(Channel(vchan)->fd))

static HANDLE hStdout;
static CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
static WORD wOldColorAttrs;

// Get handles etc. Call once before doing anything else
// returns 0 iff no problem
CAMLexport
value ANSITerminal_init(value unit)
{
  hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hStdout == INVALID_HANDLE_VALUE)
  {
    //MessageBox(NULL, TEXT("GetStdHandle"), TEXT("Console Error"), MB_OK);
    return Val_int(666);
  }

  // Save the current text colors.
  if (! GetConsoleScreenBufferInfo(hStdout, &csbiInfo))
  {
    /* MessageBox(NULL, TEXT("GetConsoleScreenBufferInfo"),  */
    /*            TEXT("Console Error"), MB_OK);  */
    return Val_int(777);
  }
  wOldColorAttrs = csbiInfo.wAttributes;
  // everything is OK
  return Val_int(0);
}


CAMLexport
value ANSITerminal_set_style(value vchan, value vcode)
{
  /* noalloc */
  HANDLE h = HANDLE_OF_CHAN(vchan);
  int code = Int_val(vcode);

  if (! SetConsoleTextAttribute(h, code) )
  {
    /* MessageBox(NULL, TEXT("SetConsoleTextAttribute"), */
    /*            TEXT("Console Error"), MB_OK); */
    printf("{%d}", code);
    return Val_int(1 + code);
  }
  return Val_int(0);
}


// Restore the original text colors.
CAMLexport
value ANSITerminal_unset_style(value vchan)
{
  /* noalloc */
  HANDLE h = HANDLE_OF_CHAN(vchan);

  if (! SetConsoleTextAttribute(hStdout, wOldColorAttrs) )
    {
      /* MessageBox(NULL, TEXT("SetConsoleTextAttribute"), */
      /*            TEXT("Console Error"), MB_OK); */
      return Val_int(888);
    }
  return Val_int(0);
}


CAMLexport
value ANSITerminal_pos(value vunit)
{
  CAMLparam1(vunit);
  CAMLlocal1(vpos);
  SMALL_RECT w;
  SHORT x, y;

  GetConsoleScreenBufferInfo(hStdout, &csbiInfo);
  w = csbiInfo.srWindow;
  x = csbiInfo.dwCursorPosition.X - w.Left + 1;
  y = csbiInfo.dwCursorPosition.Y - w.Top + 1;

  vpos = caml_alloc_tuple(2);
  Store_field(vpos, 0, Val_int(x));
  Store_field(vpos, 1, Val_int(y));
  CAMLreturn(vpos);
}

CAMLexport
value ANSITerminal_size(value vunit)
{
  CAMLparam1(vunit);
  CAMLlocal1(vsize);
  SMALL_RECT w;

  /* Update the global var as the terminal may have been be resized */
  GetConsoleScreenBufferInfo(hStdout, &csbiInfo);
  w = csbiInfo.srWindow;

  vsize = caml_alloc_tuple(2);
  Store_field(vsize, 0, Val_int(w.Right - w.Left + 1));
  Store_field(vsize, 1, Val_int(w.Bottom - w.Top + 1));

  CAMLreturn(vsize);
}

CAMLexport
value ANSITerminal_resize(value vx, value vy)
{
  /* noalloc */
  COORD dwSize;
  dwSize.X = Int_val(vx);
  dwSize.Y = Int_val(vy);
  SetConsoleScreenBufferSize(hStdout, dwSize);
  return Val_unit;
}


CAMLexport
value ANSITerminal_SetCursorPosition(value vx, value vy)
{
  COORD dwCursorPosition;
  /* The top lefmost coordinate is (1,1) for ANSITerminal while it is
   * (0,0) for windows. */
  GetConsoleScreenBufferInfo(hStdout, &csbiInfo);
  dwCursorPosition.X = Int_val(vx) - 1 + csbiInfo.srWindow.Left;
  dwCursorPosition.Y = Int_val(vy) - 1 + csbiInfo.srWindow.Top;
  SetConsoleCursorPosition(hStdout, dwCursorPosition);
  return Val_unit;
}

CAMLexport
value ANSITerminal_FillConsoleOutputCharacter(
  value vchan, value vc, value vlen, value vx, value vy)
{
  CAMLparam1(vchan);
  HANDLE h = HANDLE_OF_CHAN(vchan);
  int NumberOfCharsWritten;
  COORD dwWriteCoord;

  GetConsoleScreenBufferInfo(hStdout, &csbiInfo);
  dwWriteCoord.X = Int_val(vx) - 1 + csbiInfo.srWindow.Left;
  dwWriteCoord.Y = Int_val(vy) - 1 + csbiInfo.srWindow.Top;
  FillConsoleOutputCharacter(h, Int_val(vc), Int_val(vlen), dwWriteCoord,
                             &NumberOfCharsWritten);
  CAMLreturn(Val_int(NumberOfCharsWritten));
}


CAMLexport
value ANSITerminal_Scroll(value vx)
{
  /* noalloc */
  INT x = Int_val(vx);
  SMALL_RECT srctScrollRect, srctClipRect;
  CHAR_INFO chiFill;
  COORD coordDest;

  srctScrollRect.Left = 0;
  srctScrollRect.Top = 1;
  srctScrollRect.Right = csbiInfo.dwSize.X - x;
  srctScrollRect.Bottom = csbiInfo.dwSize.Y - x;

  // The destination for the scroll rectangle is one row up.
  coordDest.X = 0;
  coordDest.Y = 0;

  // The clipping rectangle is the same as the scrolling rectangle.
  // The destination row is left unchanged.
  srctClipRect = srctScrollRect;

  // Set the fill character and attributes.
  chiFill.Attributes = FOREGROUND_RED|FOREGROUND_INTENSITY;
  chiFill.Char.AsciiChar = (char) ' ';

  ScrollConsoleScreenBuffer(
    hStdout,         // screen buffer handle
    &srctScrollRect, // scrolling rectangle
    &srctClipRect,   // clipping rectangle
    coordDest,       // top left destination cell
    &chiFill);       // fill character and color
  return Val_unit;
}
