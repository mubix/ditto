#include <windows.h>
#include <string>
#include "./banned.h"

// Most code stolen from:
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms648008(v=vs.85).aspx#_win32_Creating_a_Resource_List

BOOL EnumTypesFunc(HMODULE hModule, LPCTSTR lpType, LONG lParam);
BOOL EnumNamesFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, LONG lParam);
BOOL EnumLangsFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLang, LONG lParam);

HANDLE hTarget;
BOOL bFailed = false;

int wmain(int argc, wchar_t * argv[])
{
	HMODULE hExe;

	if (argc < 3)
	{
		printf("\nditto - binary resource mirrorer\n"
			"--------------------------------------------------------------------\n\n"
			"C:\\>ditto.exe sourcebin.exe targetbin.exe\n\n"
			);
		return 0;
	}

	hExe = LoadLibraryEx(argv[1], NULL, LOAD_LIBRARY_AS_DATAFILE);
	if (hExe == NULL)
	{
		printf("Error code: %d\n", hExe);
	    return 1;
	}

	hTarget = BeginUpdateResource(argv[2], TRUE);
	EnumResourceTypes(hExe, (ENUMRESTYPEPROC)EnumTypesFunc, 0);
	if(bFailed) // if we failed to update anythign, discard changes
	{
		EndUpdateResource(hTarget, TRUE);
	}
	else
	{
		EndUpdateResource(hTarget, FALSE);
	}
	FreeLibrary(hExe);
	return 0;
}

BOOL EnumTypesFunc(HMODULE hModule, LPCTSTR lpType, LONG lParam)
{
	EnumResourceNames(hModule, lpType, (ENUMRESNAMEPROC)EnumNamesFunc, 0);
	return TRUE;
}

BOOL EnumNamesFunc(HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, LONG lParam)
{
	EnumResourceLanguages(hModule, lpType, lpName, (ENUMRESLANGPROC)EnumLangsFunc, 0);
    return TRUE;
}

// greetz 3nki for the fix
BOOL EnumLangsFunc( HMODULE hModule, LPCTSTR lpType, LPCTSTR lpName, WORD wLang, LONG lParam )
{
    HRSRC hResInfo;
    HGLOBAL hRes;
    DWORD sRes;
    BOOL bUpdated;
    LPVOID pRes;

    bUpdated = FALSE;

    if( !IS_INTRESOURCE( lpType ) ) {
        if( wcscmp( L"MUI", lpType ) == 0 ) {
            
            // do not copy MUI resources
            //printf( "Skipping MUI resource...\n" );
            return TRUE;
        }
    }

    hResInfo = FindResourceExW( hModule, lpType, lpName, wLang );

    if( hResInfo != NULL ) {
        
        // load it
        hRes = LoadResource( hModule, hResInfo );

        if( hRes != NULL ) {

            // get size and ptr
            sRes = SizeofResource( hModule, hResInfo );
            pRes = LockResource( hRes );

            bUpdated = UpdateResourceW( hTarget, lpType, lpName, wLang, pRes, sRes );    
        }
        else {
            //printf( "Unable to LoadResource() : %d\n", GetLastError() );
        }
        
        // free it up
        FreeResource( hRes );
    }
    else {
        //printf( "Unable to FindResourceExW() : %d\n", GetLastError() );
    }

    //printf( "Updated? %s", (bUpdated) ? "true\n" : "false\n" );
    
    if( !bUpdated ) {
        bFailed = true;
    }

    return TRUE;
}
/*

If you are looking at the source code here is a little easteregg

This is as close as I got to doing this with Meterpreter's Railgun:

client.railgun.add_function('kernel32', 'UpdateResourceA', 'BOOL',[
                        ["HANDLE","hUpdate","in"],
                        ["WORD","lpType","in"],
                        ["WORD","lpName","in"],
                        ["WORD","wLanguage","in"],
                        ["DWORD","lpData","in"],
                        ["DWORD","cb","in"],
                        ])


print_status("Running LoadLibraryExA")
b = client.railgun.kernel32.LoadLibraryExA('notepad.exe', nil, 'LOAD_LIBRARY_AS_DATAFILE')
print_good(b.inspect)

print_status("Running FindResourceExA")
c = client.railgun.kernel32.FindResourceA(b["return"], '#3', '#4')
print_good(c.inspect)

print_status("Running SizeofResource")
s = client.railgun.kernel32.SizeofResource(b["return"], c["return"])
print_good(s.inspect)

print_status("Running LoadResource")
d = client.railgun.kernel32.LoadResource(b["return"], c["return"])
print_good(d.inspect)

print_status("Running LockResource")
e = client.railgun.kernel32.LockResource(d["return"])
print_good(e.inspect)

print_status("Reading Icon from Memory")
# aa = client.railgun.memread(e["return"], s["return"])
# print_status(aa.inspect)
# print_good("Size of byte array: #{aa.size.to_s}")

print_status("Running BeginUpdateResourceA")
f = client.railgun.kernel32.BeginUpdateResourceA('bob.exe', false)
print_error(f.inspect)

print_status("Running UpdateResourceA")
g = client.railgun.kernel32.UpdateResourceA(f["return"], 'Icon', '2', 1033, e["return"], s["return"])
print_error(g.inspect)

print_status("Running EndUpdateResourceA")
h = client.railgun.kernel32.EndUpdateResourceA(f["return"], false)
print_error(h.inspect)

*/