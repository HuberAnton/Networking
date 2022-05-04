#pragma once
#include "net_common.h"
#include "net_message.h"
#include "net_tsqueue.h"
#include "net_connection.h"


namespace olc
{
	namespace net
	{
		template <typename T>
		class client_interface
		{
		public:
			client_interface() 
			{}


			virtual ~client_interface()
			{
				// Try to disconnect on client destruciton
				Disconnect();
			}


		public:
			// Connect to hostname/ip and port.
			bool Connect(const std::string& host, const uint16_t port)
			{
				try
				{
					// Make into physical address (Resolve)
					// Works for urls and ips
					asio::ip::tcp::resolver resolver(m_context);
					asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, std::to_string(port));


					// Create connection
					m_connection = std::make_unique<connection<T>>(
						connection<T>::owner::client,
						m_context,
						asio::ip::tcp::socket(m_context), m_qMessagesIn);
					// Connection object connects to this physical address
					m_connection->ConnectToServer(endpoints);

					thrContext = std::thread([this]() {m_context.run(); });
				}
				catch (std::exception& e)
				{
					std::cerr << "Client Exception " << e.what() << "\n";
					return false;
				}

				return false;
			}

			// Disconnect from server
			// Stop asio and thread.
			void Disconnect()
			{

				if (IsConnected())
				{
					m_connection->Disconnect();
				}


				m_context.stop();

				if (thrContext.joinable())
					thrContext.join();

				m_connection.release();
			}

			// Check if currently connected to server
			bool IsConnected()
			{
				if (m_connection)
					return m_connection->IsConnected();
				else
					return false;
			}

			// Access to queue
			tsqueue<owned_message<T>>& Incoming()
			{
				return m_qMessagesIn;
			}

			// Passes the msg to the connection.
			void Send(const message<T>& msg)
			{
				if (IsConnected())
					m_connection->Send(msg);
			}


		protected:
			// Asio context handles the data transfer.
			asio::io_context m_context;
			// Thread for execution of its comments
			std::thread thrContext;


			// Handles data transfer. If a connection is established
			// it will create a connection then pass that connection to asio.
			std::unique_ptr<connection<T>> m_connection;
		private:
			// For incoming messages.
			// Note that this is the one used by connection.
			tsqueue<owned_message<T>> m_qMessagesIn;
		};

	}
}