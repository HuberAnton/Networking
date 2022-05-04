#pragma once

#include "net_common.h"
#include "net_tsqueue.h"
#include "net_message.h"

namespace olc
{
	namespace net
	{

		template<typename T>
		class server_interface;



		template<typename T>				// Shared pointer
		class connection : public std::enable_shared_from_this<connection<T>>
		{
		public:

			enum class owner
			{
				server,
				client
			};


			connection(owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn)
				: m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn)
			{
				m_nOwnerType = parent;


				// Validation
				if (m_nOwnerType == owner::server)
				{
					// Server generates a 
					m_nHandshakeOut = uint64_t(std::chrono::system_clock::now().time_since_epoch().count());

					m_nHandshakeCheck = scramble(m_nHandshakeOut);
				}
				else
				{
					m_nHandshakeIn = 0;
					m_nHandshakeOut = 0;
				}

			}

			virtual ~connection()
			{}

			void ConnectToClient(olc::net::server_interface<T>* server, uint32_t uid = 0)
			{
				if (m_nOwnerType == owner::server)
				{
					if (m_socket.is_open())
					{
						id = uid;
						//ReadHeader();

						WriteValidation();

						ReadValidation(server);

					}
				}
			}
			uint32_t GetID() const
			{
				return id;
			}

			void ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints)
			{
				if (m_nOwnerType == owner::client)
				{
					asio::async_connect(m_socket, endpoints,
						[this](std::error_code ec, asio::ip::tcp::endpoint endpoint)
					{
						if (!ec)
						{
							//ReadHeader();
							ReadValidation();
						}
					});
				}
			}

			void Disconnect()
			{
				if (IsConnected())
					asio::post(m_asioContext, [this]() { m_socket.close(); });
			}

			bool IsConnected() const
			{
				return m_socket.is_open();
			}

		
			void  Send(const message<T>& msg)
			{
				asio::post(m_asioContext,
					[this, msg]()
				{
					// If already writing a message will cause failure.
					bool bWritingMessage = !m_qMessagesOut.empty();
					// Push message to out queue
					m_qMessagesOut.push_back(msg);
					// If not writing message will
					// start writing message
					if (!bWritingMessage)
					{
						WriteHeader();
					}
				});
			}

		private:

			// Client validation
			
			void WriteValidation()
			{
				asio::async_write(m_socket, asio::buffer(&m_nHandshakeOut, sizeof(uint64_t)),
					[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (m_nOwnerType == owner::client)
							ReadHeader();
					}
					else
					{
						// Could add to ban list.
						m_socket.close();
					}


				});
				
			}

			void ReadValidation(olc::net::server_interface<T>* server = nullptr)
			{
				asio::async_read(m_socket, asio::buffer(&m_nHandshakeIn, sizeof(uint64_t)),
					[this, server](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						// Different depending on server or client.
						if (m_nOwnerType == owner::server)
						{
							// Valid client 
							if (m_nHandshakeIn == m_nHandshakeCheck)
							{
								std::cout << "Client Validated" << std::endl;
								server->OnClientValidated(this->shared_from_this());

								// Wait for message.
								ReadHeader();
							}
							else
							{
								std::cout << "Client Disconnected (Incorrect Validation)" << std::endl;
								m_socket.close();
							}
						}
						else
						{
							m_nHandshakeOut = scramble(m_nHandshakeIn);

							WriteValidation();
						}
					}
					else
					{
						std::cout << "Client Disconnected (Read Validation Failure)" << std::endl;
						m_socket.close();
					}

				});
			}

			// Is this encryption?
			// Honestly not that crash hot
			uint64_t scramble(uint64_t nInput)
			{
				uint64_t out = nInput ^ 0xDEADBEEFC0DECAFE;
				out = (out & 0xF0F0F0F0F0F0F0F0) >> 4 | (out & 0xF0F0F0F0F0F0F0F0) << 4;
				return out ^ 0xC0DEFACE12345678;
			}


			// ASYNC - Prime context for writing header
			void WriteHeader()
			{
				// Check if header has size?
				asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						if (m_qMessagesOut.front().body.size() > 0)
						{
							WriteBody();
						}
						else
						{
							m_qMessagesOut.pop_front();

							if (!m_qMessagesOut.empty())
							{
								WriteHeader();
							}
						}
					}
					else
					{
						std::cout << "[" << id << "] Write Header Fail.\n";
						m_socket.close();
					}
				});
			}

			// ASYNC - Prime context for writing body
			void WriteBody()
			{
				asio::async_write(m_socket, asio::buffer(m_qMessagesOut.front().body.data(), m_qMessagesOut.front().body.size()),
					[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						m_qMessagesOut.pop_front();

						if (!m_qMessagesOut.empty())
						{
							WriteHeader();
						}
					}
					else
					{
						std::cout << "[" << id << "] Write Body Fail.\n";
						m_socket.close();
					}

				});
			}


			// ASYNC - Prime context ready to read message header
			void ReadHeader()
			{						// From					// To							// Size
				asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						// If body size greater than 0.
						if (m_msgTemporaryIn.header.size > 0)
						{
							m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
							ReadBody();
						}
						else
						{
							AddToIncomingMessageQueue();
						}
					}
					else
					{
						std::cout << "[" << id << "] Read Header Failed.\n";
						m_socket.close();
					}
				});
			}

			// ASYNC - Prime context for reading body
			void ReadBody()
			{
				asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(),m_msgTemporaryIn.body.size()),
					[this](std::error_code ec, std::size_t length)
				{
					if (!ec)
					{
						AddToIncomingMessageQueue();
					}
					else
					{
						std::cout << "[" << id << "] Read Body Failed.\n";
						m_socket.close();
					}

				});

			}
		

			void AddToIncomingMessageQueue()
			{

				if (m_nOwnerType == owner::server)
					// Enables by the inherited class		// Note that this is an init list for an owned message.
					m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn });
				else
					// Null if owner is client as it only ever has 1 connection.
					m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });


				ReadHeader();
			}



		protected:
			// Each connection has a unique socket.
			// Note that this is a tcp connection
			asio::ip::tcp::socket m_socket;

			// Context is sytem based and should be the same
			// for all server and clients. Provided by client or server
			asio::io_context& m_asioContext;

			// Holds outgoing messages to connection
			tsqueue<message<T>> m_qMessagesOut;

			// Holds received messages from connection.
			// Owner is expected to provide the que.
			tsqueue<owned_message<T>>& m_qMessagesIn;
			message<T> m_msgTemporaryIn;

			owner m_nOwnerType = owner::server;
			
			// Unique Id of the connection.
			// Provided by server
			uint32_t id;


			// For validation.
			uint64_t m_nHandshakeOut = 0;
			uint64_t m_nHandshakeIn = 0;
			uint64_t m_nHandshakeCheck = 0;

		};

	}
}