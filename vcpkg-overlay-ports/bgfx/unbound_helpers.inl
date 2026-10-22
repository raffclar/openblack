		// A shader can declare a texture that a draw does not set. WebGPU needs an entry for each binding of the
		// layout, so such a binding gets a small black texture and a default sampler.
		WGPUTextureView getUnboundTextureView(WGPUTextureViewDimension _dimension)
		{
			static WGPUTextureView s_view[4] = {};

			const uint32_t idx = WGPUTextureViewDimension_2DArray == _dimension ? 1
				: WGPUTextureViewDimension_Cube == _dimension ? 2
				: WGPUTextureViewDimension_3D   == _dimension ? 3
				: 0
				;

			if (NULL == s_view[idx])
			{
				const uint32_t layers = 2 == idx ? 6 : 1;

				const WGPUTextureDescriptor textureDesc =
				{
					.nextInChain     = NULL,
					.label           = toWGPUStringView("Unbound"),
					.usage           = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst,
					.dimension       = 3 == idx ? WGPUTextureDimension_3D : WGPUTextureDimension_2D,
					.size            = { 1, 1, layers },
					.format          = WGPUTextureFormat_RGBA8Unorm,
					.mipLevelCount   = 1,
					.sampleCount     = 1,
					.viewFormatCount = 0,
					.viewFormats     = NULL,
				};

				WGPUTexture texture = WGPU_CHECK(wgpuDeviceCreateTexture(m_device, &textureDesc) );

				const WGPUTextureViewDescriptor viewDesc =
				{
					.nextInChain     = NULL,
					.label           = toWGPUStringView("Unbound"),
					.format          = WGPUTextureFormat_RGBA8Unorm,
					.dimension       = 0 == idx ? WGPUTextureViewDimension_2D
						: 1 == idx ? WGPUTextureViewDimension_2DArray
						: 2 == idx ? WGPUTextureViewDimension_Cube
						: WGPUTextureViewDimension_3D
						,
					.baseMipLevel    = 0,
					.mipLevelCount   = 1,
					.baseArrayLayer  = 0,
					.arrayLayerCount = layers,
					.aspect          = WGPUTextureAspect_All,
					.usage           = WGPUTextureUsage_TextureBinding,
				};

				s_view[idx] = WGPU_CHECK(wgpuTextureCreateView(texture, &viewDesc) );
			}

			return s_view[idx];
		}

		WGPUSampler getUnboundSampler()
		{
			static WGPUSampler s_sampler = NULL;

			if (NULL == s_sampler)
			{
				s_sampler = WGPU_CHECK(wgpuDeviceCreateSampler(m_device, NULL) );
			}

			return s_sampler;
		}

