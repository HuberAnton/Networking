#include <iostream>
#include <chrono>


#if _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

#define EXAMPLE "93.184.216.34"
#define GOOGLE "142.250.70.142"
#define YOUTUBE "142.250.70.206"
#define WORDPRESS "51.38.81.49"

							// Bytes
std::vector<char> vBuffer(1 * 1024);

void GrabSomeData(asio::ip::tcp::socket& socket)
{
	socket.async_read_some(asio::buffer(vBuffer.data(), vBuffer.size()),
		[&](std::error_code ec, std::size_t length)
	{
		if (!ec)
		{
			std::cout << "\n\nRead " << length << " bytes\n\n";

			for (int i = 0; i < length; i++)
				std::cout << vBuffer[i];

			GrabSomeData(socket);
		}
	}
	);
}

// Demo - expect this to change
int main()
{
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

	// Error handling
	asio::error_code ec;

	// Platform specific interface
	asio::io_context context;




	// Gives a fake task to the context so that it doesn't instantly 
	// stop before we can grab some data.
	// Asio will terminate without something to do.
	asio::io_context::work idleWork(context);

	// Start the context on a seperate thread
	// Note that it returns when context is no longer running.
	std::thread thrContext = std::thread([&]() {context.run();});

	// Address of where we are connecting to. Only works with http.
	// Https needs ssl context. https://stackoverflow.com/questions/69713739/how-to-connect-with-boostasio-to-a-https-server-using-a-proxy
	asio::ip::tcp::endpoint endpoint(asio::ip::make_address(WORDPRESS, ec), 80);

	// Tell socket what context it is working in.
	asio::ip::tcp::socket socket(context);

	// Tries to connect to endpoint(website).
	// Note that this will pause for a moment as it waits for a response.
	socket.connect(endpoint, ec);

	if (!ec)
	{
		std::cout << "Connected!" << std::endl;
	}
	else
	{ 
		std::cout << "Failed to connect to adress:\n" << ec.message() << std::endl;
	}

	// Checks if socket has connected to the
	//endpoint and can send and receive data.
	if (socket.is_open())
	{
		// Grab data
		GrabSomeData(socket);

		std::string sReqest =
			"GET /index.html HTTP/1.1\r\n"
			"Host: example.com\r\n"
			"Connection:close\r\n\r\n";

		socket.write_some(asio::buffer(sReqest.data(), sReqest.size()), ec);

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(20000ms);

		context.stop();

		if (thrContext.joinable())
			thrContext.join();
	}



	return 0;
}