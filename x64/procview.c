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

#define NT_ONLY

#include <windows.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <pehlp32.h>
#include <shell32x.h> // for run dlg
#include "resource.h"
#include "lview.h"
#include "stbar.h"
#include "draw.h"
#include "misc.h"

#ifdef NT_ONLY
#include <powrprof.h>
#endif

#define         MAINWND_STYLE       CS_DBLCLKS|CS_BYTEALIGNWINDOW|CS_BYTEALIGNCLIENT|CS_HREDRAW|CS_VREDRAW

const TCHAR szclassname[] = TEXT("PviewWndClass");
const TCHAR szapptitle[]  = TEXT("ProcView");
TCHAR * p_str[4] =
{
    TEXT("Idle"),
    TEXT("Normal"),
    TEXT("High"),
    TEXT("Realtime")
};

const TBYTE p_tbl[32] =
{
    0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
};

typedef struct 
{
    HWND    hlist;
    DWORD   index;
}   LISTDATA;

// FUNCTION PROTOTYPES

static LRESULT CALLBACK     WndProc             ( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam );
static INT_PTR CALLBACK     AboutDlgProc        ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
static INT_PTR CALLBACK     PropsDlgProc        ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam );
static void                 Process_WM_COMMAND  ( HWND hwnd, WPARAM wParam, LPARAM lParam );
static void                 Process_WM_NOTIFY   ( HWND hwnd, WPARAM wParam, LPARAM lParam );
static void                 InitPropsDlg        ( HWND hDlg );
static INT_PTR CALLBACK     EnumImpProc         ( IMPORT_INFO *impinfo, EN_LPARAM lParam );
static INT_PTR CALLBACK     EnumExpProc         ( EXPORT_INFO *expinfo, EN_LPARAM lParam );
static INT_PTR CALLBACK     EnumSecProc         ( SECTION_INFO *secinfo, EN_LPARAM lParam );
static INT_PTR CALLBACK     EnumAttrProc        ( TCHAR *buf, EN_LPARAM lParam );
static int                  ShowMessage         ( HWND howner, TCHAR *message, DWORD style );
static void                 GetProcesses        ( HWND hList );
static void                 GetModules          ( HWND hList, LONG pid );
static DWORD                GetCpuFreq          ( void );
static void                 ContextMenu         ( void );
static void                 CheckRadioMenus     ( void );
static void                 ToggleMenus         ( HMENU hmenu );
static BOOL                 KillProcess         ( void );
static BOOL                 SetPriority         ( DWORD pclass );
static void                 AdvancedInfo        ( HWND hparent );
static BOOL                 PEInfo              ( HWND hdlg );
static BOOL                 SetPrivilege        ( HANDLE hToken, TCHAR * Privilege, BOOL bEnablePrivilege );
static BOOL                 IsNT                ( void );
static void                 SetRights           ( void );
static void                 CenterWindow        ( HWND hwnd );

//
// GLOBALS
//

HINSTANCE           g_hInst;
HWND                g_hMainwnd;
HWND                g_hStatusbar;
HWND                g_hList;
HACCEL              g_hAccel;
HICON               g_hIcon;
HMENU               g_hMainmenu;
int                 g_sbpanels[4] = {90, 210, 310, -1};
HIMAGELIST          g_hIml;

//
// MAIN PROGRAM
//

int WINAPI WinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow )
/*******************************************************************************************************************/
{
    MSG         msg;
    WNDCLASSEX  wndclass;

    g_hInst  = GetModuleHandle (NULL);
    
    // try to get debug priviledges
    if (IsNT()) SetRights();

    g_hIcon  = LoadIcon ( g_hInst, MAKEINTRESOURCE ( IDI_PVIEW ) );
    g_hAccel = LoadAccelerators ( g_hInst, MAKEINTRESOURCE ( IDR_ACCEL1 ) );

    wndclass.cbSize        = sizeof ( WNDCLASSEX );
    wndclass.hInstance     = g_hInst; 
    wndclass.style         = MAINWND_STYLE;
    wndclass.lpfnWndProc   = WndProc;
    wndclass.cbClsExtra    = 0;
    wndclass.cbWndExtra    = 0;
    wndclass.hIcon         = g_hIcon;
    wndclass.hIconSm       = g_hIcon;
    wndclass.hCursor       = LoadCursor ( NULL, IDC_ARROW );
    wndclass.hbrBackground = (HBRUSH) GetStockObject ( NULL_BRUSH );
    wndclass.lpszMenuName  = MAKEINTRESOURCE ( IDR_MAINMENU );
    wndclass.lpszClassName = szclassname;

    if ( !RegisterClassEx ( &wndclass ) )
    {
        ShowMessage ( NULL, TEXT("Unable to register window class"), MB_OK );
        return FALSE;
    }

    InitCommonControls();
    g_hMainwnd = CreateWindowEx (WS_EX_LEFT|WS_EX_ACCEPTFILES,
                                 szclassname,
                                 szapptitle,
                                 WS_OVERLAPPEDWINDOW,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 NULL,
                                 NULL,
                                 g_hInst,
                                 NULL
                               );
     
    if ( !g_hMainwnd )
    {
        ShowMessage ( NULL, TEXT("Could not create main window"), MB_OK );
        return FALSE;
    }

    g_hMainmenu = GetMenu ( g_hMainwnd );
    CenterWindow ( g_hMainwnd );
    ShowWindow ( g_hMainwnd, iCmdShow );
    UpdateWindow ( g_hMainwnd );

    while ( GetMessage ( &msg, NULL, 0, 0 ) )
    {
        if  ( !TranslateAccelerator ( g_hMainwnd, g_hAccel, &msg ) )
        {
            TranslateMessage ( &msg ) ;
            DispatchMessage ( &msg ) ;
        }
    }
    return msg.wParam;
}

static INT_PTR CALLBACK AboutDlgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
/*******************************************************************************************************************/
/* "About" dlg callback proc                                                                                       */
{
    switch ( uMsg )
    {
        case WM_INITDIALOG:
            break;

        case WM_CLOSE:
            EndDialog ( hDlg, FALSE );
            break;

        case WM_COMMAND:
            if ( LOWORD ( wParam ) == IDCANCEL )
                SendMessage ( hDlg, WM_CLOSE, 0, 0 );
            break;

        default:
            return FALSE;
            break;
    }

    return TRUE;
}

static INT_PTR CALLBACK PropsDlgProc ( HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
/*******************************************************************************************************************/
/* "Properties" dlg callback proc                                                                                  */
{
    switch ( uMsg )
    {
        case WM_INITDIALOG:
            InitPropsDlg ( hDlg );
            break;

        // doubleclick via WM_NOTIFY
        case WM_NOTIFY:
            if ( wParam == IDC_MODULELIST )
            {
                switch ( ( ( NMHDR *) lParam )->code )
                {
                    case NM_DBLCLK:
                        PEInfo ( hDlg );
                        break;
                }
            }        
            break;
        
        case WM_CLOSE:
            EndDialog ( hDlg, FALSE );
            break;

        case WM_COMMAND:
            if ( LOWORD ( wParam ) == IDCANCEL )
                SendMessage ( hDlg, WM_CLOSE, 0, 0 );
            break;

        default:
            return FALSE;
            break;
    }

    return TRUE;
}

static LRESULT CALLBACK WndProc ( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
/*******************************************************************************************************************/
/* main window procedure                                                                                           */
{
    RECT            rstat;
    TCHAR           buf[64];

    switch ( message )
    {
        case WM_MENUSELECT:
            MyMenuHelp ( g_hInst, g_hStatusbar, wParam );
            SendMessage ( g_hStatusbar, WM_PAINT, 0, 0 );
            break;

        case WM_SIZE:
            SendMessage ( g_hStatusbar, WM_SIZE, 0, 0 );
            GetClientRect ( g_hStatusbar, &rstat );
            MoveWindow ( g_hList, 0, 0, LOWORD ( lParam ),
                        HIWORD ( lParam ) - rstat.bottom, TRUE );
            break;

        case WM_CREATE:
            g_hStatusbar = MakeStatusBar ( hwnd, IDC_STATUSBAR, g_sbpanels, 4 );
            g_hList      = MakeListView ( g_hInst, hwnd );
            if ( ( g_hIml = InitImgList ( g_hInst ) ) != NULL )
                ListView_SetImageList ( g_hList, g_hIml, LVSIL_SMALL );
            InitProcessColumns ( g_hList );
            GetProcesses ( g_hList );
            wsprintf ( buf, TEXT("CPU: %lu Mhz"), GetCpuFreq() );
            SBSetText ( g_hStatusbar, 1, buf );
            break;

        case WM_SETFOCUS:
            SetFocus ( g_hList );
            break;

        case WM_INITMENUPOPUP:
            if (LOWORD ( lParam ) == 1 )
            {
                CheckRadioMenus();
                ToggleMenus ( ( HMENU ) wParam );
            }
            break;

        case WM_NOTIFY:
            Process_WM_NOTIFY ( hwnd, wParam, lParam );
            break;

        case WM_COMMAND:
            Process_WM_COMMAND ( hwnd, wParam, lParam );
            break;

        case WM_CLOSE:
            DestroyWindow ( hwnd );
            break;

        case WM_DESTROY:
            if ( g_hIml )
                ImageList_Destroy ( g_hIml );
            PostQuitMessage ( 0 ) ;
            break;

        default:
            return DefWindowProc ( hwnd, message, wParam, lParam );
            break;
    }

    return FALSE;
}

static int ShowMessage ( HWND howner, TCHAR *message, DWORD style )
/*******************************************************************************************************************/
/* "Custom" messagebox                                                                                             */
{
    MSGBOXPARAMS            mp;

    mp.cbSize               = sizeof ( mp );
    mp.dwStyle              = MB_USERICON | style;
    mp.hInstance            = g_hInst;
    mp.lpszIcon             = MAKEINTRESOURCE ( IDI_PVIEW );
    mp.hwndOwner            = howner;
    mp.lpfnMsgBoxCallback   = NULL;
    mp.lpszCaption          = szapptitle;
    mp.lpszText             = message;
    mp.dwContextHelpId      = 0;
    mp.dwLanguageId         = MAKELANGID ( LANG_NEUTRAL, SUBLANG_DEFAULT );
    return MessageBoxIndirect ( &mp );
}

static void GetProcesses ( HWND hList )
/*******************************************************************************************************************/
/* list running processes via toolhelp32 interface                                                                 */
{
    HANDLE          hsnap;
    PROCESSENTRY32  pe;
    MEMORYSTATUS    ms;
    BOOL            ok;
    TCHAR           buf[64];
    int             i;

    hsnap = CreateToolhelp32Snapshot ( TH32CS_SNAPPROCESS, 0 );
    if ( hsnap == INVALID_HANDLE_VALUE ) return;
    pe.dwSize = sizeof ( PROCESSENTRY32 );
    ok = Process32First ( hsnap, &pe );
    if ( ok )
    {
        i = 0;
        while ( ok )
        {
            wsprintf ( buf, TEXT("%s"), pe.szExeFile );
            LVInsertItem ( hList, i, p_tbl[pe.pcPriClassBase], buf );
            
            wsprintf ( buf, TEXT("%lu"), pe.th32ProcessID );
            LVSetItemText ( hList, i, 1, buf );
            
            wsprintf ( buf, TEXT("%lu"), pe.cntThreads );
            LVSetItemText ( hList, i, 2, buf );
            
            wsprintf ( buf, TEXT("%ld"), pe.pcPriClassBase );
            LVSetItemText ( hList, i, 3, buf );

            LVSetItemText ( hList, i, 4, p_str[(int)p_tbl[pe.pcPriClassBase]] );
            
            pe.dwSize = sizeof ( PROCESSENTRY32 );
            ok = Process32Next ( hsnap, &pe );
            i++;
        }
    }
    CloseHandle ( hsnap );
    wsprintf ( buf, TEXT("Processes: %d"), LVGetCount ( g_hList ) );
    SBSetText ( g_hStatusbar, 0, buf );
    GlobalMemoryStatus ( &ms );
    wsprintf ( buf, TEXT("Mem. load: %lu%%"), ms.dwMemoryLoad );
    SBSetText ( g_hStatusbar, 2, buf );
    wsprintf ( buf, TEXT("Page file: %lu Mb, %lu Mb available"), ms.dwTotalPageFile >> 20, ms.dwAvailPageFile >> 20 );
    SBSetText ( g_hStatusbar, 3, buf );
}

static void GetModules ( HWND hList, LONG pid )
/*******************************************************************************************************************/
/* ...got a running process, now list its loaded modules                                                           */
{
    MODULEENTRY32   me;
    HMODULE         hsnap;
    BOOL            ok;
    TCHAR           buf[256];
    int             i;

    hsnap = CreateToolhelp32Snapshot ( TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid );
    if ( hsnap == INVALID_HANDLE_VALUE ) return;
    me.dwSize = sizeof ( MODULEENTRY32 );
    ok = Module32First ( hsnap, &me );
    if ( ok )
    {
        i = 0;
        while ( ok )
        {
            LVInsertItem ( hList, i, -1, me.szModule );
            LVSetItemText ( hList, i, 1, me.szExePath );
            
            wsprintf ( buf, TEXT("%lu"), me.modBaseSize >> 10 );
            LVSetItemText ( hList, i, 2, buf );
            
            wsprintf ( buf, TEXT("0x%0.8X"), me.modBaseAddr );
            LVSetItemText ( hList, i, 3, buf );
            
            me.dwSize = sizeof ( MODULEENTRY32 );
            ok = Module32Next ( hsnap, &me );
            i++;
        }
    }
    CloseHandle ( hsnap );
}

static void ContextMenu ( void )
/*******************************************************************************************************************/
/* Display context menu                                                                                            */
{
    POINT       pt;
    HMENU       hSub;

    GetCursorPos ( &pt );
    hSub = GetSubMenu ( g_hMainmenu, 1 );
    TrackPopupMenuEx ( hSub,
                        TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
                        pt.x, pt.y, g_hMainwnd, NULL );
}

static DWORD GetCpuFreq ( void )
/*******************************************************************************************************************/
/* Return CPU freq.                                                                                                */
{
// it's always NT nowadays, left for historical reasons :-))
//#ifdef NT_ONLY // 
    PROCESSOR_POWER_INFORMATION ppi[16];    //assume 16 cores max :-) - if not an lazy ass, use GetSystemInfo to get # o cpus and malloc =)

    if ( CallNtPowerInformation ( ProcessorInformation, NULL, 0, &ppi, 16*sizeof ( PROCESSOR_POWER_INFORMATION ) ) != 0 )
        return 0;
        
    return ppi[0].CurrentMhz;               //return first cpu freq.
//#else
/*
    // can't use the code below on 64 bit (inline assembly not supported)
    // and it's obsolete even on x86
    DWORD                       TimerHi, TimerLo, Priority, PriorityClass;

    // temporarily set high priority 
    // for better precision
    PriorityClass = GetPriorityClass ( GetCurrentProcess() );
    Priority = GetThreadPriority ( GetCurrentThread() );

    SetPriorityClass ( GetCurrentProcess(), REALTIME_PRIORITY_CLASS );
    SetThreadPriority ( GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
    // posibil sa nu avem minim pentium
    __try
    {
        Sleep ( 10 );
        _asm rdtsc              //DW 310Fh
        _asm mov TimerLo, eax
        _asm mov TimerHi, edx
        Sleep ( 500 );          //sleep for 500ms
        _asm rdtsc              //DW 310Fh
        _asm sub eax, TimerLo
        _asm sbb edx, TimerHi
        _asm mov TimerLo, eax
        _asm mov TimerHi, edx
    }
    __except ( TRUE )
    {
        TimerLo = 0;
    }

    SetThreadPriority ( GetCurrentThread(), Priority );
    SetPriorityClass ( GetCurrentProcess(), PriorityClass );

    return ( TimerLo / ( 1000 * 500 ) );
#endif*/
}

// MENU MANAGEMENT FUNCTIONS

static void CheckRadioMenus ( void )
/*******************************************************************************************************************/
{
    HMENU   hSub, hDrop;
    TCHAR   buf[16];
    int     index;

    index = LVGetSelIndex ( g_hList );
    if ( index == -1 ) return;
    hSub = GetSubMenu ( g_hMainmenu, 1 );
    hDrop = GetSubMenu ( hSub, 2 );
    LVGetItemText ( g_hList, index, 3, buf, sizeof ( buf ) );
    
    CheckMenuRadioItem ( hDrop, 0, 3, p_tbl[MyAtol(buf)], MF_BYPOSITION );
}

static void ToggleMenus ( HMENU hmenu )
/*******************************************************************************************************************/
{
    DWORD       states[2] = { MF_GRAYED,
                              MF_ENABLED
                            };
    BOOL        state;

    state = ( LVGetSelIndex ( g_hList ) != -1 );

    EnableMenuItem ( hmenu, IDM_PROP, states[state] );
    EnableMenuItem ( hmenu, IDM_KILL, states[state] );
    EnableMenuItem ( hmenu, 2, ( states[state] )|MF_BYPOSITION );
}

static BOOL KillProcess ( void )
/*******************************************************************************************************************/
/* Terminate a process                                                                                             */
{
    TCHAR   buf[16];
    HANDLE  hProc;
    int     index;
    DWORD   pid;
    BOOL    bret;

    index = LVGetSelIndex ( g_hList );
    if ( index == -1 ) { return FALSE; }

    LVGetItemText ( g_hList, index, 1, buf, sizeof ( buf ) );
    pid = MyAtol ( buf );
    hProc = OpenProcess ( PROCESS_ALL_ACCESS, FALSE, pid );
    if ( hProc == NULL ) { return FALSE; }
    bret = TerminateProcess ( hProc, 1 );
    CloseHandle ( hProc );
    return bret;
}

static void AdvancedInfo ( HWND hparent )
/*******************************************************************************************************************/
/* Display the "Properties" dlgbox for a process                                                                   */
{
    int index;

    index = LVGetSelIndex ( g_hList );
    if ( index != -1 )
        DialogBoxParam ( g_hInst, MAKEINTRESOURCE ( IDD_PROP ),
                         hparent, PropsDlgProc, ( LPARAM ) index );
}

static BOOL SetPriority ( DWORD pclass )
/*******************************************************************************************************************/
/* Change process priority                                                                                         */
{
    int     index;
    HANDLE  hProc;
    TCHAR   buf[16];
    BOOL    bret;
    LONG    pid;

    index = LVGetSelIndex ( g_hList );
    if ( index == -1 ) { return FALSE; }

    LVGetItemText ( g_hList, index, 1, buf, sizeof ( buf ) );
    pid = MyAtol ( buf );
    hProc = OpenProcess ( PROCESS_SET_INFORMATION, FALSE, pid );
    if ( hProc == NULL ) { return FALSE; }
    bret = SetPriorityClass ( hProc, pclass );
    CloseHandle ( hProc );
    return bret;
}

// CALLBACK FUNCTIONS FOR ENUMERATION

static INT_PTR CALLBACK EnumImpProc ( IMPORT_INFO * impinfo, EN_LPARAM lParam )
/*******************************************************************************************************************/
{
    LISTDATA    * pldata;
    TCHAR       buf[16];
    
    pldata = ( LISTDATA * )lParam;
    
    LVInsertItem ( pldata->hlist, pldata->index, -1, impinfo->name );
    LVSetItemText ( pldata->hlist, pldata->index, 1, impinfo->fnname );
    wsprintf ( buf, TEXT("%lu"), impinfo->ordinal );
    LVSetItemText ( pldata->hlist, pldata->index, 2, buf );
    pldata->index++;
    return TRUE;
}

static INT_PTR CALLBACK EnumExpProc ( EXPORT_INFO * expinfo, EN_LPARAM lParam )
/*******************************************************************************************************************/
{
    LISTDATA    * pldata;
    TCHAR       buf[16];
    
    pldata = ( LISTDATA * )lParam;
    
    LVInsertItem ( pldata->hlist, pldata->index, -1, expinfo->fnname );
    wsprintf ( buf, TEXT("0x%0.8X"), expinfo->entryptrva );
    LVSetItemText ( pldata->hlist, pldata->index, 1, buf );
    wsprintf ( buf, TEXT("%lu"), expinfo->ordinal );
    LVSetItemText ( pldata->hlist, pldata->index, 2, buf );
    pldata->index++;
    return TRUE;
}

static INT_PTR CALLBACK EnumSecProc ( SECTION_INFO * secinfo, EN_LPARAM lParam )
/*******************************************************************************************************************/
{
    TCHAR       buf[256];
    DWORD       i;
    
    wsprintf ( buf, TEXT("%s\r\n"), secinfo->name );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    wsprintf ( buf, TEXT("\tOffset to raw data: 0x%0.8X\r\n"), secinfo->rdataoffs );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    wsprintf ( buf, TEXT("\tRaw data size: 0x%0.8X\r\n"), secinfo->rdatasize );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    wsprintf ( buf, TEXT("\tVirtual addres: 0x%0.8X\r\n"), secinfo->vaddr );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    wsprintf ( buf, TEXT("\tVirtual size: 0x%0.8X\r\n"), secinfo->vsize );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    wsprintf ( buf, TEXT("\tAttributes: 0x%0.8X\r\n"), secinfo->chars );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    for ( i = 0; i < secinfo->cchars; i++ )
    {
        wsprintf ( buf, TEXT("\t%s\r\n"), secinfo->szchars[i] );
        SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )buf );
    }
    return TRUE;
}

static INT_PTR CALLBACK EnumAttrProc ( TCHAR * buf, EN_LPARAM lParam )
/*******************************************************************************************************************/
{    
    TCHAR   temp[128];
    wsprintf ( temp, TEXT("\t%s\r\n"), buf );
    SendMessage ( ( HWND )lParam,  EM_REPLACESEL, 0, ( LPARAM )temp );
    return TRUE;
}

static BOOL PEInfo ( HWND hdlg )
/*******************************************************************************************************************/
/* Executes on dblclick on a entry from the module list                                                            */
{
    MOD_BASE                pemodule;
    LISTDATA                ldata;
    TCHAR                   buf[256];
    TCHAR                   crtmod[256];
    TCHAR                   temp[128];
    int                     index;
    HWND                    hmodlst, hexplst, himplst, hmisc;
    PIMAGE_FILE_HEADER      pImageFileHeader;
    PIMAGE_OPTIONAL_HEADER  optionalHeader;
    PIMAGE_NT_HEADERS       pNTHeader;
    FILETIME                ft;
    SYSTEMTIME              st;
    BOOL                    is64bit;

    hmodlst = GetDlgItem ( hdlg, IDC_MODULELIST );
    index = LVGetSelIndex ( hmodlst );
    if ( index == -1 ) { return FALSE; }
    
    LVGetItemText ( hmodlst, index, 1, crtmod, sizeof ( crtmod ) );

    if ( !PE_OpenModule ( crtmod, &pemodule, TRUE ) ) { return FALSE; }
    if ( pemodule.type != T_PE ) { return FALSE; }

    pNTHeader = pemodule.ntheader;

    is64bit = ( IMAGE_NT_OPTIONAL_HDR64_MAGIC == pNTHeader->OptionalHeader.Magic );

    himplst = GetDlgItem ( hdlg, IDC_IMPLIST );
    hexplst = GetDlgItem ( hdlg, IDC_EXPLIST );
    hmisc   = GetDlgItem ( hdlg, IDC_MISC );

    BeginDraw ( himplst );
    BeginDraw ( hexplst );
    BeginDraw ( hmisc );

    // list imports
    ldata.hlist = himplst;
    ldata.index = 0;
    LVClear ( himplst );
    PE_EnumImports ( ( IMG_BASE )pemodule.filebase, 0, ( ENUMIMPORTFNSPROC )EnumImpProc, ( EN_LPARAM )&ldata );

    // list exports
    ldata.hlist = hexplst;
    ldata.index = 0;
    LVClear ( hexplst );
    PE_EnumExports ( ( IMG_BASE )pemodule.filebase, 0, ( ENUMEXPORTFNSPROC )EnumExpProc, ( EN_LPARAM )&ldata ); 

    // list PE info
    SendMessage ( hmisc, WM_SETTEXT, 0, 0 );
    wsprintf ( buf, TEXT("[S E C T I O N S]\r\n") );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );
    PE_EnumSections ( ( IMG_BASE )pemodule.filebase, ( ENUMSECTIONSPROC )EnumSecProc, ( EN_LPARAM )hmisc );

    pImageFileHeader = ( PIMAGE_FILE_HEADER )PE_GetFileHeader ( ( IMG_BASE )pemodule.filebase );

    wsprintf ( buf, TEXT("\r\n[F I L E  H E A D E R]\r\n") );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );
    
    PE_GetMachineType ( pImageFileHeader->Machine, temp );
    wsprintf ( buf, TEXT("\tMachine: %s\r\n"), temp );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tNumber of sections: 0x%0.4X\r\n"), pImageFileHeader->NumberOfSections );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    TimeToFileTime ( ( long )pImageFileHeader->TimeDateStamp, &ft );
    FileTimeToSystemTime ( &ft, &st );
    wsprintf ( temp, TEXT("%0.2lu/%0.2lu/%lu, %0.2lu:%0.2lu:%0.2lu"),
               st.wDay, st.wMonth, st.wYear,
               st.wHour, st.wMinute, st.wSecond );
    wsprintf ( buf, TEXT("\tTimestamp: %s\r\n"), temp );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tOptional header size: 0x%0.4X\r\n"), pImageFileHeader->SizeOfOptionalHeader );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tCharacteristics: 0x%0.8X\r\n"), pImageFileHeader->Characteristics );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    PE_EnumCharacteristics ( (IMG_BASE)pemodule.filebase, (ENUMFHATTRIBPROC)EnumAttrProc, ( EN_LPARAM )hmisc );

    optionalHeader = (PIMAGE_OPTIONAL_HEADER)PE_GetOptionalHeader ( (IMG_BASE)pemodule.filebase );

    wsprintf ( buf, TEXT("\r\n[O P T I O N A L  H E A D E R]\r\n") );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tMagic: 0x%0.4X\r\n"), optionalHeader->Magic );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tLinker version: %u.%0.2u\r\n"), optionalHeader->MajorLinkerVersion,
               optionalHeader->MinorLinkerVersion );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tImage version: %u.%0.2u\r\n"), optionalHeader->MajorImageVersion,
               optionalHeader->MinorImageVersion );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tRequired OS version: %u.%0.2u\r\n"), optionalHeader->MajorOperatingSystemVersion,
               optionalHeader->MinorOperatingSystemVersion );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tSubsystem version: %u.%0.2u\r\n"), optionalHeader->MajorSubsystemVersion,
               optionalHeader->MinorSubsystemVersion );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tWin32 version: %0.2u\r\n"), optionalHeader->Win32VersionValue );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tSubsystem: ") );
    switch ( optionalHeader->Subsystem )
    {
        case IMAGE_SUBSYSTEM_NATIVE:
            lstrcat ( buf, TEXT("Native\r\n") );
            break;

        case IMAGE_SUBSYSTEM_WINDOWS_GUI:
            lstrcat ( buf, TEXT("Windows GUI\r\n") );
            break;

        case IMAGE_SUBSYSTEM_WINDOWS_CUI:
            lstrcat ( buf, TEXT("Windows character\r\n") );
            break;

        case IMAGE_SUBSYSTEM_OS2_CUI:
            lstrcat ( buf, TEXT("OS/2 character\r\n") );
            break;

        case IMAGE_SUBSYSTEM_POSIX_CUI:
            lstrcat ( buf, TEXT("POSIX character\r\n") );
            break;

        default:
            lstrcat ( buf, TEXT("Unknown\r\n") );
            break;
    }
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tCode size: 0x%0.8X\r\n"), optionalHeader->SizeOfCode );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tInit. data size: 0x%0.8X\r\n"), optionalHeader->SizeOfInitializedData );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tUnininit. data size: 0x%0.8X\r\n"), optionalHeader->SizeOfUninitializedData );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tImage size: 0x%0.8X\r\n"), optionalHeader->SizeOfImage );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tHeaders size: 0x%0.8X\r\n"), optionalHeader->SizeOfHeaders );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tEntry point RVA: 0x%0.8X\r\n"), optionalHeader->AddressOfEntryPoint );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tBase of code: 0x%0.8X\r\n"), optionalHeader->BaseOfCode );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    if ( !is64bit )
    {
        wsprintf ( buf, TEXT("\tBase of data: 0x%0.8X\r\n"), ((PIMAGE_OPTIONAL_HEADER32)optionalHeader)->BaseOfData );
        SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );
    }

    wsprintf ( buf, TEXT("\tPreferred image base: 0x%0.8X\r\n"), optionalHeader->ImageBase );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tSection alignment: 0x%0.8X\r\n"), optionalHeader->SectionAlignment );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tFile alignment: 0x%0.8X\r\n"), optionalHeader->FileAlignment );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tChecksum: 0x%0.8X\r\n"), optionalHeader->CheckSum );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tReserved stack: 0x%0.8X\r\n"), optionalHeader->SizeOfStackReserve );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );
    
    wsprintf ( buf, TEXT("\tCommit stack: 0x%0.8X\r\n"), optionalHeader->SizeOfStackCommit );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tReserved heap: 0x%0.8X\r\n"), optionalHeader->SizeOfHeapReserve );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    wsprintf ( buf, TEXT("\tCommit heap: 0x%0.8X\r\n"), optionalHeader->SizeOfHeapCommit );
    SendMessage ( hmisc,  EM_REPLACESEL, 0, ( LPARAM )buf );

    // finished updating, do a repaint
    EndDraw ( himplst );
    EndDraw ( hexplst );
    EndDraw ( hmisc );

    PE_CloseModule ( &pemodule );
    return TRUE;
}

static BOOL SetPrivilege ( HANDLE  hToken, TCHAR * Privilege, BOOL bEnablePrivilege )
/*******************************************************************************************************************/
/* Set a certaing execution privilege                                                                              */
{
    TOKEN_PRIVILEGES    tp;
    LUID                luid;
    TOKEN_PRIVILEGES    tpPrevious;
    DWORD               cbPrevious;

    cbPrevious = sizeof ( TOKEN_PRIVILEGES );
    
    if ( !LookupPrivilegeValue ( NULL, Privilege, &luid ) ) return FALSE;

    tp.PrivilegeCount           = 1;
    tp.Privileges[0].Luid       = luid;
    tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges ( hToken,
                            FALSE,
                            &tp,
                            sizeof ( TOKEN_PRIVILEGES ),
                            &tpPrevious,
                            &cbPrevious
                          );

    if ( GetLastError() != ERROR_SUCCESS ) return FALSE;

    tpPrevious.PrivilegeCount       = 1;
    tpPrevious.Privileges[0].Luid   = luid;

    if( bEnablePrivilege )
    {
        tpPrevious.Privileges[0].Attributes |= ( SE_PRIVILEGE_ENABLED );
    }
    else 
    {
        tpPrevious.Privileges[0].Attributes ^= ( SE_PRIVILEGE_ENABLED &
        tpPrevious.Privileges[0].Attributes );
    }

    AdjustTokenPrivileges ( hToken,
                            FALSE,
                            &tpPrevious,
                            cbPrevious,
                            NULL,
                            NULL
                          );

    if ( GetLastError() != ERROR_SUCCESS ) return FALSE;
    return TRUE;
}

static void SetRights ( void )
/*******************************************************************************************************************/
{
    HANDLE  hToken;

    if ( !OpenProcessToken ( GetCurrentProcess(),
         TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
         &hToken ) )
    {
        ShowMessage ( NULL, TEXT("Warning: You'll not be able to view system process info.\nYou don't have sufficient rights." ), MB_OK );
        return;
    }

    if ( !SetPrivilege ( hToken, SE_DEBUG_NAME, TRUE ) )
    {
        ShowMessage ( NULL, TEXT("Unable to set SE_DEBUG_NAME privilege."), MB_OK );
        CloseHandle ( hToken );
        return;
    }
    
    if ( !SetPrivilege ( hToken, SE_INC_BASE_PRIORITY_NAME, TRUE ) )
    {
        ShowMessage ( NULL, TEXT("Unable to set SE_INC_BASE_PRIORITY_NAME privilege."), MB_OK );
        CloseHandle ( hToken );
        return;
    }

    CloseHandle( hToken );
}

static BOOL IsNT ( void )
/*******************************************************************************************************************/
/* 8-)) */
{
    return TRUE;
}

static void CenterWindow ( HWND hwnd )
/*******************************************************************************************************************/
/* Center the app window on the screen                                                                             */
{
    RECT    aRt;

    GetWindowRect ( hwnd, &aRt );
    OffsetRect ( &aRt, -aRt.left, -aRt.top );
    MoveWindow ( hwnd,
                 ( ( GetSystemMetrics ( SM_CXSCREEN ) -
                 aRt.right ) / 2 + 4 ) & ~7,
                 ( GetSystemMetrics ( SM_CYSCREEN ) -
                 aRt.bottom ) / 2,
                 aRt.right, aRt.bottom, 0 );
}

static void Process_WM_COMMAND ( HWND hwnd, WPARAM wParam, LPARAM lParam )
/*******************************************************************************************************************/
/* Process WM_COMMAND for the main window                                                                          */
{
    if ( !lParam )
    {
        // menu processing
        switch ( LOWORD ( wParam ) )
        {
            case IDM_QUIT:
                SendMessage ( hwnd, WM_CLOSE, 0, 0 );
                break;

            case IDM_RUN:
                RunFileDlg ( hwnd, g_hIcon, NULL, NULL, NULL, 0 );
                LVClear ( g_hList );
                GetProcesses ( g_hList );
                break;

            case IDM_ABOUT:
                DialogBoxParam ( g_hInst, MAKEINTRESOURCE ( IDD_ABOUT ),
                                hwnd, AboutDlgProc, ( LPARAM )hwnd );
                break;

            case IDM_PROP:
                AdvancedInfo ( hwnd );
                break;

            case IDM_REFRESH:
                LVClear ( g_hList );
                GetProcesses ( g_hList );
                break;
            
            case IDM_PRI_IDLE:
                if ( !SetPriority ( IDLE_PRIORITY_CLASS ) )
                {
                    ShowMessage ( hwnd, TEXT("Unable to set priority."), MB_OK );
                    break;
                }
                Sleep ( 50 );
                LVClear ( g_hList );
                GetProcesses ( g_hList );
                break;

            case IDM_PRI_NOR:
                if ( !SetPriority ( NORMAL_PRIORITY_CLASS ) )
                {
                    ShowMessage ( hwnd, TEXT("Unable to set priority."), MB_OK );
                    break;
                }
                Sleep ( 50 );
                LVClear ( g_hList );
                GetProcesses ( g_hList );
                break;

            case IDM_PRI_HI:
                if ( !SetPriority ( HIGH_PRIORITY_CLASS ) )
                {
                    ShowMessage ( hwnd, TEXT("Unable to set priority."), MB_OK );
                    break;
                }
                Sleep ( 50 );
                LVClear ( g_hList );
                GetProcesses ( g_hList );
                break;

            case IDM_PRI_REAL:
                if ( !SetPriority ( REALTIME_PRIORITY_CLASS ) )
                {
                    ShowMessage ( hwnd, TEXT("Unable to set priority."), MB_OK );
                    break;
                }
                Sleep ( 50 );
                LVClear ( g_hList );
                GetProcesses ( g_hList );
                break;

            case IDM_KILL:
                if ( ShowMessage ( hwnd, TEXT("Terminate this process?"), MB_YESNOCANCEL ) == IDYES )
                {
                    if ( !KillProcess() )
                        ShowMessage ( hwnd, TEXT("Unable to terminate process."), MB_OK );
                    Sleep ( 300 );
                    LVClear ( g_hList );
                    GetProcesses ( g_hList );
                }
                break;
        }
    }
}

static void Process_WM_NOTIFY ( HWND hwnd, WPARAM wParam, LPARAM lParam )
/*******************************************************************************************************************/
/* Process WM_NOTIFY for the main window                                                                           */
{
    if ( wParam == IDC_PROCLIST )
    {
        switch ( ( ( NMHDR *) lParam )->code )
        {
            case NM_RCLICK:
                if ( LVGetSelIndex ( g_hList ) != -1 )
                {
                    CheckRadioMenus();
                    ToggleMenus ( GetSubMenu ( g_hMainmenu, 1 ) );
                    ContextMenu();
                }
                break;

            case NM_DBLCLK:
                AdvancedInfo ( hwnd );
                break;
        }
    }
}

static void InitPropsDlg ( HWND hDlg )
/*******************************************************************************************************************/
/* Init process properties dlg box                                                                                 */
{
    TCHAR       buf[256];
    HWND        hmodlst, hexplst, himplst;
    int         index;
    DWORD       pid;
    HANDLE      hProc;
    FILETIME    ft1, ft2, ft3, ft4, local;
    SYSTEMTIME  st;

    CenterWindow ( hDlg );
    hmodlst = GetDlgItem ( hDlg, IDC_MODULELIST );
    hexplst = GetDlgItem ( hDlg, IDC_EXPLIST );
    himplst = GetDlgItem ( hDlg, IDC_IMPLIST );
    ListView_SetExtendedListViewStyle ( hmodlst, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP );
    ListView_SetExtendedListViewStyle ( himplst, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP );
    ListView_SetExtendedListViewStyle ( hexplst, LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP );
    InitModuleColumns ( hmodlst );
    InitImpColumns ( himplst );
    InitExpColumns ( hexplst );
    index   = LVGetSelIndex ( g_hList );
    LVGetItemText ( g_hList, index, 0, buf, sizeof ( buf ) );
    SetWindowText ( hDlg, buf );
    LVGetItemText ( g_hList, index, 1, buf, sizeof ( buf ) );
    pid = MyAtol ( buf );
    GetModules ( hmodlst, pid );

    if ( IsNT() )
    {
        hProc = OpenProcess ( PROCESS_ALL_ACCESS/*PROCESS_QUERY_INFORMATION*/, FALSE, pid );
        if ( hProc )
        {
            if ( GetProcessTimes ( hProc, &ft1, &ft2, &ft3, &ft4 ) )
            {
                FileTimeToLocalFileTime ( &ft1, &local );
                FileTimeToSystemTime ( &local, &st );
                wsprintf ( buf, TEXT("Process started on %0.2lu/%0.2lu/%lu, %0.2lu:%0.2lu:%0.2lu -- %d loaded modules" ),
                          st.wDay, st.wMonth, st.wYear,
                          st.wHour, st.wMinute, st.wSecond,
                          LVGetCount ( hmodlst )
                         );
                SetDlgItemText ( hDlg, IDC_STATIC2, buf );
                CloseHandle ( hProc );
            }
        }
    }
}
