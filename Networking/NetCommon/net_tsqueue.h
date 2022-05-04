#pragma once
#include "net_common.h"

#define _HAS_CXX17
// Thread safe queue.
namespace olc
{
	namespace net
	{
		template<typename T>
		class tsqueue
		{
		public:

			tsqueue() = default;
			tsqueue(const tsqueue<T>&) = delete;
			virtual ~tsqueue() { clear(); }

			// returns a reference from the que that cant be changed
			const T& front()
			{
				// Locks the queue
				asio::detail::mutex::scoped_lock lock(muxQueue);
				// Returns the information from the queue
				return deqQueue.front();
			}

			// Same as above but back
			const T& back()
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				return deqQueue.back();
			}

			// Add item to back
			void push_back(const T& item)
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				deqQueue.emplace_back(std::move(item));

				// Creates a unique lock.
				// Why a unique lock?
				std::unique_lock<std::mutex> ul(muxBlocking);
				// Wakes 1 thread waiting for this 
				// conditional variable. If none does nothing.
				cvBlocking.notify_one();
			}

			// Add item to front
			void push_front(const T& item)
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				deqQueue.emplace_front(std::move(item));

				std::unique_lock<std::mutex> ul(muxBlocking);
				cvBlocking.notify_one();
			}

			bool empty()
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				return deqQueue.empty();
			}

			size_t count()
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				return deqQueue.size();
			}

			void clear()
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				deqQueue.clear();
			}

			// Remove and return the item removed

			T pop_front()
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				auto t = std::move(deqQueue.front());
				deqQueue.pop_front();
				return t;
			}

			T pop_back()
			{
				asio::detail::mutex::scoped_lock lock(muxQueue);
				auto t = std::move(deqQueue.back());
				deqQueue.pop_back();
				return t;
			}

			void wait()
			{
				// Loop needs to exist as 'spurious wakeup' is a thing.
				// Will signal the conditional variable to wake.
				while (empty())
				{
					std::unique_lock<std::mutex> ul(muxBlocking);
					// Makes the thread running this wait for 
					// the conditional variable to change
					cvBlocking.wait(ul);
				}
			}



		protected:
			asio::detail::mutex muxQueue;
			std::deque<T> deqQueue; // double ended que

			// Used with wait
			// On the condition changed will continue wait.
			std::condition_variable cvBlocking;
			std::mutex muxBlocking;
		};

	}
}