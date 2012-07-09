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

// Params: context, processed bytes, total bytes
typedef int (CALLBACK *HashingProgressFunc)(HANDLE, int64_t, int64_t);

bool GenerateHash(const wchar_t* filePath, rhash_ids hashAlgo, char* result, HashingProgressFunc progressFunc, HANDLE progressContext);

#endif // hashing_h__
