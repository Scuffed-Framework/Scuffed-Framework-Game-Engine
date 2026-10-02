// UniformHandler.cpp
#include "UniformHandler.hpp"

namespace SF::Engine
{
	UniformHandler::UniformHandler(bool multipipeline)
		: multipipeline_(multipipeline), handlerStatus_(Buffer::Status::Normal)
	{
	}

	UniformHandler::UniformHandler(const Shader::UniformBlock &uniformBlock, bool multipipeline)
		: multipipeline_(multipipeline), uniformBlock_(uniformBlock), size_(static_cast<uint32_t>(uniformBlock_->GetSize())), uniformBuffer_(std::make_unique<UniformBuffer>(static_cast<VkDeviceSize>(size_))), handlerStatus_(Buffer::Status::Normal)
	{
	}

	bool UniformHandler::Update(const std::optional<Shader::UniformBlock> &uniformBlock)
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
			uniformBuffer_ = std::make_unique<UniformBuffer>(static_cast<VkDeviceSize>(size_));
			handlerStatus_ = Buffer::Status::Changed;
			return false;
		}

		if (handlerStatus_ != Buffer::Status::Normal)
		{
			if (bound_)
			{
				uniformBuffer_->UnmapMemory();
				bound_ = false;
			}

			handlerStatus_ = Buffer::Status::Normal;
		}

		return true;
	}
}