#include "EditorSettings.h"

#include <Hydro/Core/Log.h>

#include <yaml-cpp/yaml.h>

#include <algorithm>
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

	static void ReadBool(const YAML::Node& node, const char* key, bool& value)
	{
		if (auto child = node[key])
			value = child.as<bool>(value);
	}

	static void ReadPath(const YAML::Node& node, const char* key, std::filesystem::path& value)
	{
		if (auto child = node[key])
			value = child.as<std::string>(value.string());
	}

	// A missing key gives an invalid node: asking it anything but "is it there?" throws
	// YAML::InvalidNode, so every reader checks that first.
	static void ReadVec2(const YAML::Node& node, const char* key, ImVec2& value)
	{
		const YAML::Node child = node[key];
		if (child.IsDefined() && child.IsSequence() && child.size() == 2)
			value = ImVec2(child[0].as<float>(value.x), child[1].as<float>(value.y));
	}

	static void ReadVec4(const YAML::Node& node, const char* key, ImVec4& value)
	{
		const YAML::Node child = node[key];
		if (child.IsDefined() && child.IsSequence() && child.size() == 4)
			value = ImVec4(child[0].as<float>(value.x), child[1].as<float>(value.y), child[2].as<float>(value.z), child[3].as<float>(value.w));
	}

	static void WriteStyle(YAML::Emitter& out, const EditorStyle& style)
	{
		out << YAML::Key << "Style" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Theme" << YAML::Value << EditorStyle::GetThemeName(style.Theme);
		WriteVec4(out, "Accent", style.Accent);
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
		WriteVec2(out, "CellPadding", style.CellPadding);
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
	}

	static EditorStyle ReadStyle(const YAML::Node& styleNode, int version)
	{
		// Start from the saved theme's preset, so anything the file doesn't list keeps that theme's value.
		EditorTheme theme = EditorTheme::Hydro;
		if (auto themeNode = styleNode["Theme"])
			EditorStyle::TryParseTheme(themeNode.as<std::string>(""), theme);

		ImVec4 accent = EditorStyle::GetDefaultAccent();
		ReadVec4(styleNode, "Accent", accent);

		EditorStyle style = EditorStyle::FromTheme(theme, accent);
		ReadFloat(styleNode, "FontScale", style.FontScale);

		// Version 1 stored the old Hydro theme's colours and sizes in full; they'd hide the new theme.
		if (version < 2)
			return style;

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
		ReadVec2(styleNode, "CellPadding", style.CellPadding);
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

		return style;
	}

	static bool ReadSettings(const YAML::Node& node, EditorSettings& settings);

	static std::string ToYaml(const EditorSettings& settings)
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "EditorSettings" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Version" << YAML::Value << EditorSettingsSerializer::Version;

		WriteStyle(out, settings.Style);

		out << YAML::Key << "Font" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Size" << YAML::Value << settings.FontSize;
		out << YAML::EndMap;

		out << YAML::Key << "PlayMode" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Tint" << YAML::Value << settings.PlayMode.Tint;
		WriteVec4(out, "TintColor", settings.PlayMode.TintColor);
		out << YAML::Key << "TintStrength" << YAML::Value << settings.PlayMode.TintStrength;
		out << YAML::EndMap;

		const EditorCameraSettings& camera = settings.Camera;
		out << YAML::Key << "EditorCamera" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "StartsIn2D" << YAML::Value << settings.CameraStartsIn2D;
		out << YAML::Key << "VerticalFOV" << YAML::Value << camera.VerticalFOV;
		out << YAML::Key << "NearClip" << YAML::Value << camera.NearClip;
		out << YAML::Key << "FarClip" << YAML::Value << camera.FarClip;
		out << YAML::Key << "OrbitSpeed" << YAML::Value << camera.OrbitSpeed;
		out << YAML::Key << "PanSpeed" << YAML::Value << camera.PanSpeed;
		out << YAML::Key << "ZoomSpeed" << YAML::Value << camera.ZoomSpeed;
		out << YAML::EndMap;

		out << YAML::Key << "Console" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "AutoScroll" << YAML::Value << settings.Console.AutoScroll;
		out << YAML::Key << "ShowTimestamps" << YAML::Value << settings.Console.ShowTimestamps;
		out << YAML::Key << "ShowSource" << YAML::Value << settings.Console.ShowSource;
		out << YAML::EndMap;

		out << YAML::Key << "Startup" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "ReopenLastProject" << YAML::Value << settings.Startup.ReopenLastProject;
		out << YAML::Key << "ReopenLastScene" << YAML::Value << settings.Startup.ReopenLastScene;
		out << YAML::EndMap;

		out << YAML::Key << "Autosave" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "Enabled" << YAML::Value << settings.Autosave.Enabled;
		out << YAML::Key << "IntervalMinutes" << YAML::Value << settings.Autosave.IntervalMinutes;
		out << YAML::EndMap;

		out << YAML::Key << "Session" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "LastProject" << YAML::Value << settings.Session.LastProject.generic_string();
		out << YAML::Key << "LastScene" << YAML::Value << settings.Session.LastScene.generic_string();
		out << YAML::EndMap;

		out << YAML::EndMap; // EditorSettings
		out << YAML::EndMap;
		return out.c_str();
	}

	static bool WriteFile(const std::string& text, const std::filesystem::path& path)
	{
		std::ofstream file(path);
		if (file.is_open() == false)
			return false;

		file << text;
		return true;
	}

	bool EditorSettingsSerializer::Save(const EditorSettings& settings, const std::filesystem::path& path)
	{
		if (WriteFile(ToYaml(settings), path) == false)
		{
			HY_ERROR("Could not write editor settings to {}", path.string());
			return false;
		}

		HY_INFO("Saved editor settings to {}", path.string());
		return true;
	}

	bool EditorSettingsSerializer::SaveSession(const EditorSession& session, const std::filesystem::path& path)
	{
		// Missing file: the defaults, plus the session.
		EditorSettings onDisk;
		Load(path, onDisk);
		onDisk.Session = session;

		if (WriteFile(ToYaml(onDisk), path) == false)
		{
			HY_WARN("Could not write the editor session to {}", path.string());
			return false;
		}
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

		const YAML::Node node = root["EditorSettings"];
		if (node.IsDefined() == false || node.IsMap() == false)
			return false;

		// A value of the wrong type (e.g. text where a number belongs) throws; treat the file as unreadable.
		try
		{
			return ReadSettings(node, settings);
		}
		catch (const YAML::Exception& e)
		{
			HY_ERROR("Could not read editor settings {}: {}", path.string(), e.what());
			return false;
		}
	}

	static bool ReadSettings(const YAML::Node& node, EditorSettings& settings)
	{

		const YAML::Node versionNode = node["Version"];
		const int version = versionNode ? versionNode.as<int>(1) : 1;
		EditorSettings loaded;

		if (const YAML::Node styleNode = node["Style"]; styleNode.IsDefined() && styleNode.IsMap())
		{
			loaded.Style = ReadStyle(styleNode, version);
			if (version < EditorSettingsSerializer::Version)
				HY_INFO("Editor settings are from an older version: colours and sizes reset to the {} theme", EditorStyle::GetThemeName(loaded.Style.Theme));
		}

		if (const YAML::Node font = node["Font"])
			ReadFloat(font, "Size", loaded.FontSize);

		if (const YAML::Node playMode = node["PlayMode"])
		{
			ReadBool(playMode, "Tint", loaded.PlayMode.Tint);
			ReadVec4(playMode, "TintColor", loaded.PlayMode.TintColor);
			ReadFloat(playMode, "TintStrength", loaded.PlayMode.TintStrength);
		}

		if (const YAML::Node camera = node["EditorCamera"])
		{
			ReadBool(camera, "StartsIn2D", loaded.CameraStartsIn2D);
			ReadFloat(camera, "VerticalFOV", loaded.Camera.VerticalFOV);
			ReadFloat(camera, "NearClip", loaded.Camera.NearClip);
			ReadFloat(camera, "FarClip", loaded.Camera.FarClip);
			ReadFloat(camera, "OrbitSpeed", loaded.Camera.OrbitSpeed);
			ReadFloat(camera, "PanSpeed", loaded.Camera.PanSpeed);
			ReadFloat(camera, "ZoomSpeed", loaded.Camera.ZoomSpeed);
		}

		if (const YAML::Node console = node["Console"])
		{
			ReadBool(console, "AutoScroll", loaded.Console.AutoScroll);
			ReadBool(console, "ShowTimestamps", loaded.Console.ShowTimestamps);
			ReadBool(console, "ShowSource", loaded.Console.ShowSource);
		}

		if (const YAML::Node startup = node["Startup"])
		{
			ReadBool(startup, "ReopenLastProject", loaded.Startup.ReopenLastProject);
			ReadBool(startup, "ReopenLastScene", loaded.Startup.ReopenLastScene);
		}

		if (const YAML::Node autosave = node["Autosave"])
		{
			ReadBool(autosave, "Enabled", loaded.Autosave.Enabled);
			ReadFloat(autosave, "IntervalMinutes", loaded.Autosave.IntervalMinutes);
			loaded.Autosave.IntervalMinutes = std::max(loaded.Autosave.IntervalMinutes, 0.5f);
		}

		if (const YAML::Node session = node["Session"])
		{
			ReadPath(session, "LastProject", loaded.Session.LastProject);
			ReadPath(session, "LastScene", loaded.Session.LastScene);
		}

		settings = loaded;
		return true;
	}

	bool EditorSettingsSerializer::ReadFontSize(const std::filesystem::path& path, float& fontSize)
	{
		std::error_code ec;
		if (std::filesystem::exists(path, ec) == false)
			return false;

		try
		{
			const YAML::Node size = YAML::LoadFile(path.string())["EditorSettings"]["Font"]["Size"];
			if (size.IsScalar() == false)
				return false;

			fontSize = std::clamp(size.as<float>(fontSize), 10.0f, 32.0f);
			return true;
		}
		catch (const YAML::Exception&)
		{
			return false;
		}
	}
}
