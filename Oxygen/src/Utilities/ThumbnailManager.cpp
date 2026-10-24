#include "ThumbnailManager.h"
#include "ThumbnailBaker.h"

#include <Hydro/Asset/AssetManager.h>
#include <Hydro/Renderer/TextureImporter.h>

#include <imgui/imgui.h>

#include <system_error>
#include <unordered_map>

namespace Hydro
{
	// How much work one frame may do: reading a saved thumbnail costs 1, decoding an image 4.
	static constexpr int s_FrameBudget = 8;
	static constexpr int s_ReadCost = 1;
	static constexpr int s_BakeCost = 4;
	// The image's date is looked at this often (seconds), like the Project window's folder scan.
	static constexpr double s_CheckInterval = 1.0;

	struct ThumbnailEntry
	{
		Thumbnail Result;
		uint64_t Version = 0;     // of the image this was made from
		bool HasVersion = false;
		bool Done = false;        // made (or failed): nothing to do until the version changes
		double LastCheckTime = -100.0;
	};

	static std::unordered_map<uint64_t, ThumbnailEntry> s_Entries;
	static Thumbnail s_Empty;
	static int s_BudgetFrame = -1;
	static int s_BudgetLeft = 0;

	static bool TakeBudget(int cost)
	{
		const int frame = ImGui::GetFrameCount();
		if (frame != s_BudgetFrame)
		{
			s_BudgetFrame = frame;
			s_BudgetLeft = s_FrameBudget;
		}

		// Always allow the first piece of work of a frame, so a cost bigger than the budget still happens.
		if (s_BudgetLeft <= 0)
		{
			return false;
		}

		s_BudgetLeft -= cost;
		return true;
	}

	// What identifies the image as it is now: its size and date and its .hymeta's date.
	static bool ComputeVersion(const std::filesystem::path& file, uint64_t& version)
	{
		std::error_code ec;
		const uint64_t size = (uint64_t)std::filesystem::file_size(file, ec);
		if (ec)
		{
			return false;
		}

		const std::filesystem::file_time_type time = std::filesystem::last_write_time(file, ec);
		if (ec)
		{
			return false;
		}

		uint64_t value = 1469598103934665603ull;
		const uint64_t parts[3] = { size, (uint64_t)time.time_since_epoch().count(), (uint64_t)TextureImporter::GetImportConfigVersion(file) };
		for (uint64_t part : parts)
		{
			value ^= part;
			value *= 1099511628211ull;
		}
		version = value;
		return true;
	}

	static std::filesystem::path GetCacheFile(AssetHandle handle)
	{
		const std::filesystem::path projectDirectory = AssetManager::GetAssetDirectory().parent_path();
		return projectDirectory / ".hydro" / "thumbnails" / (std::to_string((uint64_t)handle) + ".thumb");
	}

	static Ref<Texture2D> Upload(const ThumbnailPixels& pixels)
	{
		ImageSpecification specification;
		specification.Format = ImageFormat::SRGB;
		specification.Usage = ImageUsage::Texture;
		specification.Width = pixels.Width;
		specification.Height = pixels.Height;

		// A downscaled picture is smooth; a small one (pixel art) stays crisp.
		TextureProperties properties;
		properties.SamplerFilter = pixels.Downscaled ? TextureFilter::Linear : TextureFilter::Nearest;
		properties.SamplerWrap = TextureWrap::Clamp;
		return CreateRef<Texture2D>(specification, pixels.Pixels.data(), properties);
	}

	const Thumbnail& ThumbnailManager::Get(AssetHandle handle)
	{
		const AssetMetadata* metadata = AssetManager::GetMetadata(handle);
		if (metadata == nullptr || metadata->IsMissing || metadata->Type != AssetType::Texture2D)
		{
			return s_Empty;
		}

		ThumbnailEntry& entry = s_Entries[(uint64_t)handle];
		const double now = ImGui::GetTime();
		const bool recheck = entry.HasVersion == false || now - entry.LastCheckTime >= s_CheckInterval;
		if (recheck == false && entry.Done)
		{
			return entry.Result;
		}

		const std::filesystem::path file = AssetManager::GetAbsolutePath(handle);
		if (recheck)
		{
			entry.LastCheckTime = now;
			uint64_t version = 0;
			if (ComputeVersion(file, version) == false)
			{
				entry.Result = Thumbnail();
				entry.HasVersion = false;
				entry.Done = true;
				return entry.Result;
			}

			if (entry.HasVersion == false || version != entry.Version)
			{
				entry.Version = version;
				entry.HasVersion = true;
				entry.Done = false;
			}
		}

		if (entry.Done || entry.HasVersion == false)
		{
			return entry.Result;
		}

		const std::filesystem::path cacheFile = GetCacheFile(handle);
		ThumbnailPixels pixels;
		if (TakeBudget(s_ReadCost) == false)
		{
			entry.Result.Pending = true;
			return entry.Result;
		}

		if (ThumbnailBaker::Read(cacheFile, entry.Version, pixels) == false)
		{
			// Not saved yet (or the image changed): decode it, and save the small copy.
			if (TakeBudget(s_BakeCost) == false)
			{
				entry.Result.Pending = true;
				return entry.Result;
			}

			ImportedTexture imported;
			if (TextureImporter::Import(file, TextureImportConfig(), imported))
			{
				pixels = ThumbnailBaker::Downscale(imported.Pixels.data(), imported.Specification.Width, imported.Specification.Height);
				pixels.IsSheet = imported.Config.Mode == SpriteMode::Multiple;
				ThumbnailBaker::Write(cacheFile, entry.Version, pixels);
			}
			else
			{
				// Logged by the importer; not tried again until the image changes.
				pixels = ThumbnailPixels();
			}
		}

		entry.Result = Thumbnail();
		if (pixels.Width > 0)
		{
			entry.Result.Texture = Upload(pixels);
			entry.Result.IsSheet = pixels.IsSheet;
		}
		entry.Done = true;
		return entry.Result;
	}

	void ThumbnailManager::Clear()
	{
		s_Entries.clear();
	}
}
