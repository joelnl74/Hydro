#include "ThumbnailBaker.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <system_error>

namespace Hydro
{
	// "HYTH" and the layout version: a file from another layout is read as "no thumbnail".
	static constexpr uint32_t s_Magic = 0x48545948u;
	static constexpr uint32_t s_Layout = 1;

	struct ThumbnailFileHeader
	{
		uint32_t Magic = s_Magic;
		uint32_t Layout = s_Layout;
		uint64_t SourceVersion = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Flags = 0; // bit 0: downscaled, bit 1: sprite sheet
	};

	ThumbnailPixels ThumbnailBaker::Downscale(const uint8_t* rgba, uint32_t width, uint32_t height)
	{
		ThumbnailPixels thumbnail;
		if (rgba == nullptr || width == 0 || height == 0)
		{
			return thumbnail;
		}

		const uint32_t longest = std::max(width, height);
		if (longest <= MaxSize)
		{
			thumbnail.Width = width;
			thumbnail.Height = height;
			thumbnail.Pixels.assign(rgba, rgba + (size_t)width * height * 4);
			return thumbnail;
		}

		const double scale = (double)MaxSize / (double)longest;
		thumbnail.Width = std::max(1u, (uint32_t)std::lround(width * scale));
		thumbnail.Height = std::max(1u, (uint32_t)std::lround(height * scale));
		thumbnail.Downscaled = true;
		thumbnail.Pixels.resize((size_t)thumbnail.Width * thumbnail.Height * 4);

		for (uint32_t y = 0; y < thumbnail.Height; y++)
		{
			const uint64_t sourceY0 = (uint64_t)y * height / thumbnail.Height;
			const uint64_t sourceY1 = std::max<uint64_t>(sourceY0 + 1, (uint64_t)(y + 1) * height / thumbnail.Height);
			for (uint32_t x = 0; x < thumbnail.Width; x++)
			{
				const uint64_t sourceX0 = (uint64_t)x * width / thumbnail.Width;
				const uint64_t sourceX1 = std::max<uint64_t>(sourceX0 + 1, (uint64_t)(x + 1) * width / thumbnail.Width);

				uint64_t red = 0;
				uint64_t green = 0;
				uint64_t blue = 0;
				uint64_t alpha = 0;
				for (uint64_t sy = sourceY0; sy < sourceY1; sy++)
				{
					for (uint64_t sx = sourceX0; sx < sourceX1; sx++)
					{
						const uint8_t* pixel = rgba + (sy * width + sx) * 4;
						red += (uint64_t)pixel[0] * pixel[3];
						green += (uint64_t)pixel[1] * pixel[3];
						blue += (uint64_t)pixel[2] * pixel[3];
						alpha += pixel[3];
					}
				}

				const uint64_t count = (sourceX1 - sourceX0) * (sourceY1 - sourceY0);
				uint8_t* out = thumbnail.Pixels.data() + ((size_t)y * thumbnail.Width + x) * 4;
				if (alpha == 0)
				{
					out[0] = 0;
					out[1] = 0;
					out[2] = 0;
					out[3] = 0;
				}
				else
				{
					out[0] = (uint8_t)(red / alpha);
					out[1] = (uint8_t)(green / alpha);
					out[2] = (uint8_t)(blue / alpha);
					out[3] = (uint8_t)(alpha / count);
				}
			}
		}

		return thumbnail;
	}

	bool ThumbnailBaker::Write(const std::filesystem::path& file, uint64_t sourceVersion, const ThumbnailPixels& thumbnail)
	{
		if (thumbnail.Width == 0 || thumbnail.Height == 0 || thumbnail.Pixels.size() != (size_t)thumbnail.Width * thumbnail.Height * 4)
		{
			return false;
		}

		std::error_code ec;
		if (file.has_parent_path())
		{
			std::filesystem::create_directories(file.parent_path(), ec);
		}

		ThumbnailFileHeader header;
		header.SourceVersion = sourceVersion;
		header.Width = thumbnail.Width;
		header.Height = thumbnail.Height;
		header.Flags = (thumbnail.Downscaled ? 1u : 0u) | (thumbnail.IsSheet ? 2u : 0u);

		std::ofstream out(file, std::ios::binary | std::ios::trunc);
		if (out.is_open() == false)
		{
			return false;
		}

		out.write(reinterpret_cast<const char*>(&header), sizeof(header));
		out.write(reinterpret_cast<const char*>(thumbnail.Pixels.data()), (std::streamsize)thumbnail.Pixels.size());
		return out.good();
	}

	bool ThumbnailBaker::Read(const std::filesystem::path& file, uint64_t sourceVersion, ThumbnailPixels& thumbnail)
	{
		std::ifstream in(file, std::ios::binary);
		if (in.is_open() == false)
		{
			return false;
		}

		ThumbnailFileHeader header;
		in.read(reinterpret_cast<char*>(&header), sizeof(header));
		if (in.gcount() != (std::streamsize)sizeof(header) || header.Magic != s_Magic || header.Layout != s_Layout ||
			header.SourceVersion != sourceVersion || header.Width == 0 || header.Height == 0 ||
			header.Width > MaxSize || header.Height > MaxSize)
		{
			return false;
		}

		ThumbnailPixels loaded;
		loaded.Width = header.Width;
		loaded.Height = header.Height;
		loaded.Downscaled = (header.Flags & 1u) != 0;
		loaded.IsSheet = (header.Flags & 2u) != 0;
		loaded.Pixels.resize((size_t)header.Width * header.Height * 4);
		in.read(reinterpret_cast<char*>(loaded.Pixels.data()), (std::streamsize)loaded.Pixels.size());
		if (in.gcount() != (std::streamsize)loaded.Pixels.size())
		{
			return false;
		}

		thumbnail = std::move(loaded);
		return true;
	}
}
