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

// LISTVIEW RELATED FUNCTIONS

#include <windows.h>
#include <commctrl.h>
#include "resource.h"

typedef struct
{
    TCHAR * text;
    int    width;
    int    flags;
}   COLDATA;


HWND MakeListView ( HINSTANCE hinst, HWND hParent )
/*******************************************************************************************************************/
/* Create a listview                                                                                               */
{
    HWND    hlist;
    hlist = CreateWindowEx ( WS_EX_CLIENTEDGE, TEXT("SysListView32"), NULL,
                            WS_CHILD|WS_VISIBLE|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,
                            0, 0, 0, 0, hParent, ( HMENU )IDC_PROCLIST, hinst, NULL );
    
    if ( !hlist ) { return NULL; }

    ListView_SetExtendedListViewStyle ( hlist, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP );
    return hlist;
}

HIMAGELIST InitImgList ( HINSTANCE hinst )
/*******************************************************************************************************************/
/* Create a imagelist, 16x16, color                                                                                */
{
    HIMAGELIST  img;

    img = ImageList_Create ( 16, 16, ILC_COLOR | ILC_MASK, 0, 4 );

    if ( img == NULL ) { return NULL; }

    if ( ImageList_AddIcon ( img, LoadIcon ( hinst, MAKEINTRESOURCE ( IDI_IDLE ) ) ) == -1 ) { return NULL; }
    if ( ImageList_AddIcon ( img, LoadIcon ( hinst, MAKEINTRESOURCE ( IDI_NOR ) ) ) == -1 )  { return NULL; }
    if ( ImageList_AddIcon ( img, LoadIcon ( hinst, MAKEINTRESOURCE ( IDI_HI ) ) ) == -1 )   { return NULL; }
    if ( ImageList_AddIcon ( img, LoadIcon ( hinst, MAKEINTRESOURCE ( IDI_REAL ) ) ) == -1 ) { return NULL; }

    return img;
}

int LVInsertColumn ( HWND hList, int nCol, TCHAR *lpszColumnHeading, int nFormat, int nWidth, int nSubItem )
/*******************************************************************************************************************/
/* Insert a column in the listview                                                                                 */
{
    LVCOLUMN column;

    column.mask     = LVCF_TEXT | LVCF_FMT;
    column.pszText  = lpszColumnHeading;
    column.fmt      = nFormat;

    if ( nWidth != -1 )
    {
        column.mask |= LVCF_WIDTH;
        column.cx = nWidth;
    }

    if ( nSubItem != -1 )
    {
        column.mask |= LVCF_SUBITEM;
        column.iSubItem = nSubItem;
    }

    return ( int ) SendMessage ( hList, LVM_INSERTCOLUMN, nCol, ( LPARAM )&column );
}

int LVInsertItem ( HWND hList, int nItem, int nImgIndex, TCHAR *lpszItem )
/*******************************************************************************************************************/
/* Insert new item in the list                                                                                     */
{
    LVITEM  item;
    
    item.mask       = LVIF_TEXT;
    
    if ( nImgIndex != -1 ) { item.mask |= LVIF_IMAGE; }
    
    item.iItem      = nItem;
    item.iSubItem   = 0;
    item.pszText    = lpszItem;
    item.state      = 0;
    item.stateMask  = 0;
    item.iImage     = nImgIndex;
    item.lParam     = 0;

    return ( int ) SendMessage ( hList, LVM_INSERTITEM, 0, ( LPARAM )&item );
}

BOOL LVSetItemText ( HWND hList, int nItem, int nSubItem, TCHAR * lpszText )
/*******************************************************************************************************************/
/* Set item text                                                                                                   */
{
    LVITEM  item;

    item.iSubItem = nSubItem;
    item.pszText  = lpszText;
    return ( BOOL ) SendMessage ( hList, LVM_SETITEMTEXT, nItem, ( LPARAM )&item );
}

BOOL LVGetItemText ( HWND hList, int nItem, int nSubItem, TCHAR * lpszText, int size )
/*******************************************************************************************************************/
/* Get item text                                                                                                   */
{
    LVITEM  item;

    item.iSubItem   = nSubItem;
    item.cchTextMax = size;
    item.pszText    = lpszText;
    return ( BOOL ) SendMessage ( hList, LVM_GETITEMTEXT, nItem, ( LPARAM )&item );
}

BOOL LVClear ( HWND hList )
/*******************************************************************************************************************/
/* Clear a listvirew                                                                                               */
{
    return SendMessage ( hList, LVM_DELETEALLITEMS, 0, 0 );
}

int LVGetCount ( HWND hList )
/*******************************************************************************************************************/
/* Return list item count                                                                                          */
{
    return SendMessage ( hList, LVM_GETITEMCOUNT, 0, 0 );
}

int LVGetSelIndex ( HWND hList )
/*******************************************************************************************************************/
/* Return index of currently selected item                                                                         */
{
    int     i, count;
    DWORD   state;

    count = LVGetCount ( hList );
    for ( i = 0; i < count; i++ )
    {
        state = SendMessage ( hList, LVM_GETITEMSTATE, i, LVIS_SELECTED );
        if ( state & LVIS_SELECTED ) { return i; }
    }
    return -1;
}

void InitProcessColumns ( HWND hList )
/*******************************************************************************************************************/
/* Prepare list columns                                                                                            */
{
    COLDATA cd[5] = {   { TEXT("Process"), 150, LVCFMT_LEFT },
                        { TEXT("PID"), 85, LVCFMT_LEFT },
                        { TEXT("Threads"), 65, LVCFMT_LEFT },
                        { TEXT("Base priority"), 75, LVCFMT_LEFT },
                        { TEXT("Priority class"), 75, LVCFMT_LEFT } };
    int     i;

    for ( i = 0; i < 5; i++ )
        LVInsertColumn ( hList, i, cd[i].text, cd[i].flags, cd[i].width, -1 );
}

void InitModuleColumns ( HWND hList )
/*******************************************************************************************************************/
{
    COLDATA cd[4] = {   { TEXT("Module"), 90, LVCFMT_LEFT },
                        { TEXT("Loaded from"), 300, LVCFMT_LEFT },
                        { TEXT("Size (Kb)"), 55, LVCFMT_LEFT },
                        { TEXT("Base address"), 85, LVCFMT_LEFT } };
    int     i;

    for ( i = 0; i < 4; i++ )
        LVInsertColumn ( hList, i, cd[i].text, cd[i].flags, cd[i].width, -1 );
}

void InitImpColumns ( HWND hList )
/*******************************************************************************************************************/
{
    COLDATA cd[3] = {   { TEXT("From"), 90, LVCFMT_LEFT },
                        { TEXT("Imported function"), 180, LVCFMT_LEFT },
                        { TEXT("Ord."), 40, LVCFMT_LEFT } };
    int     i;

    for ( i = 0; i < 3; i++ )
        LVInsertColumn ( hList, i, cd[i].text, cd[i].flags, cd[i].width, -1 );
}

void InitExpColumns ( HWND hList )
/*******************************************************************************************************************/
{
    COLDATA cd[3] = {   { TEXT("Exported function"), 180, LVCFMT_LEFT },
                        { TEXT("RVA"), 75, LVCFMT_LEFT },
                        { TEXT("Ord."), 40, LVCFMT_LEFT } };
    int     i;

    for ( i = 0; i < 3; i++ )
        LVInsertColumn ( hList, i, cd[i].text, cd[i].flags, cd[i].width, -1 );
}
