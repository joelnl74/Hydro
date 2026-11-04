#include "hypch.h"
#include "ComponentSerializers.h"

#include "Hydro/Asset/AssetManager.h"
#include "Hydro/Renderer/Font.h"
#include "Hydro/Renderer/Texture2D.h"

#include <algorithm>

namespace Hydro
{
	static const char* UIScaleModeToString(UIScaleMode mode)
	{
		switch (mode)
		{
		case UIScaleMode::ScaleWithScreen: return "ScaleWithScreen";
		case UIScaleMode::ConstantPixelSize: return "ConstantPixelSize";
		default: return "IntegerScale";
		}
	}

	static UIScaleMode UIScaleModeFromString(const std::string& name)
	{
		if (name == "ScaleWithScreen")
		{
			return UIScaleMode::ScaleWithScreen;
		}
		if (name == "ConstantPixelSize")
		{
			return UIScaleMode::ConstantPixelSize;
		}
		return UIScaleMode::IntegerScale;
	}

	static const char* UITextAlignHToString(TextAlignH align)
	{
		switch (align)
		{
		case TextAlignH::Center: return "Center";
		case TextAlignH::Right: return "Right";
		default: return "Left";
		}
	}

	static TextAlignH UITextAlignHFromString(const std::string& name)
	{
		if (name == "Center")
		{
			return TextAlignH::Center;
		}
		if (name == "Right")
		{
			return TextAlignH::Right;
		}
		return TextAlignH::Left;
	}

	static const char* UITextAlignVToString(TextAlignV align)
	{
		switch (align)
		{
		case TextAlignV::Middle: return "Middle";
		case TextAlignV::Bottom: return "Bottom";
		default: return "Top";
		}
	}

	static TextAlignV UITextAlignVFromString(const std::string& name)
	{
		if (name == "Middle")
		{
			return TextAlignV::Middle;
		}
		if (name == "Bottom")
		{
			return TextAlignV::Bottom;
		}
		return TextAlignV::Top;
	}

	static const char* UINavigationToString(UINavigation navigation)
	{
		switch (navigation)
		{
		case UINavigation::Explicit: return "Explicit";
		case UINavigation::None: return "None";
		default: return "Auto";
		}
	}

	static UINavigation UINavigationFromString(const std::string& name)
	{
		if (name == "Explicit")
		{
			return UINavigation::Explicit;
		}
		if (name == "None")
		{
			return UINavigation::None;
		}
		return UINavigation::Auto;
	}

	void ComponentSerializers::SaveUICanvas(YAML::Emitter& out, const UICanvasComponent& canvas)
	{
		out << YAML::Key << "ReferenceResolution" << YAML::Value << canvas.ReferenceResolution;
		out << YAML::Key << "ScaleMode" << YAML::Value << UIScaleModeToString(canvas.ScaleMode);
		out << YAML::Key << "SortOrder" << YAML::Value << canvas.SortOrder;
		out << YAML::Key << "Visible" << YAML::Value << canvas.Visible;
		out << YAML::Key << "BlocksGameplayInput" << YAML::Value << canvas.BlocksGameplayInput;
		// WorldSize and WorldPivot, only for World canvases.
		if (canvas.Space == UICanvasSpace::World)
		{
			out << YAML::Key << "Space" << YAML::Value << "World";
			out << YAML::Key << "WorldSize" << YAML::Value << canvas.WorldSize;
			out << YAML::Key << "WorldPivot" << YAML::Value << canvas.WorldPivot;
		}
		if (canvas.DrawDebug)
		{
			out << YAML::Key << "DrawDebug" << YAML::Value << true;
		}
	}

	void ComponentSerializers::LoadUICanvas(const YAML::Node& canvasNode, UICanvasComponent& canvas)
	{
		canvas.ReferenceResolution = glm::max(canvasNode["ReferenceResolution"].as<glm::vec2>(canvas.ReferenceResolution),
			glm::vec2(1.0f));
		canvas.ScaleMode = UIScaleModeFromString(canvasNode["ScaleMode"].as<std::string>("IntegerScale"));
		canvas.SortOrder = canvasNode["SortOrder"].as<int32_t>(canvas.SortOrder);
		canvas.Visible = canvasNode["Visible"].as<bool>(canvas.Visible);
		canvas.BlocksGameplayInput = canvasNode["BlocksGameplayInput"].as<bool>(canvas.BlocksGameplayInput);
		canvas.DrawDebug = canvasNode["DrawDebug"].as<bool>(false);
		canvas.Space = canvasNode["Space"].as<std::string>("Screen") == "World" ? UICanvasSpace::World : UICanvasSpace::Screen;
		canvas.WorldSize = glm::max(canvasNode["WorldSize"].as<glm::vec2>(canvas.WorldSize), glm::vec2(1.0f));
		canvas.WorldPivot = canvasNode["WorldPivot"].as<glm::vec2>(canvas.WorldPivot);
	}

	void ComponentSerializers::SaveUIRect(YAML::Emitter& out, const UIRectComponent& rect)
	{
		out << YAML::Key << "AnchorMin" << YAML::Value << rect.AnchorMin;
		out << YAML::Key << "AnchorMax" << YAML::Value << rect.AnchorMax;
		out << YAML::Key << "Pivot" << YAML::Value << rect.Pivot;
		out << YAML::Key << "AnchoredPosition" << YAML::Value << rect.AnchoredPosition;
		out << YAML::Key << "SizeDelta" << YAML::Value << rect.SizeDelta;
		out << YAML::Key << "Visible" << YAML::Value << rect.Visible;
	}

	void ComponentSerializers::LoadUIRect(const YAML::Node& rectNode, UIRectComponent& rect)
	{
		rect.AnchorMin = rectNode["AnchorMin"].as<glm::vec2>(rect.AnchorMin);
		rect.AnchorMax = rectNode["AnchorMax"].as<glm::vec2>(rect.AnchorMax);
		rect.Pivot = rectNode["Pivot"].as<glm::vec2>(rect.Pivot);
		rect.AnchoredPosition = rectNode["AnchoredPosition"].as<glm::vec2>(rect.AnchoredPosition);
		rect.SizeDelta = rectNode["SizeDelta"].as<glm::vec2>(rect.SizeDelta);
		rect.Visible = rectNode["Visible"].as<bool>(rect.Visible);
	}

	void ComponentSerializers::SaveUIImage(YAML::Emitter& out, const UIImageComponent& image)
	{
		if ((uint64_t)image.Texture != 0)
		{
			out << YAML::Key << "TextureHandle" << YAML::Value << (uint64_t)image.Texture;
			// By name, like a sprite's, so slicing the sheet again keeps it; the index is the fallback.
			const Ref<Texture2D> texture = AssetManager::GetLoadedAsset<Texture2D>(image.Texture);
			if (texture != nullptr && texture->GetSpriteMode() == SpriteMode::Multiple)
			{
				out << YAML::Key << "Sprite" << YAML::Value << texture->GetSprite(image.SpriteIndex).Name;
			}
		}
		out << YAML::Key << "SpriteIndex" << YAML::Value << image.SpriteIndex;
		out << YAML::Key << "Color" << YAML::Value << image.Color;
		out << YAML::Key << "NineSlice" << YAML::Value << image.NineSlice;
		out << YAML::Key << "FlipX" << YAML::Value << image.FlipX;
		out << YAML::Key << "FlipY" << YAML::Value << image.FlipY;
	}

	void ComponentSerializers::LoadUIImage(const YAML::Node& imageNode, UIImageComponent& image)
	{
		// Loaded now, like a sprite's texture, so the renderer (which never loads) finds it.
		Ref<Texture2D> texture;
		if (auto textureNode = imageNode["TextureHandle"])
		{
			image.Texture = AssetHandle(textureNode.as<uint64_t>(0));
			texture = (uint64_t)image.Texture != 0 ? AssetManager::GetAsset<Texture2D>(image.Texture) : nullptr;
		}
		image.SpriteIndex = imageNode["SpriteIndex"].as<uint32_t>(image.SpriteIndex);
		if (auto spriteNameNode = imageNode["Sprite"]; spriteNameNode && texture != nullptr)
		{
			const int32_t found = texture->FindSprite(spriteNameNode.as<std::string>(""));
			if (found >= 0)
			{
				image.SpriteIndex = (uint32_t)found;
			}
		}
		image.Color = imageNode["Color"].as<glm::vec4>(image.Color);
		image.NineSlice = glm::max(imageNode["NineSlice"].as<glm::vec4>(image.NineSlice), glm::vec4(0.0f));
		image.FlipX = imageNode["FlipX"].as<bool>(image.FlipX);
		image.FlipY = imageNode["FlipY"].as<bool>(image.FlipY);
	}

	void ComponentSerializers::SaveUIText(YAML::Emitter& out, const UITextComponent& text)
	{
		out << YAML::Key << "Text" << YAML::Value << text.Text;
		if ((uint64_t)text.Font != 0)
		{
			out << YAML::Key << "FontHandle" << YAML::Value << (uint64_t)text.Font;
		}
		out << YAML::Key << "Color" << YAML::Value << text.Color;
		out << YAML::Key << "Scale" << YAML::Value << text.Scale;
		out << YAML::Key << "AlignH" << YAML::Value << UITextAlignHToString(text.AlignH);
		out << YAML::Key << "AlignV" << YAML::Value << UITextAlignVToString(text.AlignV);
		out << YAML::Key << "Wrap" << YAML::Value << text.Wrap;
		out << YAML::Key << "LineSpacing" << YAML::Value << text.LineSpacing;
		out << YAML::Key << "VisibleCharacters" << YAML::Value << text.VisibleCharacters;
		if (text.ShadowColor.a > 0.0f)
		{
			out << YAML::Key << "ShadowColor" << YAML::Value << text.ShadowColor;
			out << YAML::Key << "ShadowOffset" << YAML::Value << text.ShadowOffset;
		}
	}

	void ComponentSerializers::LoadUIText(const YAML::Node& textNode, UITextComponent& text)
	{
		text.Text = textNode["Text"].as<std::string>(text.Text);
		// Loaded now, like a sprite's texture, so the renderer (which never loads) finds it.
		text.Font = AssetHandle(textNode["FontHandle"].as<uint64_t>(0));
		if ((uint64_t)text.Font != 0)
		{
			AssetManager::GetAsset<Font>(text.Font);
		}
		text.Color = textNode["Color"].as<glm::vec4>(text.Color);
		text.Scale = std::max(textNode["Scale"].as<uint32_t>(text.Scale), 1u);
		text.AlignH = UITextAlignHFromString(textNode["AlignH"].as<std::string>("Left"));
		text.AlignV = UITextAlignVFromString(textNode["AlignV"].as<std::string>("Top"));
		text.Wrap = textNode["Wrap"].as<bool>(text.Wrap);
		text.LineSpacing = AtLeast(0.0f, textNode["LineSpacing"].as<float>(text.LineSpacing));
		text.VisibleCharacters = std::max(textNode["VisibleCharacters"].as<int32_t>(text.VisibleCharacters), -1);
		text.ShadowColor = textNode["ShadowColor"].as<glm::vec4>(text.ShadowColor);
		text.ShadowOffset = textNode["ShadowOffset"].as<glm::vec2>(text.ShadowOffset);
	}

	void ComponentSerializers::SaveUILayoutGroup(YAML::Emitter& out, const UILayoutGroupComponent& group)
	{
		out << YAML::Key << "Direction" << YAML::Value
			<< (group.Direction == UILayoutDirection::Horizontal ? "Horizontal" : "Vertical");
		out << YAML::Key << "Spacing" << YAML::Value << group.Spacing;
		out << YAML::Key << "Padding" << YAML::Value << group.Padding;
		out << YAML::Key << "ChildAlignment" << YAML::Value
			<< (group.ChildAlignment == UILayoutAlignment::Center ? "Center"
			: (group.ChildAlignment == UILayoutAlignment::End ? "End" : "Start"));
		out << YAML::Key << "StretchCrossAxis" << YAML::Value << group.StretchCrossAxis;
		out << YAML::Key << "FitContent" << YAML::Value << group.FitContent;
	}

	void ComponentSerializers::LoadUILayoutGroup(const YAML::Node& groupNode, UILayoutGroupComponent& group)
	{
		group.Direction = groupNode["Direction"].as<std::string>("Vertical") == "Horizontal" ? UILayoutDirection::Horizontal
			: UILayoutDirection::Vertical;
		group.Spacing = AtLeast(0.0f, groupNode["Spacing"].as<float>(group.Spacing));
		group.Padding = glm::max(groupNode["Padding"].as<glm::vec4>(group.Padding), glm::vec4(0.0f));
		const std::string alignment = groupNode["ChildAlignment"].as<std::string>("Start");
		group.ChildAlignment = alignment == "Center" ? UILayoutAlignment::Center
			: (alignment == "End" ? UILayoutAlignment::End : UILayoutAlignment::Start);
		group.StretchCrossAxis = groupNode["StretchCrossAxis"].as<bool>(group.StretchCrossAxis);
		group.FitContent = groupNode["FitContent"].as<bool>(group.FitContent);
	}

	void ComponentSerializers::SaveUIScrollView(YAML::Emitter& out, const UIScrollViewComponent& scroll)
	{
		out << YAML::Key << "ScrollPosition" << YAML::Value << scroll.ScrollPosition;
		out << YAML::Key << "FollowFocus" << YAML::Value << scroll.FollowFocus;
		if ((uint64_t)scroll.Scrollbar != 0)
		{
			out << YAML::Key << "Scrollbar" << YAML::Value << (uint64_t)scroll.Scrollbar;
		}
	}

	void ComponentSerializers::LoadUIScrollView(const YAML::Node& scrollNode, UIScrollViewComponent& scroll)
	{
		scroll.ScrollPosition = AtLeast(0.0f, scrollNode["ScrollPosition"].as<float>(scroll.ScrollPosition));
		scroll.FollowFocus = scrollNode["FollowFocus"].as<bool>(scroll.FollowFocus);
		scroll.Scrollbar = UUID(scrollNode["Scrollbar"].as<uint64_t>(0));
	}

	void ComponentSerializers::SaveUIFocusVisibility(YAML::Emitter& out, const UIFocusVisibilityComponent& visibility)
	{
		out << YAML::Key << "Show" << YAML::Value
			<< (visibility.Show == UIFocusVisibility::Unfocused ? "Unfocused" : "Focused");
	}

	void ComponentSerializers::LoadUIFocusVisibility(const YAML::Node& visibilityNode, UIFocusVisibilityComponent& visibility)
	{
		visibility.Show = visibilityNode["Show"].as<std::string>("Focused") == "Unfocused" ? UIFocusVisibility::Unfocused
			: UIFocusVisibility::Focused;
	}

	void ComponentSerializers::SaveUIButton(YAML::Emitter& out, const UIButtonComponent& button)
	{
		out << YAML::Key << "Interactable" << YAML::Value << button.Interactable;
		out << YAML::Key << "Event" << YAML::Value << button.Event;
		out << YAML::Key << "NormalTint" << YAML::Value << button.NormalTint;
		out << YAML::Key << "FocusedTint" << YAML::Value << button.FocusedTint;
		out << YAML::Key << "PressedTint" << YAML::Value << button.PressedTint;
		out << YAML::Key << "DisabledTint" << YAML::Value << button.DisabledTint;
		out << YAML::Key << "Navigation" << YAML::Value << UINavigationToString(button.Navigation);
		// Explicit navigation's neighbours, only when set.
		const std::pair<const char*, UUID> neighbours[] = { { "Up", button.Up }, { "Down", button.Down }, { "Left", button.Left },
			{ "Right", button.Right } };
		for (const auto& neighbour : neighbours)
		{
			if ((uint64_t)neighbour.second != 0)
			{
				out << YAML::Key << neighbour.first << YAML::Value << (uint64_t)neighbour.second;
			}
		}
	}

	void ComponentSerializers::LoadUIButton(const YAML::Node& buttonNode, UIButtonComponent& button)
	{
		button.Interactable = buttonNode["Interactable"].as<bool>(button.Interactable);
		button.Event = buttonNode["Event"].as<std::string>(button.Event);
		button.NormalTint = buttonNode["NormalTint"].as<glm::vec4>(button.NormalTint);
		button.FocusedTint = buttonNode["FocusedTint"].as<glm::vec4>(button.FocusedTint);
		button.PressedTint = buttonNode["PressedTint"].as<glm::vec4>(button.PressedTint);
		button.DisabledTint = buttonNode["DisabledTint"].as<glm::vec4>(button.DisabledTint);
		button.Navigation = UINavigationFromString(buttonNode["Navigation"].as<std::string>("Auto"));
		button.Up = UUID(buttonNode["Up"].as<uint64_t>(0));
		button.Down = UUID(buttonNode["Down"].as<uint64_t>(0));
		button.Left = UUID(buttonNode["Left"].as<uint64_t>(0));
		button.Right = UUID(buttonNode["Right"].as<uint64_t>(0));
	}
}
