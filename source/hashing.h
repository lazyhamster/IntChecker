#ifndef hashing_h__
#define hashing_h__

#include "rhash/librhash/rhash.h"

struct FileHashInfo
{
	std::wstring Filename;
	std::string HashStr;
};

struct HashAlgoInfo
{
	rhash_ids AlgoId;
	int HashStrSize;
	bool HashBeforePath;
	int NumDelimSpaces;
	std::wstring DefaultExt;
	std::wstring AlgoName;
};

class HashList
{
private:
	rhash_ids m_HashId;
	std::vector<FileHashInfo> m_HashList;
	int m_Codepage;

	int GetFileRecordIndex(const wchar_t* fileName) const;
	bool DumpStringToFile(const char* data, size_t dataSize, const wchar_t* filePath);
	void SerializeFileHash(const FileHashInfo& data, stringstream& dest);
	HashAlgoInfo* DetectHashAlgo(const char* testStr);

public:
	HashList(rhash_ids hashId, int codePage) : m_HashId(hashId), m_Codepage(codePage) {}
	HashList(rhash_ids hashId) : m_HashId(hashId), m_Codepage(CP_UTF8) {}
	HashList() : m_HashId(RHASH_HASH_COUNT), m_Codepage(CP_UTF8) {}

	bool SaveList(const wchar_t* filepath);
	bool SaveListSeparate(const wchar_t* baseDir);
	bool LoadList(const wchar_t* filepath);

	std::string GetFileHash(const wchar_t* FileName) const;
	void SetFileHash(const wchar_t* FileName, std::string HashVal);
	
	size_t GetCount() const { return m_HashList.size(); }
	FileHashInfo GetFileInfo(size_t index) { return m_HashList.at(index); }
	rhash_ids GetHashAlgo() const { return m_HashId; }
	std::wstring FileInfoToString(size_t index);
};

// Params: context, processed bytes
typedef bool (CALLBACK *HashingProgressFunc)(HANDLE, int64_t);

#define GENERATE_SUCCESS 0
#define GENERATE_ERROR 1
#define GENERATE_ABORTED 2

int GenerateHash(const wchar_t* filePath, rhash_ids hashAlgo, char* result, HashingProgressFunc progressFunc, HANDLE progressContext);
HashAlgoInfo* GetAlgoInfo(rhash_ids algoId);

#define NUMBER_OF_SUPPORTED_HASHES 6
extern HashAlgoInfo SupportedHashes[NUMBER_OF_SUPPORTED_HASHES];

#endif // hashing_h__
