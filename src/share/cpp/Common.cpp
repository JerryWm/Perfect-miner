#pragma once

#include <string>

#include <tchar.h>
#include <Ntsecapi.h>

#include <share/cpp/PerfomanceTime.cpp>
#include <share/cpp/WinApi.cpp>

#define MACRO_ZERO_MEMORY(m)\
	memset((void*)&(m), 0, sizeof(m));
	

	
namespace Common {

	namespace Dev {
		std::string dumpMemoryToString(uint8_t* address, uint32_t size) {
			std::string ret = "";
			static const char* hex = "0123456789ABCDEF";

			uint8_t hex_symbols[3];
			uint8_t l,h;
			
			hex_symbols[2] = 0;
			for(int i = 0; i < size; i++) {
				l = (*address >> 0) & 15;
				h = (*address >> 4) & 15;
				
				hex_symbols[0] = *( (uint8_t *)(hex + h) );
				hex_symbols[1] = *( (uint8_t *)(hex + l) );
				
				ret += (char*)&hex_symbols;
				
				if ( (i & 15) == 15 ) {
					ret += "\r\n";
				}
				
				address++;
			}
			ret += "\r\n";
			return ret;
		}
		void dumpMemory(uint8_t* address, uint32_t size) {
			std::string m = dumpMemoryToString(address, size);
			printf("%s", m.c_str());
		}
		
	}


	bool getCacheL2L3Size(uint32_t *l2, uint32_t *l3) {
		bool ret = false;
		
		typedef WINBOOL (WINAPI *pf_GetLogicalProcessorInformationEx) (LOGICAL_PROCESSOR_RELATIONSHIP, PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX, PDWORD);
		pf_GetLogicalProcessorInformationEx WinApi_GetLogicalProcessorInformationEx = (pf_GetLogicalProcessorInformationEx)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "GetLogicalProcessorInformationEx");
		if ( !WinApi_GetLogicalProcessorInformationEx ) {
			return ret;
		}

		*l2 = 0;
		*l3 = 0;
		
		char *buffer = NULL;
		DWORD len = 0;
		if ( !WinApi_GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len) && ( GetLastError() == ERROR_INSUFFICIENT_BUFFER ) ) {
			char *ptr = buffer = (char*)malloc(len);

			if ( WinApi_GetLogicalProcessorInformationEx(RelationAll, (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)buffer, &len) ) {
				
				while(ptr < buffer + len) {
					PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pi = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)ptr;
					
					if ( pi->Relationship == RelationCache ) {
						if ( pi->Cache.Level == 2 ) { *l2 += pi->Cache.CacheSize; }
						if ( pi->Cache.Level == 3 ) { *l3 += pi->Cache.CacheSize; }
					}
					
					ptr += pi->Size;
				}
			
				ret = true;
			}
			
			free(buffer);
		}
		
		return ret;
	}


	class Random {
		private:
			uint64_t initSRand = 0;
		public:
			void init() {
				initSRand = PerfomanceTime::i().getCounter();
				std::srand(initSRand);
			}

			uint8_t uint8() {
				return std::rand() & 0xFF;
			}
			uint16_t uint16() {
				return uint8() | (uint8() << 8);
			}
			uint32_t uint32() {
				return uint16() | (uint16() << 16);
			}
			
		public:
			static Random& i() {
				static Random *_this = NULL;
				
				if ( !_this ) {
					_this = (Random*)malloc(sizeof(Random));
					_this->init();
				}
				
				return *_this;
			}
		
	};
		




	class TimeInterval {
		private:
			uint64_t interval;
			uint64_t time;
			
		private:
			uint64_t getMsecTime() {
				return PerfomanceTime::i().getMiliSec();
			}
		public:
			TimeInterval(uint64_t interval) {
				this->interval = interval;
				reset();
			}
			bool frame() {
				if ( getMsecTime() > this->time ) {
					reset();
					return true;
				}
				
				return false;
			}
			void reset() {
				this->time = getMsecTime() + this->interval;
			}
	};

	bool fileExists(const std::string &path) {
		auto file = fopen(path.c_str(), "r");
		if ( !file ) {
			return false;
		}
		fclose(file);
		return true;
	}

	bool filePutContents(const std::string &path, const char *buf, size_t size) {
		auto file = fopen(path.c_str(), "w");
		if ( !file ) {
			return false;
		}
		fwrite(buf, size, 1, file);
		fclose(file);
		return true;
	}
	std::string fileGetContents(const std::string &path) {
		auto file = fopen(path.c_str(), "r");
		if ( !file ) {
			return "";
		}
		
		std::string s = "";
		
		char buf[1024];
		while(int rSize = fread(&buf, 1, sizeof(buf)-1, file)) {
			buf[rSize] = 0;
			s += (char*)&buf;
		}
		
		fclose(file);
		return s;
	}
	
	bool textFileAppend(const std::string &path, const std::string s) {
		auto f = fopen(path.c_str(), "a+");
		if ( f ) {
			fwrite(s.c_str(), s.length(), 1, f);
			fclose(f);
		}
	}
	

	
	
	bool hexToBin(const std::string &hex, uint8_t *bin, int size) {
			char b;
			uint8_t lo, hi;
			
			int i = 0;
			
			while( i + 1 < hex.length() ) {
				b = hex[i];
				if (b >= '0' && b <= '9') { hi = (b - '0'     ); } else
				if (b >= 'a' && b <= 'f') { hi = (b - 'a' + 10); } else
				if (b >= 'A' && b <= 'F') { hi = (b - 'A' + 10); } else 
				{ return false; }
				i++;
				
				b = hex[i];
				if (b >= '0' && b <= '9') { lo = (b - '0'     ); } else
				if (b >= 'a' && b <= 'f') { lo = (b - 'a' + 10); } else
				if (b >= 'A' && b <= 'F') { lo = (b - 'A' + 10); } else 
				{ return false; }
				i++;
				
				if ( !size ) {
					return true;
				}
				
				*bin = lo | (hi << 4);
				bin++;
				size--;
			}
			return true;
	}
	std::string binToHex(uint8_t *bin, int size) {
		static const char *tb_to_hex = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f202122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f404142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f606162636465666768696a6b6c6d6e6f707172737475767778797a7b7c7d7e7f808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9fa0a1a2a3a4a5a6a7a8a9aaabacadaeafb0b1b2b3b4b5b6b7b8b9babbbcbdbebfc0c1c2c3c4c5c6c7c8c9cacbcccdcecfd0d1d2d3d4d5d6d7d8d9dadbdcdddedfe0e1e2e3e4e5e6e7e8e9eaebecedeeeff0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";
			
		std::string s = "";
		while(size--) {
			s += *((char*)(tb_to_hex + (*bin*2) + 0));
			s += *((char*)(tb_to_hex + (*bin*2) + 1));
			bin++;
		}
			
		return s;
	}

	
	void terminateCurrentProcess() {
		TerminateProcess(GetCurrentProcess(), NO_ERROR);
	}
	bool isProcessAlive(HANDLE hProcess) {
		DWORD status;
		return ( GetExitCodeProcess(hProcess, &status) && ( status == STILL_ACTIVE ) );
	}

	
	bool addPrivilege(HANDLE hProcess, const char *pszPrivilege) {
		HANDLE           hToken;
		TOKEN_PRIVILEGES tp;
		BOOL             status;

		if ( !OpenProcessToken(hProcess, TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken) ) {
			return false;
		}

		if ( !LookupPrivilegeValue(NULL, pszPrivilege, &tp.Privileges[0].Luid) ) {
			CloseHandle(hToken);
			return false;
		}

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		status = AdjustTokenPrivileges(hToken, false, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);

		if ( !status || (GetLastError() != ERROR_SUCCESS) ) {
			CloseHandle(hToken);
			return false;
		}

		CloseHandle(hToken);
		return true;
	}
	static LSA_UNICODE_STRING StringToLsaUnicodeString(LPCTSTR string) {
		LSA_UNICODE_STRING lsaString;

		DWORD dwLen = (DWORD) wcslen((wchar_t*)string);
		lsaString.Buffer = (LPWSTR) string;
		lsaString.Length = (USHORT)((dwLen) * sizeof(WCHAR));
		lsaString.MaximumLength = (USHORT)((dwLen + 1) * sizeof(WCHAR));
		return lsaString;
	}
	static bool obtainLockPagesPrivilege() {
		HANDLE token;
		PTOKEN_USER user = NULL;

		if ( OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) ) {
			DWORD size = 0;

			GetTokenInformation(token, TokenUser, NULL, 0, &size);
			if (size) {
				user = (PTOKEN_USER) LocalAlloc(LPTR, size);
			}

			GetTokenInformation(token, TokenUser, user, size, &size);
			CloseHandle(token);
		}

		if (!user) {
			return false;
		}

		LSA_HANDLE handle;
		LSA_OBJECT_ATTRIBUTES attributes;
		ZeroMemory(&attributes, sizeof(attributes));

		BOOL result = false;
		if (LsaOpenPolicy(NULL, &attributes, POLICY_ALL_ACCESS, &handle) == 0) {
			LSA_UNICODE_STRING str = StringToLsaUnicodeString(_T(SE_LOCK_MEMORY_NAME));

			if (LsaAddAccountRights(handle, user->User.Sid, &str, 1) == 0) {
				result = true;
			}

			LsaClose(handle);
		}

		LocalFree(user);
		return result;
	}




	void *createSharedMemory(std::string &name, size_t size, bool isLargePage = false, bool isNoCache = false) {
		uint64_t largePage = GetLargePageMinimum();
		if ( isLargePage ) {
			while(largePage < size) {
				largePage <<= 1;
			}
			size = largePage;
		}
		
		name = "Memory_id_" + std::to_string(std::rand()) + std::to_string(std::rand()) + std::to_string(std::rand()) + std::to_string(std::rand()) +  std::to_string(PerfomanceTime::i().getCounter());

		HANDLE hFileMap = CreateFileMapping(
			INVALID_HANDLE_VALUE, 
			NULL, 
			PAGE_READWRITE | SEC_COMMIT | (isLargePage ? SEC_LARGE_PAGES : 0) | (isNoCache ? SEC_NOCACHE : 0), 
			0, 
			size, 
			name.c_str()
		);

		if ( (hFileMap == INVALID_HANDLE_VALUE) || (hFileMap == NULL) || (GetLastError() == ERROR_ALREADY_EXISTS) ) {
			return NULL;
		}

		void *ptr = (void*)MapViewOfFile(hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
		if ( !ptr ) {
			CloseHandle(hFileMap);
		}

		return ptr;
	}

	uint32_t numaNodeIndexNormalize(uint32_t numaNodeIndex) {
		ULONG numaHighestNodeNumber;
		if ( WinApi::GetNumaHighestNodeNumber(&numaHighestNodeNumber) && numaHighestNodeNumber ) {
			return numaNodeIndex;
		}
		//printf(" => NUMA_NO_PREFERRED_NODE \n");

		return NUMA_NO_PREFERRED_NODE;
	}
	void *allocMemoryNuma(size_t size, uint32_t numaIndex, bool isLargePage = false, bool isNoCache = false) {
		uint64_t largePage = GetLargePageMinimum();
		if ( isLargePage ) {
			if ( !largePage ) {
				return NULL;
			}
			
			while(largePage < size) {
				largePage <<= 1;
			}
			size = largePage;
		}
		
		return (void*)WinApi::VirtualAllocExNuma(
			GetCurrentProcess(), 
			NULL, 
			size, 
			MEM_COMMIT | MEM_RESERVE | (isLargePage ? MEM_LARGE_PAGES : 0), 
			PAGE_READWRITE | (isNoCache ? PAGE_NOCACHE : 0),
			numaNodeIndexNormalize(numaIndex)
		);
	}
	void *createSharedMemoryNuma(std::string &name, size_t size, uint32_t numaIndex, bool isLargePage = false, bool isNoCache = false) {
		uint64_t largePage = GetLargePageMinimum();
		if ( isLargePage ) {
			if ( !largePage ) {
				return NULL;
			}
			
			while(largePage < size) {
				largePage <<= 1;
			}
			
			size = largePage;
		}
		
		name = "Memory_id_" + 
			std::to_string(Random::i().uint32()) + 
			std::to_string(Random::i().uint32()) + 
			std::to_string(Random::i().uint32()) + 
			std::to_string(Random::i().uint32()) + 
			std::to_string(PerfomanceTime::i().getCounter());
		
		HANDLE hFileMap = WinApi::CreateFileMappingNumaA(
			INVALID_HANDLE_VALUE, 
			NULL, 
			PAGE_READWRITE | SEC_COMMIT | (isLargePage ? SEC_LARGE_PAGES : 0) | (isNoCache ? SEC_NOCACHE : 0), 
			0, 
			size, 
			name.c_str(),
			numaNodeIndexNormalize(numaIndex)
		);

		if ( (hFileMap == INVALID_HANDLE_VALUE) || (hFileMap == NULL) || (GetLastError() == ERROR_ALREADY_EXISTS) ) {
			return NULL;
		}

		void *ptr = (void*)MapViewOfFile(hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
		if ( !ptr ) {
			CloseHandle(hFileMap);
		}

		return ptr;
	}
	
	void *openSharedMemory(const std::string &name) {
		HANDLE hFileMap = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());

		if ( hFileMap == INVALID_HANDLE_VALUE ) {
			return NULL;
		}		

		void *ptr = MapViewOfFile(hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);

		if ( !ptr ) {
			CloseHandle(hFileMap);
		}
		
		return ptr;
	}


	std::string dumpMemory(uint8_t* address, uint32_t size) {
		std::string s;
		
		static const char* hex = "0123456789ABCDEF";

		uint8_t hex_symbols[4];
		uint8_t l,h;
		
		hex_symbols[2] = 32;
		hex_symbols[3] = 0;
		for(int i = 0; i < size; i++) {
			l = (*address >> 0) & 15;
			h = (*address >> 4) & 15;
			
			hex_symbols[0] = *( (uint8_t *)(hex + h) );
			hex_symbols[1] = *( (uint8_t *)(hex + l) );
			
			s += (char*)&hex_symbols[0];
			
			if ( (i & 15) == 15 ) {
				s += "\r\n";
			}
			
			address++;
		}
		s += "\r\n";
		
		return s;
	}

	
	bool createNewProcess(const std::string &path) {
		STARTUPINFO si;
		PROCESS_INFORMATION pi;
				
		MACRO_ZERO_MEMORY(si)
		si.cb = sizeof(STARTUPINFO);
		si.dwFlags = 0;

		MACRO_ZERO_MEMORY(pi)
		
		char *pworker_path = (char*)malloc(path.length() + 1);
		memcpy(pworker_path, path.c_str(), path.length() + 1);

		char *pworker_cmd = (char*)malloc(path.length() + 1);
		memcpy(pworker_cmd, path.c_str(), path.length() + 1);
		//	CREATE_NEW_CONSOLE	|	CREATE_NO_WINDOW
		if ( !CreateProcess(pworker_path, pworker_cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi) ) {
			free(pworker_path);
			free(pworker_cmd);
			return false;
		}
		
		free(pworker_path);
		free(pworker_cmd);
		return true;
	}

	std::string errorToString(DWORD errorCode) {
		char msg[1024];
		wchar_t msgw[1024];
		
		memset((void*)&msg, 0, sizeof(msg));
		memset((void*)&msgw, 0, sizeof(msgw));
		
		if ( FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&msg[0], sizeof(msg)-1, NULL) ) {
			if ( MultiByteToWideChar(CP_ACP, 0, &msg[0], strlen(&msg[0])+1, &msgw[0], (sizeof(msgw) / sizeof(wchar_t))-1) ) {
				if ( WideCharToMultiByte(CP_UTF8, 0, &msgw[0], lstrlenW(&msgw[0]) + 1, &msg[0], sizeof(msg)-1, NULL, NULL) ) {
					return "Error code: "+std::to_string(errorCode) + "; Error text: " + (char*)&msg;
				}
			}
		}
		
		return "Error code: "+std::to_string(errorCode) + "; Error text: " "Error unk";
	}	
	
	namespace File {
		class FileData {
			public:
				void *data = NULL;
				uint64_t size = 0;
		};
		
		bool getContentsA(const char *path, void **data, uint64_t *size, std::string *errorText = NULL) {
			if ( errorText ) {
				*errorText = "";
			}
			
			FileData ret;
			
			HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			
			if ( hFile == INVALID_HANDLE_VALUE ) {
				if ( errorText ) { *errorText = errorToString(GetLastError()); }
				return false;
			}
			
			DWORD dwSizeHigh = 0, dwSizeLow = 0;
			dwSizeLow = GetFileSize(hFile, &dwSizeHigh);
			if ( dwSizeHigh ) {
				if ( errorText ) { *errorText = "File size >= 4gb"; }
				CloseHandle(hFile);
				return false;
			}
			*size = (((uint64_t)dwSizeHigh)<<32) | dwSizeLow;
			
			*data = malloc(*size);
			if ( !*data ) {
				if ( errorText ) { *errorText = "Allocate memory filed"; }
				CloseHandle(hFile);
				return false;
			}
			
			uint64_t rSizeFull = 0;
			char *ptr = (char*)(*data);
			DWORD rSize = 0;
			
			while(rSizeFull < *size) {
				if ( !ReadFile(hFile, (void*)ptr, *size, &rSize, NULL) ) {
					if ( errorText ) { *errorText = errorToString(GetLastError()); }
					return false;
				}
				
				rSizeFull += rSize;
				ptr += rSize;
			}

			CloseHandle(hFile);
			
			return true;
		}
		bool putContentsA(const char *path, void *data, uint64_t size, std::string *errorText = NULL) {
			 HANDLE hFile = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			 
			if ( hFile == INVALID_HANDLE_VALUE ) {
				if ( errorText ) { *errorText = errorToString(GetLastError()); }
				return false;
			}
			
			DWORD wSize = 0;
			if ( !WriteFile(hFile, data, size, &wSize, NULL) ) {
				if ( errorText ) { *errorText = errorToString(GetLastError()); }
				CloseHandle(hFile);
				return false;
			}
			// TODO
			CloseHandle(hFile);
			return true;
		}
	
		bool getTmpPath(std::string &path, std::string *errorText = NULL) {
			path = "";
			
			int maxPath = 4*1024;

			char *lpTempPathBuffer = (char*)malloc(maxPath);
			char *szTempFileName = (char*)malloc(1024*1024);
			*lpTempPathBuffer = 0;
			*szTempFileName = 0;
			
			int len = GetTempPath(maxPath, lpTempPathBuffer);
			if ( !len || len > maxPath ) {
				memcpy(lpTempPathBuffer, "./", 3);
			}

			int uRetVal = GetTempFileName(lpTempPathBuffer, TEXT("tmp"), 0, szTempFileName);
			if ( !uRetVal ) {
				if ( errorText ) { *errorText = errorToString(GetLastError()); }
				free(lpTempPathBuffer);
				free(szTempFileName);
				return false;
			}
			
			path = szTempFileName + std::to_string(PerfomanceTime::i().getCounter()) + std::to_string(Random::i().uint32()) + ".tmp";

			free(lpTempPathBuffer);
			free(szTempFileName);
			return true;
		}
	
	
	};
	
	namespace String {
		
		std::string rtrim(const std::string &s) {
			return rtrim(s.c_str());
		}
		std::string rtrim(const char *s) {
			std::string r = "";
			
			for(; *s; s++) {
				switch(*s) {
					case '\r':
					case '\n':
					case '\x20':
					case '\x09':
						break;
					
					default:
						r = s;
						return r;
				}
			}
			
			return r;
		}
		
		std::string random(uint32_t len = 16) {
			const char sFirst[] = "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
			const char sMore[] = "abcdefghijklmnopqrstuvwxyz" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "0123456789";
			
			std::string ret = "";
			ret.reserve(len);
			
			for(int i = 0; i < len; i++) {
				ret += !i ? 
					sFirst[ Random::i().uint16() % strlen(sFirst) ] :
					sMore [ Random::i().uint16() % strlen(sMore)  ] ;
			}
			
			return ret;
		}
		
		bool replace(std::string& str, const std::string from, const std::string to) {
			size_t start_pos = str.find(from);
			
			if ( start_pos == std::string::npos ) {
				return false;
			}
			
			str.replace(start_pos, from.length(), to);
			
			return true;
		}
		
	}
}
