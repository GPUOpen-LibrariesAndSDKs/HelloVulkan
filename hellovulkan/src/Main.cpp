#include "VulkanQuad.h"

int WinMain (
	_In_ HINSTANCE /* hInstance */,
	_In_opt_ HINSTANCE /* hPrevInstance */,
	_In_ LPSTR     /* lpCmdLine */,
	_In_ int       /* nCmdShow */
	)
{
	AMD::VulkanSample* sample = nullptr;

	const auto sampleId = 0;

	switch (sampleId) {
	case 0: sample = new AMD::VulkanQuad; break;
	}

	if (sample == nullptr || sample->IsInitialized() == false) {
		return 1;
	}

	sample->Run (512);
	delete sample;

	return 0;
}