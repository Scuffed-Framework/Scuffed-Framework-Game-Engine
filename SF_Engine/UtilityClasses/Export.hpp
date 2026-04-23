#ifdef SF_Engine_DLL
#define SF_Export __declspec(dllexport)
#else
#define SF_Export __declspec(dllimport)
#endif