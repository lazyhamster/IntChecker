#ifndef hashing_h__
#define hashing_h__

#include "rhash/librhash/rhash.h"

struct FileHashInfo
{
	std::wstring Filename;
	std::string HashStr;
};

class HashList
{
private:
	rhash_ids m_HashId;
	std::vector<FileHashInfo> m_HashList;

public:
	HashList(rhash_ids hashId) : m_HashId(hashId) {}

	bool SaveList(const wchar_t* filepath);
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
int PrepareFilesList(const wchar_t* basePath, StringList &destList, bool recursive);

#endif // hashing_h__
