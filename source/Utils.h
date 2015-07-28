#ifndef Utils_h__
#define Utils_h__

bool CheckEsc();

std::wstring MakeAbsPath(const std::wstring path, const std::wstring refDir);
void IncludeTrailingPathDelim(wchar_t *pathBuf, size_t bufMaxSize);
void IncludeTrailingPathDelim(wstring &pathStr);
std::wstring ExtractFileName(const std::wstring& fullPath);
std::wstring ExtractFileExt(const std::wstring& path);

int64_t GetFileSize_i64(const wchar_t* path);
int64_t GetFileSize_i64(HANDLE hFile);
bool IsFile(const wchar_t* path);
bool CanCreateFile(const wchar_t* path);

void TrimRight(char* str);
void TrimStr(std::string &str);

int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, int64_t &totalSize, bool recursive);

bool CopyTextToClipboard(std::wstring &data);
bool CopyTextToClipboard(std::vector<std::wstring> &data);
bool GetTextFromClipboard(std::string &data);

std::wstring FormatString(const std::wstring fmt, ...);
std::wstring ConvertToUnicode(std::string &str, int cp);

#endif // Utils_h__