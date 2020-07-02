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
        
    Please note that this is cca. 20 yesrs old code, not particullary something
    to write home about :-))
    
    It's taylored to my own needs, modify it to suit your own. I'm not a professional programmer,
    so this isn't the best code you'll find on the web, you have been warned :-))

    All the bugs are guaranteed to be genuine, and are exclusively mine =)
*/

// MISC. FUNCTIONS

#include <windows.h>

#define _IS_SP   1
#define _IS_DIG  2
#define _IS_UPP  4
#define _IS_LOW  8
#define _IS_HEX  16
#define _IS_CTL  32
#define _IS_PUN  64

const TCHAR mctype[257] = {
        0,

        _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL,_IS_CTL,
        _IS_CTL, _IS_CTL|_IS_SP,
                          _IS_SP|_IS_CTL,
                                   _IS_SP|_IS_CTL,
                                            _IS_SP|_IS_CTL,
                                                     _IS_SP|_IS_CTL,
                                                              _IS_CTL,_IS_CTL,
        _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL,_IS_CTL,
        _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL, _IS_CTL,_IS_CTL,

        _IS_SP,  _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN,_IS_PUN,
        _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN,_IS_PUN,
        _IS_DIG, _IS_DIG, _IS_DIG, _IS_DIG, _IS_DIG, _IS_DIG, _IS_DIG,_IS_DIG,
        _IS_DIG, _IS_DIG, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN,_IS_PUN,

        _IS_PUN, _IS_UPP|_IS_HEX,
                          _IS_HEX|_IS_UPP,
                                   _IS_UPP|_IS_HEX,
                                            _IS_UPP|_IS_HEX,
                                                     _IS_UPP|_IS_HEX,
                                                              _IS_UPP|_IS_HEX,
                                                                      _IS_UPP,
        _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP,_IS_UPP,
        _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP, _IS_UPP,_IS_UPP,
        _IS_UPP, _IS_UPP, _IS_UPP, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN,_IS_PUN,

        _IS_PUN, _IS_LOW|_IS_HEX,
                          _IS_HEX|_IS_LOW,
                                   _IS_LOW|_IS_HEX,
                                            _IS_LOW|_IS_HEX,
                                                     _IS_LOW|_IS_HEX,
                                                              _IS_LOW|_IS_HEX,
                                                                      _IS_LOW,
        _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW,_IS_LOW,
        _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW, _IS_LOW,_IS_LOW,
        _IS_LOW, _IS_LOW, _IS_LOW, _IS_PUN, _IS_PUN, _IS_PUN, _IS_PUN,_IS_CTL
};

#define myisspace(c)   (mctype[(c) + 1] & _IS_SP)
#define myissdigit(c)  (mctype[(c) + 1] & _IS_DIG)


long MyAtol ( TCHAR *nptr )
/************************************************************************************************************/
{
    int     c;
    long    total;
    int     sign;

    while ( myisspace ( ( int ) ( TBYTE ) *nptr ) ) { ++nptr; }

    c = ( int ) ( TBYTE ) *nptr++;
    sign = c;
    
    if ( c == '-' || c == '+' )
        c = ( int ) ( TBYTE ) *nptr++;

    total = 0;

    while ( myissdigit ( c ) )
    {
        total = 10 * total + ( c - '0' );
        c = ( int ) ( TBYTE ) *nptr++;
    }

    if ( sign == '-' )
        return -total;
    else
        return total;
}

void TimeToFileTime (long time, FILETIME *pft)
/************************************************************************************************************/
{
    LONGLONG ll = Int32x32To64 ( time, 10000000 ) + 116444736000000000;
    pft->dwLowDateTime = ( DWORD ) ll;
    pft->dwHighDateTime = ll >> 32;
}
