#include "Utility.h"

#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////
std::vector<std::uint8_t> ReadFile (const char* filename)
{
	std::vector<std::uint8_t> result;
	std::uint8_t buffer[4096];

	auto handle = std::fopen (filename, "rb");

	for (;;) {
		const auto bytesRead = std::fread (buffer, 1, sizeof (buffer), handle);

		result.insert (result.end (), buffer, buffer + bytesRead);

		if (bytesRead < sizeof (buffer)) {
			break;
		}
	}

	std::fclose (handle);

	return result;
}