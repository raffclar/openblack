				else if (isValid(shaderBind.uniformHandle)
				     &&  ShaderBinding::Type::Sampler == shaderBind.type
				     &&  !_isCompute)
				{
					bindGroupEntry[entryCount++] =
					{
						.nextInChain = NULL,
						.binding     = shaderBind.binding,
						.buffer      = NULL,
						.offset      = 0,
						.size        = 0,
						.sampler     = NULL,
						.textureView = getUnboundTextureView(shaderBind.viewDimension),
					};

					bindGroupEntry[entryCount++] =
					{
						.nextInChain = NULL,
						.binding     = shaderBind.samplerBinding,
						.buffer      = NULL,
						.offset      = 0,
						.size        = 0,
						.sampler     = getUnboundSampler(),
						.textureView = NULL,
					};
				}
