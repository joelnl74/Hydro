#include "hypch.h"
#include "ImageWriter.h"

// stb_image_write's implementation is compiled here, once for the whole engine. Vendored code: its
// warnings are not ours. STBIW_WINDOWS_UTF8 makes it open UTF-8 paths correctly on Windows.
#if defined(_MSC_VER)
	#pragma warning(push, 0)
#endif
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_WINDOWS_UTF8
#include <stb_image_write.h>
#if defined(_MSC_VER)
	#pragma warning(pop)
#endif

namespace Hydro
{
	bool ImageWriter::WritePNG(const std::filesystem::path& path, uint32_t width, uint32_t height, const uint8_t* rgba)
	{
		if (rgba == nullptr || width == 0 || height == 0)
		{
			HY_CORE_ERROR("ImageWriter::WritePNG: no pixels for {}", path.string());
			return false;
		}

		std::error_code error;
		if (path.has_parent_path())
		{
			std::filesystem::create_directories(path.parent_path(), error);
			if (error)
			{
				HY_CORE_ERROR("ImageWriter::WritePNG: can't create {} ({})", path.parent_path().string(), error.message());
				return false;
			}
		}

		// u8string is a std::string in C++17: UTF-8 bytes, which STBIW_WINDOWS_UTF8 expects.
		const std::string file = path.u8string();
		const int written = stbi_write_png(file.c_str(), (int)width, (int)height, 4, rgba, (int)width * 4);
		if (written == 0)
		{
			HY_CORE_ERROR("ImageWriter::WritePNG: can't write {}", path.string());
			return false;
		}

		return true;
	}
}
