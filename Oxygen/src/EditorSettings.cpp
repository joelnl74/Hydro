#include "EditorSettings.h"

#include <Hydro/Core/Log.h>

#include <yaml-cpp/yaml.h>

#include <fstream>

namespace Hydro
{
	// ImGui vectors as flow sequences, like the engine's glm conversions: [x, y] / [r, g, b, a].
	static void WriteVec2(YAML::Emitter& out, const char* key, const ImVec2& value)
	{
		out << YAML::Key << key << YAML::Value << YAML::Flow << YAML::BeginSeq << value.x << value.y << YAML::EndSeq;
	}

	static void WriteVec4(YAML::Emitter& out, const char* key, const ImVec4& value)
	{
		out << YAML::Key << key << YAML::Value << YAML::Flow << YAML::BeginSeq
			<< value.x << value.y << value.z << value.w << YAML::EndSeq;
	}

	static void ReadFloat(const YAML::Node& node, const char* key, float& value)
	{
		if (auto child = node[key])
			value = child.as<float>(value);
	}

	static void ReadVec2(const YAML::Node& node, const char* key, ImVec2& value)
	{
		const YAML::Node child = node[key];
		if (child.IsSequence() && child.size() == 2)
			value = ImVec2(child[0].as<float>(value.x), child[1].as<float>(value.y));
	}

	static void ReadVec4(const YAML::Node& node, const char* key, ImVec4& value)
	{
		const YAML::Node child = node[key];
		if (child.IsSequence() && child.size() == 4)
			value = ImVec4(child[0].as<float>(value.x), child[1].as<float>(value.y), child[2].as<float>(value.z), child[3].as<float>(value.w));
	}

	bool EditorSettingsSerializer::Save(const EditorSettings& settings, const std::filesystem::path& path)
	{
		const EditorStyle& style = settings.Style;

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;

		out << YAML::Key << "Style" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Theme" << YAML::Value << EditorStyle::GetThemeName(style.Theme);
		out << YAML::Key << "FontScale" << YAML::Value << style.FontScale;

		out << YAML::Key << "WindowRounding" << YAML::Value << style.WindowRounding;
		out << YAML::Key << "ChildRounding" << YAML::Value << style.ChildRounding;
		out << YAML::Key << "FrameRounding" << YAML::Value << style.FrameRounding;
		out << YAML::Key << "PopupRounding" << YAML::Value << style.PopupRounding;
		out << YAML::Key << "ScrollbarRounding" << YAML::Value << style.ScrollbarRounding;
		out << YAML::Key << "GrabRounding" << YAML::Value << style.GrabRounding;
		out << YAML::Key << "TabRounding" << YAML::Value << style.TabRounding;

		out << YAML::Key << "WindowBorderSize" << YAML::Value << style.WindowBorderSize;
		out << YAML::Key << "ChildBorderSize" << YAML::Value << style.ChildBorderSize;
		out << YAML::Key << "FrameBorderSize" << YAML::Value << style.FrameBorderSize;
		out << YAML::Key << "PopupBorderSize" << YAML::Value << style.PopupBorderSize;
		out << YAML::Key << "TabBorderSize" << YAML::Value << style.TabBorderSize;

		WriteVec2(out, "WindowPadding", style.WindowPadding);
		WriteVec2(out, "FramePadding", style.FramePadding);
		WriteVec2(out, "ItemSpacing", style.ItemSpacing);
		WriteVec2(out, "ItemInnerSpacing", style.ItemInnerSpacing);
		out << YAML::Key << "IndentSpacing" << YAML::Value << style.IndentSpacing;
		out << YAML::Key << "ScrollbarSize" << YAML::Value << style.ScrollbarSize;
		out << YAML::Key << "GrabMinSize" << YAML::Value << style.GrabMinSize;

		// Colours by ImGui's own names, so the file stays readable and survives ImGui adding colours.
		out << YAML::Key << "Colors" << YAML::Value << YAML::BeginMap;
		for (int i = 0; i < ImGuiCol_COUNT; i++)
			WriteVec4(out, ImGui::GetStyleColorName(i), style.Colors[i]);
		out << YAML::EndMap; // Colors

		out << YAML::EndMap; // Style
		out << YAML::EndMap; // EditorSettings
		out << YAML::EndMap;

		std::ofstream file(path);
		if (file.is_open() == false)
		{
			HY_ERROR("Could not write editor settings to {}", path.string());
			return false;
		}

		file << out.c_str();
		HY_INFO("Saved editor settings to {}", path.string());
		return true;
	}

	bool EditorSettingsSerializer::Load(const std::filesystem::path& path, EditorSettings& settings)
	{
		std::error_code ec;
		if (std::filesystem::exists(path, ec) == false)
			return false;

		YAML::Node root;
		try
		{
			root = YAML::LoadFile(path.string());
		}
		catch (const YAML::Exception& e)
		{
			HY_ERROR("Could not read editor settings {}: {}", path.string(), e.what());
			return false;
		}

		const YAML::Node styleNode = root["EditorSettings"]["Style"];
		if (styleNode.IsMap() == false)
			return false;

		// Start from the saved theme's preset, so anything the file doesn't list keeps that theme's value.
		EditorTheme theme = EditorTheme::Hydro;
		if (auto themeNode = styleNode["Theme"])
			EditorStyle::TryParseTheme(themeNode.as<std::string>(""), theme);

		EditorStyle style = EditorStyle::FromTheme(theme);
		ReadFloat(styleNode, "FontScale", style.FontScale);

		ReadFloat(styleNode, "WindowRounding", style.WindowRounding);
		ReadFloat(styleNode, "ChildRounding", style.ChildRounding);
		ReadFloat(styleNode, "FrameRounding", style.FrameRounding);
		ReadFloat(styleNode, "PopupRounding", style.PopupRounding);
		ReadFloat(styleNode, "ScrollbarRounding", style.ScrollbarRounding);
		ReadFloat(styleNode, "GrabRounding", style.GrabRounding);
		ReadFloat(styleNode, "TabRounding", style.TabRounding);

		ReadFloat(styleNode, "WindowBorderSize", style.WindowBorderSize);
		ReadFloat(styleNode, "ChildBorderSize", style.ChildBorderSize);
		ReadFloat(styleNode, "FrameBorderSize", style.FrameBorderSize);
		ReadFloat(styleNode, "PopupBorderSize", style.PopupBorderSize);
		ReadFloat(styleNode, "TabBorderSize", style.TabBorderSize);

		ReadVec2(styleNode, "WindowPadding", style.WindowPadding);
		ReadVec2(styleNode, "FramePadding", style.FramePadding);
		ReadVec2(styleNode, "ItemSpacing", style.ItemSpacing);
		ReadVec2(styleNode, "ItemInnerSpacing", style.ItemInnerSpacing);
		ReadFloat(styleNode, "IndentSpacing", style.IndentSpacing);
		ReadFloat(styleNode, "ScrollbarSize", style.ScrollbarSize);
		ReadFloat(styleNode, "GrabMinSize", style.GrabMinSize);

		if (const YAML::Node colors = styleNode["Colors"])
		{
			for (int i = 0; i < ImGuiCol_COUNT; i++)
				ReadVec4(colors, ImGui::GetStyleColorName(i), style.Colors[i]);
		}

		settings.Style = style;
		return true;
	}
}
