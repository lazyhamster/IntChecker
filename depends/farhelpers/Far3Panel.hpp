#ifndef Far3Panel_h__
#define Far3Panel_h__

#ifndef __cplusplus
#error C++ only
#endif

#include <string>
#include <filesystem>
#include <functional>

// Returns:
//    1 - set item selection
//    0 - leave unchanged
//   -1 - remove item selection
typedef std::function<int(const wchar_t* aItemName)> ItemSelectionPredicate;

namespace fs = std::filesystem;

static std::wstring PathJoin(const std::wstring& path1, const std::wstring& path2)
{
	fs::path p1(path1), p2(path2);
	fs::path fullPath = p1 / p2;

	return fullPath.native();
}

struct PanelFileInfo
{
	std::wstring FullPath;
	std::wstring PanelPath;
	int64_t Size;
};

class Far3Panel
{
protected:
	const PluginStartupInfo* m_SInfo;

	HANDLE m_hPanel;
	PanelInfo m_panelInfo;
	bool m_fIsValid;

	intptr_t PanelControl(enum FILE_CONTROL_COMMANDS Command, intptr_t Param1, void* Param2)
	{
		return m_SInfo->PanelControl(m_hPanel, Command, Param1, Param2);
	}

	bool PathMatchFileFilter(const PluginPanelItem* item, HANDLE fileFilter)
	{
		if (fileFilter == NULL || fileFilter == INVALID_HANDLE_VALUE)
			return true;
			
		return m_SInfo->FileFilterControl(fileFilter, FFCTL_ISFILEINFILTER, 0, (void *)item) != FALSE;
	}

	bool PathMatchFileFilter(const std::wstring& path, HANDLE fileFilter)
	{
		if (fileFilter == NULL || fileFilter == INVALID_HANDLE_VALUE)
			return true;

		WIN32_FIND_DATA winData = { 0 };
		HANDLE hFind = FindFirstFile(path.c_str(), &winData);
		if (hFind == INVALID_HANDLE_VALUE) return false;
						
		PluginPanelItem item = { 0 };
		item.FileName = winData.cFileName;
		item.AlternateFileName = winData.cAlternateFileName;
		item.FileSize = winData.nFileSizeLow + ((unsigned long long)winData.nFileSizeHigh << 32);
		item.FileAttributes = winData.dwFileAttributes;
		item.CreationTime = winData.ftCreationTime;
		item.LastWriteTime = winData.ftLastWriteTime;
		item.LastAccessTime = winData.ftLastAccessTime;

		FindClose(hFind);

		return PathMatchFileFilter(&item, fileFilter);
	}

public:
	Far3Panel(const PluginStartupInfo* SInfo, const HANDLE hPanel)
	{
		m_SInfo = SInfo;
		m_hPanel = hPanel;
		m_panelInfo = { sizeof(PanelInfo), 0 };

		Refresh();
	}

	void Refresh()
	{
		m_fIsValid = m_SInfo->PanelControl(PANEL_ACTIVE, FCTL_GETPANELINFO, 0, &m_panelInfo);
	}

	bool IsValid() { return m_fIsValid; }
	HANDLE GetHandle() { return m_hPanel; }
	
	bool HasSelectedItems() { return m_panelInfo.SelectedItemsNumber > 0; }
	size_t GetSelectedItemsNumber() { return m_panelInfo.SelectedItemsNumber; }
	
	bool IsReadablePanel()
	{
		return (m_panelInfo.PanelType == PTYPE_FILEPANEL) && ((m_panelInfo.PluginHandle == nullptr) || ((m_panelInfo.Flags & PFLAGS_REALNAMES) != 0));
	}

	void RedrawPanel()
	{
		PanelControl(FCTL_REDRAWPANEL, 0, NULL);
	}

	std::wstring GetPanelDirectory()
	{
		if (!m_fIsValid) return L"";
		
		std::wstring dirStr;

		size_t nBufSize = PanelControl(FCTL_GETPANELDIRECTORY, 0, NULL);
		FarPanelDirectory *panelDir = (FarPanelDirectory*)malloc(nBufSize);
		panelDir->StructSize = sizeof(FarPanelDirectory);
		if (PanelControl(FCTL_GETPANELDIRECTORY, nBufSize, panelDir))
		{
			dirStr.assign(panelDir->Name);
		}
		
		free(panelDir);
		return dirStr;
	}

	std::wstring GetSelectedItemPath()
	{
		std::wstring strPath;
		if (m_fIsValid)
		{
			size_t itemBufSize = PanelControl(FCTL_GETSELECTEDPANELITEM, 0, NULL);
			PluginPanelItem *PPI = (PluginPanelItem*)malloc(itemBufSize);
			if (PPI)
			{
				FarGetPluginPanelItem FGPPI = { sizeof(FarGetPluginPanelItem), itemBufSize, PPI };
				PanelControl(FCTL_GETSELECTEDPANELITEM, 0, &FGPPI);
				strPath = PathJoin(GetPanelDirectory(), PPI->FileName);
				free(PPI);
			}
		}
		return strPath;
	}

	void GetSelectedFiles(bool recursive, HANDLE fileFilter, std::vector<PanelFileInfo>& selectedFiles)
	{
		std::wstring strPanelPath = GetPanelDirectory();
		
		for (size_t i = 0; i < m_panelInfo.SelectedItemsNumber; ++i)
		{
			size_t nRequiredBytes = PanelControl(FCTL_GETSELECTEDPANELITEM, i, NULL);
			PluginPanelItem *PPI = static_cast<PluginPanelItem*>(malloc(nRequiredBytes));
			if (PPI)
			{
				FarGetPluginPanelItem FGPPI = { sizeof(FarGetPluginPanelItem), nRequiredBytes, PPI };
				PanelControl(FCTL_GETSELECTEDPANELITEM, i, &FGPPI);
				if (wcscmp(PPI->FileName, L"..") && wcscmp(PPI->FileName, L".") && PathMatchFileFilter(PPI, fileFilter))
				{
					auto itemPath = PathJoin(strPanelPath, PPI->FileName);
					
					if ((PPI->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
					{
						PanelFileInfo info;
						info.FullPath = itemPath;
						info.PanelPath = PPI->FileName;
						info.Size = PPI->FileSize;
						
						selectedFiles.push_back(info);
					}
					else if (recursive)
					{
						for (auto& p : std::filesystem::recursive_directory_iterator(itemPath, fs::directory_options::skip_permission_denied))
							if (!p.is_directory())
							{
								auto strEntryPath = p.path().native();
								std::wstring strRelativePath = strEntryPath.substr(strPanelPath.size());
								if (strRelativePath[0] == '\\') strRelativePath.erase(0, 1);
								
								if (PathMatchFileFilter(strEntryPath, fileFilter))
								{
									PanelFileInfo info;
									info.FullPath = strEntryPath;
									info.PanelPath = strRelativePath;
									info.Size = p.file_size();

									selectedFiles.push_back(info);
								}
							}
					}
				}
				free(PPI);
			}
		}
	}

	void SetItemsSelection(ItemSelectionPredicate selectPred)
	{
		if (!m_fIsValid) return;

		PanelControl(FCTL_BEGINSELECTION, 0, NULL);

		for (size_t i = 0; i < m_panelInfo.ItemsNumber; ++i)
		{
			size_t nSize = PanelControl(FCTL_GETPANELITEM, i, NULL);
			PluginPanelItem *PPI = (PluginPanelItem*) malloc(nSize);
			if (PPI)
			{
				FarGetPluginPanelItem gppi = { sizeof(FarGetPluginPanelItem), nSize, PPI };
				PanelControl(FCTL_GETPANELITEM, i, &gppi);
				int nNewSelection = selectPred(PPI->FileName);
				if (nNewSelection != 0)
				{
					PanelControl(FCTL_SETSELECTION, i, (void*)(UINT_PTR)(nNewSelection > 0 ? TRUE : FALSE));
				}
				free(PPI);
			}
		}

		PanelControl(FCTL_ENDSELECTION, 0, NULL);
		PanelControl(FCTL_REDRAWPANEL, 0, NULL);
	}

	void ClearSelection()
	{
		if (!m_fIsValid) return;

		PanelControl(FCTL_BEGINSELECTION, 0, NULL);

		for (size_t i = 0; i < m_panelInfo.SelectedItemsNumber; ++i)
		{
			PanelControl(FCTL_CLEARSELECTION, i, NULL);
		}

		PanelControl(FCTL_ENDSELECTION, 0, NULL);
		PanelControl(FCTL_REDRAWPANEL, 0, NULL);
	}
};

#endif // Far3Panel_h__
