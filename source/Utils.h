#ifndef Utils_h__
#define Utils_h__

class FarScreenSave
{
private:
	HANDLE hScreen;

public:
	FarScreenSave();
	~FarScreenSave();
};

bool ArePanelsComparable();
bool CheckEsc();
bool IsAbsPath(const wchar_t* path);
void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize);
void IncludeTrailingPathDelim(wstring &pathStr);
int64_t GetFileSize_i64(const wchar_t* path);
int64_t GetFileSize_i64(HANDLE hFile);

#endif // Utils_h__