#include "StorageHandler.hpp"

namespace SF::Engine
{
	StorageHandler::StorageHandler(bool multipipeline)
		: multipipeline_(multipipeline), handlerStatus_(Buffer::Status::Reset)
	{
	}

	StorageHandler::StorageHandler(const Shader::UniformBlock &uniformBlock, bool multipipeline)
		: multipipeline_(multipipeline), uniformBlock_(uniformBlock), size_(static_cast<uint32_t>(uniformBlock_->GetSize())), storageBuffer_(std::make_unique<StorageBuffer>(static_cast<VkDeviceSize>(size_))), handlerStatus_(Buffer::Status::Changed)
	{
	}

	bool StorageHandler::Update(const std::optional<Shader::UniformBlock> &uniformBlock)
	{
		if (handlerStatus_ == Buffer::Status::Reset ||
			(multipipeline_ && !uniformBlock_) ||
			(!multipipeline_ && uniformBlock_ != uniformBlock))
		{
			if ((size_ == 0 && !uniformBlock_) ||
				(uniformBlock_ && uniformBlock_ != uniformBlock &&
				 static_cast<uint32_t>(uniformBlock_->GetSize()) == size_))
			{
				size_ = static_cast<uint32_t>(uniformBlock->GetSize());
			}

			uniformBlock_ = uniformBlock;
			bound_ = false;
			storageBuffer_ = std::make_unique<StorageBuffer>(static_cast<VkDeviceSize>(size_));
			handlerStatus_ = Buffer::Status::Changed;
			return false;
		}

		if (handlerStatus_ != Buffer::Status::Normal)
		{
			if (bound_)
			{
				storageBuffer_->UnmapMemory();
				bound_ = false;
			}

			handlerStatus_ = Buffer::Status::Normal;
		}

		return true;
	}
}