#include <iostream>
#include <olc_net.h>


enum class CustomMsgTypes : uint32_t
{
	ServerAccept,
	ServerDeny,
	ServerPing,
	MessageAll,
	ServerMessage,
	Test,
	SendTest
};

// Example case
class CustomClient : public olc::net::client_interface<CustomMsgTypes>
{
public:
	void PingServer()
	{
		olc::net::message<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::ServerPing;

		// Caution with this. Depends on setup of system clocks.
		//std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
		
		msg << "PinG";
		Send(msg);
	}

	void MessageAll()
	{
		olc::net::message<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::MessageAll;
		Send(msg);
	}

	void SendTest()
	{
		olc::net::message<CustomMsgTypes> msg;
		msg.header.id = CustomMsgTypes::Test;
		std::cout << "Sent Test.\n";
		Send(msg);
	}




};


int main()
{
	CustomClient c;
	c.Connect("127.0.0.1", 60000);

	// Windows specific input
	bool key[4] = { false, false, false , false};
	bool old_key[4] = { false, false, false, false };


	bool bQuit = false;
	while (!bQuit)
	{
		// Windows specific input
		if (GetForegroundWindow() == GetConsoleWindow())
		{
			key[0] = GetAsyncKeyState('1') & 0x8000;
			key[1] = GetAsyncKeyState('2') & 0x8000;
			key[2] = GetAsyncKeyState('3') & 0x8000;
			key[3] = GetAsyncKeyState('4') & 0x8000;
		}

		if (key[0] && !old_key[0]) c.PingServer();
		if (key[1] && !old_key[1]) c.MessageAll();
		if (key[2] && !old_key[2]) c.SendTest();
		if (key[2] && !old_key[3]) bQuit = true;

		for (int i = 0; i < 3; i++) old_key[i] = key[i];


		if (c.IsConnected())
		{
			if (!c.Incoming().empty())
			{
				auto msg = c.Incoming().pop_front().msg;

				switch (msg.header.id)
				{
				case CustomMsgTypes::ServerAccept:
				{
					std::cout << "Server Accepted Connection\n";
				}
				break;
				case CustomMsgTypes::ServerPing:
				{
					//std::chrono::system_clock::time_point timeNow = std::chrono::system_clock::now();
					//std::chrono::system_clock::time_point timeThen;
					

					std::cout << msg; // << std::chrono::duration<double>(timeNow - timeThen).count() << "\n";
				}
				break;

				case CustomMsgTypes::ServerMessage:
				{
					uint32_t clientID;
					msg >> clientID;
					std::cout << "Hello from [" << clientID << "]\n";
				}
				break;
				
				case CustomMsgTypes::Test:
				{
					std::cout << "Test\n";
				}
				break;

				}
			}
		}
		else
		{
			std::cout << "Server Down\n";
			bQuit = true;
		}
	}

	return 0;
}
