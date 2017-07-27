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

#define NUMBER_OF_SUPPORTED_HASHES 7
extern HashAlgoInfo SupportedHashes[NUMBER_OF_SUPPORTED_HASHES];

struct FileHashInfo
{
	std::wstring Filename;
	std::string HashStr;
	int HashAlgoIndex;

	rhash_ids GetAlgo() const { return (HashAlgoIndex >= 0) ? SupportedHashes[HashAlgoIndex].AlgoId : RHASH_HASH_COUNT; }
	std::wstring ToString() const;
	void Serialize(std::stringstream& dest, UINT codepage) const;
};

class HashList
{
private:
	std::vector<FileHashInfo> m_HashList;

	int GetFileRecordIndex(const wchar_t* fileName) const;
	bool DumpStringToFile(const std::string& data, const wchar_t* filePath);
	bool DetectHashAlgo(const char* testStr, UINT codepage, const wchar_t* filePath, int &foundAlgoIndex, HashListFormat &listFormat);
	bool TryParseBSD(const char* inputStr, UINT codepage, FileHashInfo &fileInfo);
	bool TryParseSimple(const char* inputStr, UINT codepage, int hashAlgoIndex, FileHashInfo &fileInfo);

public:
	HashList() {}

	bool SaveList(const wchar_t* filepath, UINT codepage);
	bool SaveListSeparate(const wchar_t* baseDir, UINT codepage, int &successCount, int &failCount);
	bool LoadList(const wchar_t* filepath, UINT codepage, bool merge);

	void SetFileHash(const std::wstring &fileName, std::string hashVal, rhash_ids hashAlgo);
	
	size_t GetCount() const { return m_HashList.size(); }
	const FileHashInfo& GetFileInfo(size_t index) const { return m_HashList.at(index); }
};

// Params: context, processed bytes
typedef bool (CALLBACK *HashingProgressFunc)(HANDLE context, int64_t bytesProcessed);

enum class GenResult
{
	Success = 0,
	Error = 1,
	Aborted = 2
};

extern size_t FileReadBufferSize;

GenResult GenerateHash(const std::wstring& filePath, std::vector<rhash_ids> hashAlgos, std::vector<std::string> &results, bool useUppercase, HashingProgressFunc progressFunc, HANDLE progressContext);
GenResult GenerateHash(const std::wstring& filePath, rhash_ids hashAlgo, std::string& result, bool useUppercase, HashingProgressFunc progressFunc, HANDLE progressContext);

HashAlgoInfo* GetAlgoInfo(rhash_ids algoId);
int GetAlgoIndex(rhash_ids algoId);
int GetAlgoIndexByName(const wchar_t* name);

// Returns list of algorithms that have matching hash pattern
std::vector<int> DetectHashAlgo(std::string &testStr);

bool SameHash(const std::string& hash1, const std::string& hash2);

#endif // hashing_h__
