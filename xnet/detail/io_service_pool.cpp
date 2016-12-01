#include "io_service_pool.h"
#include "io_worker.h"

namespace moon
{

	io_service_pool::io_service_pool(uint8_t pool_size)
	{
		pool_size = pool_size > 0 ? pool_size : 1;

		for (uint8_t i = 1; i <= pool_size; i++)
		{
			auto worker = std::make_shared<io_worker>();
			worker->workerid(i);
			workers_.emplace_back(std::move(worker));
		}
		next_worker_ = workers_.begin();
	}

	void io_service_pool::run()
	{
		assert(thread_pool_.size() == 0);

		for (auto& it : workers_)
		{
			thread_pool_.emplace_back(std::thread(std::bind(&io_worker::run, it)));
		}
	}

	void io_service_pool::stop()
	{
		for (auto& iter : workers_)
		{
			iter->stop();
		}

		for (auto& iter : thread_pool_)
		{
			if (iter.joinable())
			{
				iter.join();
			}
		}
	}

	io_worker_ptr_t io_service_pool::find(uint32_t sessionid)
	{
		uint8_t worker_id = (sessionid >> IO_WORKER_ID_SHIFT) & 0xFF;
		if (worker_id <= workers_.size() && worker_id > 0)
		{
			return workers_[worker_id-1];
		}
		return io_worker_ptr_t();
	}

	io_worker_ptr_t& io_service_pool::get_io_worker()
	{
		if (next_worker_ == workers_.end())
		{
			next_worker_ = workers_.begin();
		}
		auto& ret = *next_worker_;
		next_worker_++;
		return ret;
	}

	asio::io_service& io_service_pool::io_service()
	{
		if (next_worker_ == workers_.end())
		{
			next_worker_ = workers_.begin();
		}
		return (*(next_worker_++))->io_service();
	}

	void io_service_pool::set_network_handle(const network_handle_t & handle)
	{
		for (auto& it : workers_)
		{
			it->set_network_handle(handle);
		}
	}
}