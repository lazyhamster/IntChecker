#ifndef Utils_h__
#define Utils_h__

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

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

#endif // Utils_h__