#ifndef hashing_h__
#define hashing_h__

#include "rhash/librhash/rhash.h"

struct FileHashInfo
{
	std::wstring Filename;
	std::string HashStr;
};

typedef std::map<std::wstring, std::string> StringMap;

class HashList
{
private:
	rhash_ids m_HashId;
	StringMap m_HashList;
	int m_Codepage;

public:
	HashList(rhash_ids hashId, int codePage) : m_HashId(hashId), m_Codepage(codePage) {}
	HashList(rhash_ids hashId) : m_HashId(hashId), m_Codepage(CP_UTF8) {}

	bool SaveList(const wchar_t* filepath);
	bool SaveListSeparate(const wchar_t* baseDir);
	int LoadList(const wchar_t* filepath, bool replaceExisting = true);

	std::string GetFileHash(const wchar_t* FileName) const;
	void SetFileHash(const wchar_t* FileName, std::string HashVal);
};

// Params: context, processed bytes
typedef bool (CALLBACK *HashingProgressFunc)(HANDLE, int64_t);

#define GENERATE_SUCCESS 0
#define GENERATE_ERROR 1
#define GENERATE_ABORTED 2

int GenerateHash(const wchar_t* filePath, rhash_ids hashAlgo, char* result, HashingProgressFunc progressFunc, HANDLE progressContext);
int PrepareFilesList(const wchar_t* basePath, const wchar_t* basePrefix, StringList &destList, bool recursive);

#endif // hashing_h__
