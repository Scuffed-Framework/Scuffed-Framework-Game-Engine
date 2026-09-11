#include "PushHandler.hpp"

namespace SF::Engine
{
	PushHandler::PushHandler(bool multipipeline)
		: multipipeline_(multipipeline)
	{
	}

	PushHandler::PushHandler(const Shader::UniformBlock &uniformBlock, bool multipipeline)
		: multipipeline_(multipipeline), uniformBlock_(uniformBlock), data_(std::make_unique<std::byte[]>(uniformBlock_->GetSize()))
	{
	}

	bool PushHandler::Update(const std::optional<Shader::UniformBlock> &uniformBlock)
	{
		if ((multipipeline_ && !uniformBlock_) ||
			(!multipipeline_ && uniformBlock_ != uniformBlock))
		{
			uniformBlock_ = uniformBlock;
			data_ = std::make_unique<std::byte[]>(uniformBlock_->GetSize());
			return false;
		}

		return true;
	}

	void PushHandler::BindPush(const CommandBuffer &commandBuffer, const Pipeline &pipeline)
	{
		vkCmdPushConstants(
			commandBuffer,
			pipeline.GetPipelineLayout(),
			uniformBlock_->GetStageFlags(),
			0,
			static_cast<uint32_t>(uniformBlock_->GetSize()),
			data_.get());
	}
}