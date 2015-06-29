#ifndef hashing_h__
#define hashing_h__

#include "rhash/librhash/rhash.h"

enum HashListFormat
{
	HLF_UNKNOWN,
	HLF_SIMPLE,
	HLF_BSD
};

struct HashAlgoInfo
{
	rhash_ids AlgoId;
	std::wstring AlgoName;
	std::wstring DefaultExt;
	std::string ParseExpr;
	short HashStrSize;
};

#define NUMBER_OF_SUPPORTED_HASHES 6
extern HashAlgoInfo SupportedHashes[NUMBER_OF_SUPPORTED_HASHES];

struct FileHashInfo
{
	std::wstring Filename;
	std::string HashStr;
	int HashAlgoIndex;

	rhash_ids GetAlgo() const { return (HashAlgoIndex >= 0) ? SupportedHashes[HashAlgoIndex].AlgoId : RHASH_HASH_COUNT; }
};

#define DEFAULT_HASHLIST_SIZE_LIMIT 10 * 1024 * 1024

class HashList
{
private:
	std::vector<FileHashInfo> m_HashList;
	int m_Codepage;
	int64_t m_MaxListSize;

	int GetFileRecordIndex(const wchar_t* fileName) const;
	bool DumpStringToFile(const char* data, size_t dataSize, const wchar_t* filePath);
	void SerializeFileHash(const FileHashInfo& data, stringstream& dest) const;
	bool DetectHashAlgo(const char* testStr, const wchar_t* filePath, int &foundAlgoIndex, HashListFormat &listFormat);
	bool TryParseBSD(const char* inputStr, FileHashInfo &fileInfo);
	bool TryParseSimple(const char* inputStr, int hashAlgoIndex, FileHashInfo &fileInfo);

public:
	HashList(int codePage) : m_Codepage(codePage), m_MaxListSize(DEFAULT_HASHLIST_SIZE_LIMIT) {}
	HashList() : m_Codepage(CP_UTF8), m_MaxListSize(DEFAULT_HASHLIST_SIZE_LIMIT) {}

	bool SaveList(const wchar_t* filepath);
	bool SaveListSeparate(const wchar_t* baseDir);
	bool LoadList(const wchar_t* filepath);

	void SetFileHash(const wchar_t* fileName, std::string hashVal, rhash_ids hashAlgo);
	
	size_t GetCount() const { return m_HashList.size(); }
	FileHashInfo GetFileInfo(size_t index) { return m_HashList.at(index); }
	std::wstring FileInfoToString(size_t index) const;
};

// Params: context, processed bytes
typedef bool (CALLBACK *HashingProgressFunc)(HANDLE, int64_t);

#define GENERATE_SUCCESS 0
#define GENERATE_ERROR 1
#define GENERATE_ABORTED 2

int GenerateHash(const wchar_t* filePath, rhash_ids hashAlgo, char* result, bool useUppercase, HashingProgressFunc progressFunc, HANDLE progressContext);
HashAlgoInfo* GetAlgoInfo(rhash_ids algoId);
int GetAlgoIndex(rhash_ids algoId);

// Returns list of algorithms that have matching hash pattern
std::vector<int> DetectHashAlgo(std::string &testStr);

#endif // hashing_h__
