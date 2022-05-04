#pragma once
#include "net_common.h"
//#include "net_connection.h"

namespace olc
{
	namespace net
	{
		// Header sent at start of all messages.
		// Create an "enum class" to parse at compile time.
		template <typename T> 
		struct message_header
		{
			// Enum for parsing messages.
			T id{};
			// size of message
			uint32_t size = 0;
		};

		
		template <class T>
		struct message
		{
			message_header<T> header{};
			std::vector <uint8_t> body;

			size_t size() const
			{
				return body.size();
			}

			// Overrider to allow for std::cout - overwrites <<.
			friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
			{
				os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
				return os;
			}

			// Data stream for writing
			template<typename DataType>
			friend message<T>& operator << (message<T>& msg, const DataType& data)
			{
				// Check if data type can be trivially copied
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed int vector.");
				
				// Cache current size of vector. Any extra data will be inserted after(?)
				size_t i = msg.body.size();

				// Adjust body vector by data being pushed.
				msg.body.resize(msg.body.size() + sizeof(DataType));

				// Copy new data into space created.
				std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

				// Copy new size to the header.
				msg.header.size = msg.size();

				// Returns the target so it can be chained?
				// (Written to several times)
				return msg;
			}

			// Data stream for output

			// Copy data out and reduce message by copied data.
			template<typename DataType>
			friend message<T>& operator >> (message<T>& msg, DataType& data)
			{
				// Check if data type can be trivially copied
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector.");

				// Why the minus?
				// Wait, that's so it selects the last of this datatype in the message.
				// Writes from last to first.
				size_t i = msg.body.size() - sizeof(DataType);

				// Physically copy the data from the vector into the data variable.
				std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

				// Remove read bytes
				msg.body.resize(i);

				// Update header size.
				msg.header.size = msg.size();


				// Message retunred to be chained.
				// (read several times)
				return msg;
			}
		};

		// Forward declared
		template <typename T>
		class connection;


		template <typename T>
		struct owned_message
		{
			// Where the message came from.
			std::shared_ptr<connection<T>> remote = nullptr;
			message<T> msg;


			friend std::ostream& operator<<(std::ostream& os, const owned_message<T>& msg)
			{
				os << msg.msg;
				return os;
			}
		};



	}
}
