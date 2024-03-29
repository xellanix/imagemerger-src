#include "MergeContextMenuExt.h"
#include "resource.h"
#include <strsafe.h>
#include <Shlwapi.h>
#include "common.h"
#include <unordered_set>
#include <algorithm>
#pragma comment(lib, "shlwapi.lib")

extern long g_cDllRef;

#define IDM_DISPLAY             0  // The command's identifier offset

HMODULE GetCurrentModule()
{ // NB: XP+ solution!
    HMODULE hModule = NULL;
    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCTSTR)GetCurrentModule,
        &hModule);

    return hModule;
}

MergeContextMenuExt::MergeContextMenuExt(void)
    : m_cRef(1),
    m_pszMenuText(PWSTR(L_Menu_Text)),
    m_pszVerb(Verb_Name),
    m_pwszVerb(L_Verb_Name),
    m_pszVerbCanonicalName(Verb_Canonical_Name),
    m_pwszVerbCanonicalName(L_Verb_Canonical_Name),
    m_pszVerbHelpText(Verb_Help_Text),
    m_pwszVerbHelpText(L_Verb_Help_Text)
{
    InterlockedIncrement(&g_cDllRef);

    // Load the bitmap for the menu item. 
    // If you want the menu item bitmap to be transparent, the color depth of 
    // the bitmap must not be greater than 8bpp.
    m_hMenuBmp = LoadImage(GetCurrentModule(), MAKEINTRESOURCE(IDB_ICON),
                           IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | LR_LOADTRANSPARENT | LR_CREATEDIBSECTION);
}

MergeContextMenuExt::~MergeContextMenuExt(void)
{
    if (m_hMenuBmp)
    {
        DeleteObject(m_hMenuBmp);
        m_hMenuBmp = NULL;
    }

    InterlockedDecrement(&g_cDllRef);
}


void MergeContextMenuExt::OnVerbMergeFiles()
{
    wchar_t szModule[MAX_PATH];
    if (GetModuleFileName(GetCurrentModule(), szModule, ARRAYSIZE(szModule)) == 0)
    {
        return;
    }

    std::wstring app = szModule;
    app = app.substr(0, app.size() - __builtin_wcslen(PROJECT_NAME)) + L"ImageMerger.exe";
    std::wstring params = L"--nw";

    for (auto const& file : m_vSelectedFiles)
    {
        params += L" \"" + file + L"\"";
    }

    ShellExecuteW(NULL, NULL, app.c_str(), params.c_str(), NULL, SW_SHOWNORMAL);
}


#pragma region IUnknown

// Query to the interface the component supported.
IFACEMETHODIMP MergeContextMenuExt::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(MergeContextMenuExt, IContextMenu),
        QITABENT(MergeContextMenuExt, IShellExtInit),
        { 0 },
    };
    return QISearch(this, qit, riid, ppv);
}

// Increase the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) MergeContextMenuExt::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

// Decrease the reference count for an interface on an object.
IFACEMETHODIMP_(ULONG) MergeContextMenuExt::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (0 == cRef)
    {
        delete this;
    }

    return cRef;
}

#pragma endregion

#pragma region IShellExtInit

// Initialize the context menu handler.
IFACEMETHODIMP MergeContextMenuExt::Initialize(
    LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID)
{
    if (NULL == pDataObj)
    {
        return E_INVALIDARG;
    }

    HRESULT hr = E_FAIL;

    FORMATETC fe = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stm;

    // The pDataObj pointer contains the objects being acted upon. In this 
    // example, we get an HDROP handle for enumerating the selected files and 
    // folders.
    if (SUCCEEDED(pDataObj->GetData(&fe, &stm)))
    {
        // Get an HDROP handle.
        HDROP hDrop = static_cast<HDROP>(GlobalLock(stm.hGlobal));
        if (hDrop != NULL)
        {
            // Determine how many files are involved in this operation. This 
            // code sample displays the custom context menu item when only 
            // one file is selected. 
            UINT nFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            for (size_t i = 0; i < nFiles; i++)
            {
                wchar_t szSelectedFile[MAX_PATH] = { 0 };
                // Get the path of the file.
                if (0 != DragQueryFile(hDrop, i, szSelectedFile, ARRAYSIZE(szSelectedFile)))
                {
                    m_vSelectedFiles.push_back(szSelectedFile);
                    hr = S_OK;
                    continue;
                }
                hr = E_FAIL;
                break;
            }

            GlobalUnlock(stm.hGlobal);
        }

        ReleaseStgMedium(&stm);
    }

    if (S_OK == hr)
    {
        auto getExt = [](std::wstring const& file)
        {
            auto pos = file.rfind(L".");
            if (pos == std::wstring::npos) return std::wstring();

            auto retval = file.substr(pos);
            std::transform(retval.begin(), retval.end(), retval.begin(), ::towlower);

            return retval;
        };

        for (auto file = m_vSelectedFiles.cbegin(); file != m_vSelectedFiles.cend(); ++file)
        {
            std::wstring ext = getExt(*file);
            if (ext.empty())
            {
                hr = E_INVALIDARG;
                break;
            }

            std::unordered_set<std::wstring> types{ L_Associated_Types };
            if (types.find(ext) == types.end())
            {
                hr = E_INVALIDARG;
                break;
            }
        }
    }

    // If any value other than S_OK is returned from the method, the context 
    // menu item is not displayed.
    return hr;
}

#pragma endregion


#pragma region IContextMenu

//
//   FUNCTION: MergeContextMenuExt::QueryContextMenu
//
//   PURPOSE: The Shell calls IContextMenu::QueryContextMenu to allow the 
//            context menu handler to add its menu items to the menu. It 
//            passes in the HMENU handle in the hmenu parameter. The 
//            indexMenu parameter is set to the index to be used for the 
//            first menu item that is to be added.
//
IFACEMETHODIMP MergeContextMenuExt::QueryContextMenu(
    HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags)
{
    // If uFlags include CMF_DEFAULTONLY then we should not do anything.
    if (CMF_DEFAULTONLY & uFlags)
    {
        return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(0));
    }

    // Use either InsertMenu or InsertMenuItem to add menu items.
    // Learn how to add sub-menu from:
    // http://www.codeproject.com/KB/shell/ctxextsubmenu.aspx

    MENUITEMINFOW mii = { sizeof(mii) };
    mii.fMask = MIIM_BITMAP | MIIM_STRING | MIIM_FTYPE | MIIM_ID | MIIM_STATE;
    mii.wID = idCmdFirst + IDM_DISPLAY;
    mii.fType = MFT_STRING;
    mii.dwTypeData = m_pszMenuText;
    mii.fState = MFS_ENABLED;
    mii.hbmpItem = static_cast<HBITMAP>(m_hMenuBmp);
    if (!InsertMenuItem(hMenu, indexMenu + 1, TRUE, &mii))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Add a separator.
    MENUITEMINFOW sep = { sizeof(sep) };
    sep.fMask = MIIM_TYPE;
    sep.fType = MFT_SEPARATOR;
    if (!InsertMenuItem(hMenu, indexMenu, TRUE, &sep))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Add a separator.
    MENUITEMINFOW sep2 = { sizeof(sep2) };
    sep2.fMask = MIIM_TYPE;
    sep2.fType = MFT_SEPARATOR;
    if (!InsertMenuItem(hMenu, indexMenu + 2, TRUE, &sep2))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Return an HRESULT value with the severity set to SEVERITY_SUCCESS. 
    // Set the code value to the offset of the largest command identifier 
    // that was assigned, plus one (1).
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, USHORT(IDM_DISPLAY + 1));
}


//
//   FUNCTION: MergeContextMenuExt::InvokeCommand
//
//   PURPOSE: This method is called when a user clicks a menu item to tell 
//            the handler to run the associated command. The lpcmi parameter 
//            points to a structure that contains the needed information.
//
IFACEMETHODIMP MergeContextMenuExt::InvokeCommand(LPCMINVOKECOMMANDINFO pici)
{
    BOOL fUnicode = FALSE;

    // Determine which structure is being passed in, CMINVOKECOMMANDINFO or 
    // CMINVOKECOMMANDINFOEX based on the cbSize member of lpcmi. Although 
    // the lpcmi parameter is declared in Shlobj.h as a CMINVOKECOMMANDINFO 
    // structure, in practice it often points to a CMINVOKECOMMANDINFOEX 
    // structure. This struct is an extended version of CMINVOKECOMMANDINFO 
    // and has additional members that allow Unicode strings to be passed.
    if (pici->cbSize == sizeof(CMINVOKECOMMANDINFOEX))
    {
        if (pici->fMask & CMIC_MASK_UNICODE)
        {
            fUnicode = TRUE;
        }
    }

    // Determines whether the command is identified by its offset or verb.
    // There are two ways to identify commands:
    // 
    //   1) The command's verb string 
    //   2) The command's identifier offset
    // 
    // If the high-order word of lpcmi->lpVerb (for the ANSI case) or 
    // lpcmi->lpVerbW (for the Unicode case) is nonzero, lpVerb or lpVerbW 
    // holds a verb string. If the high-order word is zero, the command 
    // offset is in the low-order word of lpcmi->lpVerb.

    // For the ANSI case, if the high-order word is not zero, the command's 
    // verb string is in lpcmi->lpVerb. 
    if (!fUnicode && HIWORD(pici->lpVerb))
    {
        // Is the verb supported by this context menu extension?
        if (StrCmpIA(pici->lpVerb, m_pszVerb) == 0)
        {
            OnVerbMergeFiles();
        }
        else
        {
            // If the verb is not recognized by the context menu handler, it 
            // must return E_FAIL to allow it to be passed on to the other 
            // context menu handlers that might implement that verb.
            return E_FAIL;
        }
    }

    // For the Unicode case, if the high-order word is not zero, the 
    // command's verb string is in lpcmi->lpVerbW. 
    else if (fUnicode && HIWORD(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW))
    {
        // Is the verb supported by this context menu extension?
        if (StrCmpIW(((CMINVOKECOMMANDINFOEX*)pici)->lpVerbW, m_pwszVerb) == 0)
        {
            OnVerbMergeFiles();
        }
        else
        {
            // If the verb is not recognized by the context menu handler, it 
            // must return E_FAIL to allow it to be passed on to the other 
            // context menu handlers that might implement that verb.
            return E_FAIL;
        }
    }

    // If the command cannot be identified through the verb string, then 
    // check the identifier offset.
    else
    {
        // Is the command identifier offset supported by this context menu 
        // extension?
        if (LOWORD(pici->lpVerb) == IDM_DISPLAY)
        {
            OnVerbMergeFiles();
        }
        else
        {
            // If the verb is not recognized by the context menu handler, it 
            // must return E_FAIL to allow it to be passed on to the other 
            // context menu handlers that might implement that verb.
            return E_FAIL;
        }
    }

    return S_OK;
}


//
//   FUNCTION: CMergeContextMenuExt::GetCommandString
//
//   PURPOSE: If a user highlights one of the items added by a context menu 
//            handler, the handler's IContextMenu::GetCommandString method is 
//            called to request a Help text string that will be displayed on 
//            the Windows Explorer status bar. This method can also be called 
//            to request the verb string that is assigned to a command. 
//            Either ANSI or Unicode verb strings can be requested. This 
//            example only implements support for the Unicode values of 
//            uFlags, because only those have been used in Windows Explorer 
//            since Windows 2000.
//
IFACEMETHODIMP MergeContextMenuExt::GetCommandString(UINT_PTR idCommand,
                                                    UINT uFlags, UINT* pwReserved, LPSTR pszName, UINT cchMax)
{
    HRESULT hr = E_INVALIDARG;

    if (idCommand == IDM_DISPLAY)
    {
        switch (uFlags)
        {
            case GCS_HELPTEXTW:
                // Only useful for pre-Vista versions of Windows that have a 
                // Status bar.
                hr = StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax,
                                   m_pwszVerbHelpText);
                break;

            case GCS_VERBW:
                // GCS_VERBW is an optional feature that enables a caller to 
                // discover the canonical name for the verb passed in through 
                // idCommand.
                hr = StringCchCopy(reinterpret_cast<PWSTR>(pszName), cchMax,
                                   m_pwszVerbCanonicalName);
                break;

            default:
                hr = S_OK;
        }
    }

    // If the command (idCommand) is not supported by this context menu 
    // extension handler, return E_INVALIDARG.

    return hr;
}

#pragma endregion
