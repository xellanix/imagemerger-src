#pragma once

#include <windows.h>
#include <shlobj.h>     // For IShellExtInit and IContextMenu
#include <vector>
#include <string>


class MergeContextMenuExt : public IShellExtInit, public IContextMenu
{
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IShellExtInit
    IFACEMETHODIMP Initialize(LPCITEMIDLIST pidlFolder, LPDATAOBJECT pDataObj, HKEY hKeyProgID);

    // IContextMenu
    IFACEMETHODIMP QueryContextMenu(HMENU hMenu, UINT indexMenu, UINT idCmdFirst, UINT idCmdLast, UINT uFlags);
    IFACEMETHODIMP InvokeCommand(LPCMINVOKECOMMANDINFO pici);
    IFACEMETHODIMP GetCommandString(UINT_PTR idCommand, UINT uFlags, UINT* pwReserved, LPSTR pszName, UINT cchMax);

    MergeContextMenuExt(void);

protected:
    ~MergeContextMenuExt(void);

private:
    // Reference count of component.
    long m_cRef;

    // The name of the selected files.
    std::vector<std::wstring> m_vSelectedFiles;

    // The method that handles the "display" verb.
    void OnVerbMergeFiles();

    PWSTR m_pszMenuText;
    HANDLE m_hMenuBmp;
    PCSTR m_pszVerb;
    PCWSTR m_pwszVerb;
    PCSTR m_pszVerbCanonicalName;
    PCWSTR m_pwszVerbCanonicalName;
    PCSTR m_pszVerbHelpText;
    PCWSTR m_pwszVerbHelpText;
};
