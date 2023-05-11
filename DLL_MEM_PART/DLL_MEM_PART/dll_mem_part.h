#pragma once

#ifdef DLL_MEM_PART_EXPORTS
# define DLL_MEM_PART_API __declspec(dllexport)
#else
#define DLL_MEM_PART_API __declspec(dllimport)
#endif // DLL_MEM_PART_EXPORTS
