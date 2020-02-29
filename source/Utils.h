#ifndef Utils_h__
#define Utils_h__

bool CheckEsc();

void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize);
void IncludeTrailingPathDelim(std::wstring &pathStr);
std::wstring ExtractFileName(const std::wstring& fullPath);
std::wstring ExtractFileExt(const std::wstring& path);
std::wstring PrependLongPrefix(const std::wstring &basePath);

int64_t GetFileSize_i64(const wchar_t* path);
int64_t GetFileSize_i64(HANDLE hFile);
bool IsFile(const std::wstring &path, int64_t *fileSize = nullptr);
bool CanCreateFile(const wchar_t* path);

void TrimRight(char* str);
void TrimStr(std::string &str);
void TrimLeft(wchar_t* str);
void TrimRight(wchar_t* str);
void TrimStr(wchar_t* str);

bool SameText(const wchar_t* str1, const wchar_t* str2);

typedef bool (CALLBACK *FilterCompareProc)(const WIN32_FIND_DATA*, HANDLE);
int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, int64_t &totalSize, bool recursive, FilterCompareProc filterProc, HANDLE filterData);

bool CopyTextToClipboard(std::wstring &data);
bool CopyTextToClipboard(std::vector<std::wstring> &data);
bool GetTextFromClipboard(std::string &data);

std::wstring FormatString(const std::wstring fmt, ...);
std::wstring ConvertToUnicode(const std::string &str, int cp);

typedef std::chrono::steady_clock clock_type;

class time_check
{
public:
	enum class mode { delayed, immediate };
	time_check(mode Mode, clock_type::duration Interval) :
		m_Begin(Mode == mode::delayed ? clock_type::now() : clock_type::now() - Interval),
		m_Interval(Interval) {}

	void reset(clock_type::time_point Value = clock_type::now()) { m_Begin = Value; }

	bool operator!()
	{
		const auto Current = clock_type::now();
		if (Current - m_Begin > m_Interval)
		{
			reset(Current);
			return false;
		}
		return true;
	}

private:
	mutable clock_type::time_point m_Begin;
	const clock_type::duration m_Interval;
};

#endif // Utils_h__