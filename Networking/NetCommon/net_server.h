#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"
#include "net_connection.h"

namespace olc
{
	namespace net
	{
		template <typename T>
		class server_interface
		{
		public:
			server_interface(uint16_t port)
				: m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))

			{

			}

			virtual ~server_interface()
			{
				Stop();
			}

			bool Start()
			{
				try
				{
					WaitForClientConneciton();

					// Note that the thread creation first unlike the client
					m_threadContext = std::thread([this]() { m_asioContext.run(); });


				}
				catch (std::exception& e)
				{
					std::cerr << "[SERVER] Exception: " << e.what() << "\n";
					return false;
				}


				std::cout << "[SERVER] Started!\n";
				return true;
			}
				
			void Stop()
			{
				m_asioContext.stop();


				if (m_threadContext.joinable())
					m_threadContext.join();



				std::cout << "[SERVER] Stopped!\n";
			}

			// ASYNC - Asio will wait for connection
			void WaitForClientConneciton()
			{
				m_asioAcceptor.async_accept(
					[this](std::error_code ec, asio::ip::tcp::socket socket)
					{
					if (!ec)
					{
						std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";


						// Shares pointer across all conneciton
						std::shared_ptr<connection<T>> newconn =	// Owned by server
							std::make_shared<connection<T>>(connection<T>::owner::server,
								// Context			// Socket		// Shared the que across all conneciton
								m_asioContext, std::move(socket), m_qMessagesIn);

						//// User can deny conneciton
						if (OnClientConnect(newconn))
						{
							// Connection allowed -- Add to container of connections
							m_deqConnections.push_back(std::move(newconn));

							// Allocated Id
							m_deqConnections.back()->ConnectToClient(this, nIdCounter++);

							std::cout << "[" << m_deqConnections.back()->GetID() << "] Connection Approved\n";
						}
						else
						{
							std::cout << "[-----] Connection denied\n";
						}

					}
					else
					{
						// Error accepting
						std::cout << "[SERVER] New Conneciton Error: " << ec.message() << "\n";
					}

						// Wait for another connection.
						WaitForClientConneciton();
					}
				);
			}

			void MessageClient(std::shared_ptr<connection<T>> client, const message<T>& msg)
			{
				// Check if connected and simply send
				if (client && client.IsConnected())
				{
					client->Send(msg);
				}
				else
				{
					OnClientDisconnect(client);
					client.reset();
					m_deqConnections.erase(
						std::remove(m_deqConnections.begin(), m_deqConnections.end(), client), m_deqConnections);
				}
			}

			void MessageAllClients(const message<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr)
			{
				// Instead of removing when clients are found to be disconnected
				// removes at the end of the loop.
				bool bInvalidClientExists = false;

				// Loop through all connections in dqueue
				for (auto& client : m_deqConnections)
				{
					if (client && client->IsConnected())
					{
						if (client != pIgnoreClient)
							client->Send(msg);
					}
					else
					{
						OnClientDisconnect(client);
						client.reset();
						bInvalidClientExists = true;
					}
				}

				if (bInvalidClientExists)
					m_deqConnections.erase(
						std::remove(m_deqConnections.begin(), m_deqConnections.end(), nullptr), m_deqConnections.end());
				

			}

			// -1 for max messages, if server should wait for a received message.
			void Update(size_t nMaxMessages = -1, bool bWait = false)
			{
				if (bWait)
					m_qMessagesIn.wait();


				size_t nMessageCount = 0;
				while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty())
				{
					// Grab from message queue
					auto msg = m_qMessagesIn.pop_front();
					// Pass to handler
					OnMessage(msg.remote, msg.msg);

					nMessageCount++;
				}
			}


			



		protected:

			// Called when connection occurs. Deny connection by returning false.
			virtual bool OnClientConnect(std::shared_ptr<connection<T>> client)
			{

				return false;
			}

			virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client)
			{

			}

			virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T>& msg)
			{

			}

		public:

			virtual  void OnClientValidated(std::shared_ptr<connection<T>> client)
			{

			}


			// Incoming messages queue
			tsqueue<owned_message<T>> m_qMessagesIn;

			// Alomst got it correct.
			// Shared pointer and dqueue not vector
			std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

			// Order is important
			asio::io_context m_asioContext;
			std::thread m_threadContext;

			asio::ip::tcp::acceptor m_asioAcceptor;

			uint32_t nIdCounter = 10000;


		};
	}
}