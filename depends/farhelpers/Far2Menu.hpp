#ifndef Far2Menu_h__
#define Far2Menu_h__

#ifndef __cplusplus
#error C++ only
#endif

#include <boost/function.hpp>
#include <boost/optional.hpp>

typedef boost::function<void (DWORD_PTR aItemUserData)> MenuAction;

#define SET_OPT_STR(str, val) if (val) str = std::wstring(val); else str = boost::none
#define OPT_STR_VAL(str) (str ? str->c_str() : nullptr)

class FarMenu
{
protected:
	const PluginStartupInfo* m_SInfo;
	
	FARMENUFLAGS m_Flags;
	intptr_t m_MaxHeight;
	boost::optional<std::wstring> m_Title;
	boost::optional<std::wstring> m_Bottom;
	boost::optional<std::wstring> m_HelpTopic;

	std::vector<FarMenuItemEx> m_Items;
	std::vector<MenuAction> m_Actions;

public:
	FarMenu(const PluginStartupInfo* SInfo, const wchar_t* aTitle, const wchar_t* aBottom = nullptr)
	{
		m_SInfo = SInfo;
		
		SET_OPT_STR(m_Title, aTitle);
		SET_OPT_STR(m_Bottom, aBottom);

		m_Flags = FMENU_USEEXT;
		m_MaxHeight = 0;
		m_HelpTopic = boost::none;
	}

	void SetMaxHeight(intptr_t aMaxHeight) { m_MaxHeight = aMaxHeight; }
	void SetFlags(FARMENUFLAGS aFlags) { m_Flags = aFlags; }
	void SetHelpTopic(const wchar_t* aHelpTopic) { SET_OPT_STR(m_HelpTopic, aHelpTopic); }

	void AddItem(const wchar_t* aText, MENUITEMFLAGS aFlags = (MENUITEMFLAGS) 0, DWORD_PTR aUserData = 0)
	{
		AddItemEx(aText, NULL, aFlags, aUserData);
	}

	void AddItemEx(const wchar_t* aText, MenuAction aAction, MENUITEMFLAGS aFlags = (MENUITEMFLAGS) 0, DWORD_PTR aUserData = 0)
	{
		FarMenuItemEx item = {0};

		item.Text = aText;
		item.Flags = aFlags | FMENU_USEEXT;
		item.UserData = aUserData;

		m_Items.push_back(item);
		m_Actions.push_back(aAction);
	}

	void AddSeparator()
	{
		AddItem(nullptr, MIF_SEPARATOR);
	}

	void Clear() { m_Items.clear(); m_Actions.clear(); }

	size_t ItemCount() { return m_Items.size(); }

	// Returns index of selected menu item
	intptr_t Run()
	{
		return m_SInfo->Menu(m_SInfo->ModuleNumber, -1, -1, (int) m_MaxHeight, m_Flags, OPT_STR_VAL(m_Title), OPT_STR_VAL(m_Bottom), OPT_STR_VAL(m_HelpTopic), nullptr, nullptr, (FarMenuItem*) &m_Items[0], (int) m_Items.size());
	}

	bool RunEx()
	{
		intptr_t rc = Run();
		if (rc >= 0)
		{
			// Execute binded action
			if (rc <= (intptr_t) m_Actions.size())
			{
				MenuAction &action = m_Actions[rc];
				FarMenuItemEx &item = m_Items[rc];
				if (action) action(item.UserData);
			}
			return true;
		}

		return false;
	}
};

#endif // Far2Menu_h__
