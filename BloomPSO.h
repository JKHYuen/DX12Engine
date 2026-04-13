#pragma once

// This class manages/creates all pipeline states related to Bloom effect

class Device;
class RenderTarget;

class BloomPSO {
public:
	enum BloomRenderType {
		Prefilter,
		Downsample,
		Upsample,
		Combine,

		NumBloomRenderType
	};

	BloomPSO(Device& device, const RenderTarget& renderTarget);

private:


	
};

