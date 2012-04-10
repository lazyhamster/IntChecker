#ifndef hashing_h__
#define hashing_h__

#include "rhash/librhash/rhash.h"

struct FileHashInfo
{
	std::wstring Filename;
	uint8_t HashData[32];
	bool IsHashed;
};

class Hasher
{
private:
	std::wstring m_BasePath;
	rhash_ids m_HashId;
	std::list<FileHashInfo> m_HashList;

	std::wstring normalizePath(const wchar_t* input);

public:
	Hasher(const wchar_t* basePath, rhash_ids hashId);

	void AddFile(const wchar_t* relativePath);
	void AddDir(const wchar_t* relativePath);

	bool SaveList(const wchar_t* filepath);
	int LoadList(const wchar_t* filepath);
};

#endif // hashing_h__
