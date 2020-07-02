/*
    PROCVIEW, a small task manager with PE (portable executable) exploring facilities
    ----------------------------------------------------------------------------------
    Copyright (c) 2002-2020 Adrian Petrila, YO3GFH
    Uses the PEHLP32 library (https://github.com/yo3gfh/pehlp32)
    
    This was initially a lab project and test app for the PEHLP32 library.
    Dugged out recently and dusted off to compile with Pelle's C compiler.
    
                                * * *
                                
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

                                * * *

    Features
    ---------
    
        - create, kill and change process priority
        - list imported/exported modules, along with other PE information
        
    Please note that this is cca. 20 years old code, not particullary something
    to write home about :-))
    
    It's taylored to my own needs, modify it to suit your own. I'm not a professional programmer,
    so this isn't the best code you'll find on the web, you have been warned :-))

    All the bugs are guaranteed to be genuine, and are exclusively mine =)
*/

// STATUSBAR RELATED FUCTIONS

#include <windows.h>
#include <commctrl.h>

void SBSetText ( HWND hStatus, int panel, TCHAR * text )
/************************************************************************************************************/
/* Set text on the selected SB panel                                                                        */
{
    SendMessage ( hStatus, SB_SETTEXT, panel, ( LPARAM )text );
}

void SBSetSimple ( HWND hStatus, BOOL simple )
/************************************************************************************************************/
/* Make a monopanel SB (for displaying menu hints)                                                          */
{
    SendMessage ( hStatus, SB_SIMPLE, ( WPARAM )simple, 0 );
}

HWND MakeStatusBar ( HWND hParent, int sbid, DWORD * panels, int numpanels )
/************************************************************************************************************/
/* Create a nice SB                                                                                         */
{
    HWND    hstatus;
    DWORD   style;

    hstatus = CreateStatusWindow (WS_CHILDWINDOW|WS_VISIBLE|SBS_SIZEGRIP,
                                  NULL,
                                  hParent,
                                  sbid
                                 );

    if ( !hstatus ) { return NULL; }
    
    // optimize redraw
    style = GetClassLong (hstatus, GCL_STYLE);
	
    if ( style )
    {
        style &= ~( CS_HREDRAW | CS_VREDRAW );
        style |= ( CS_BYTEALIGNWINDOW | CS_BYTEALIGNCLIENT );
        SetClassLong ( hstatus, GCL_STYLE, style );
    }

    SendMessage ( hstatus, SB_SETPARTS, ( WPARAM )numpanels, ( LPARAM )panels );
    return hstatus;
}

void MyMenuHelp (HINSTANCE hinst, HWND hStatus, WPARAM wParam)
/************************************************************************************************************/
/* comctl32.dll MenuHelp replacement                                                                        */
{
    TCHAR    temp[256];

    if ( HIWORD ( wParam ) == 0xFFFF )
    {
        SBSetSimple ( hStatus, FALSE );
    }
    else
    {
        SBSetSimple ( hStatus, TRUE );
        if ( HIWORD ( wParam ) & MF_HILITE )
        {
            if ( LoadString ( hinst, LOWORD ( wParam ), temp, sizeof ( temp ) ) )
                SBSetText ( hStatus, 255 | SBT_NOBORDERS, temp );
            else
                SBSetText ( hStatus, 255 | SBT_NOBORDERS, TEXT("") );
        }
    }
}
