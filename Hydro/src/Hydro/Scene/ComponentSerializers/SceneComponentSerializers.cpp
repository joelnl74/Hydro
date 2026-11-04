#include "hypch.h"
#include "ComponentSerializers.h"

namespace Hydro
{
	void ComponentSerializers::SaveTransform(YAML::Emitter& out, const TransformComponent& transform)
	{
		out << YAML::Key << "Translation" << YAML::Value << transform.Translation;
		out << YAML::Key << "Rotation" << YAML::Value << transform.Rotation;
		out << YAML::Key << "Scale" << YAML::Value << transform.Scale;
	}

	void ComponentSerializers::LoadTransform(const YAML::Node& transformNode, TransformComponent& transform)
	{
		transform.Translation = transformNode["Translation"].as<glm::vec3>(transform.Translation);
		transform.Rotation = transformNode["Rotation"].as<glm::vec3>(transform.Rotation);
		transform.Scale = transformNode["Scale"].as<glm::vec3>(transform.Scale);
	}

	void ComponentSerializers::SaveCamera(YAML::Emitter& out, const CameraComponent& cameraComponent)
	{
		const SceneCamera& camera = cameraComponent.Camera;

		out << YAML::Key << "Primary" << YAML::Value << cameraComponent.Primary;
		out << YAML::Key << "Projection" << YAML::Value
			<< (camera.Projection == ProjectionType::Perspective ? "Perspective" : "Orthographic");
		out << YAML::Key << "PerspectiveFOV" << YAML::Value << camera.PerspectiveFOV;
		out << YAML::Key << "PerspectiveNear" << YAML::Value << camera.PerspectiveNear;
		out << YAML::Key << "PerspectiveFar" << YAML::Value << camera.PerspectiveFar;
		out << YAML::Key << "OrthographicSize" << YAML::Value << camera.OrthographicSize;
		out << YAML::Key << "OrthographicNear" << YAML::Value << camera.OrthographicNear;
		out << YAML::Key << "OrthographicFar" << YAML::Value << camera.OrthographicFar;
		out << YAML::Key << "FocusDistance" << YAML::Value << camera.FocusDistance;
	}

	void ComponentSerializers::LoadCamera(const YAML::Node& cameraNode, CameraComponent& cameraComponent)
	{
		SceneCamera& camera = cameraComponent.Camera;

		cameraComponent.Primary = cameraNode["Primary"].as<bool>(cameraComponent.Primary);
		camera.Projection = cameraNode["Projection"].as<std::string>("Orthographic") == "Perspective" ? ProjectionType::Perspective
			: ProjectionType::Orthographic;
		camera.PerspectiveFOV = cameraNode["PerspectiveFOV"].as<float>(camera.PerspectiveFOV);
		camera.PerspectiveNear = cameraNode["PerspectiveNear"].as<float>(camera.PerspectiveNear);
		camera.PerspectiveFar = cameraNode["PerspectiveFar"].as<float>(camera.PerspectiveFar);
		camera.OrthographicSize = cameraNode["OrthographicSize"].as<float>(camera.OrthographicSize);
		camera.OrthographicNear = cameraNode["OrthographicNear"].as<float>(camera.OrthographicNear);
		camera.OrthographicFar = cameraNode["OrthographicFar"].as<float>(camera.OrthographicFar);
		if (auto focusDistance = cameraNode["FocusDistance"])
		{
			camera.FocusDistance = focusDistance.as<float>(camera.FocusDistance);
		}
	}

	void ComponentSerializers::SaveLight2D(YAML::Emitter& out, const Light2DComponent& light)
	{
		out << YAML::Key << "Type" << YAML::Value
			<< (light.Type == Light2DType::Global ? "Global" : (light.Type == Light2DType::Spot ? "Spot" : "Point"));
		out << YAML::Key << "Color" << YAML::Value << light.Color;
		out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
		out << YAML::Key << "Radius" << YAML::Value << light.Radius;
		out << YAML::Key << "Height" << YAML::Value << light.Height;
		if (light.Type == Light2DType::Spot)
		{
			out << YAML::Key << "InnerAngle" << YAML::Value << light.InnerAngle;
			out << YAML::Key << "OuterAngle" << YAML::Value << light.OuterAngle;
		}
	}

	void ComponentSerializers::LoadLight2D(const YAML::Node& lightNode, Light2DComponent& light)
	{
		const std::string type = lightNode["Type"].as<std::string>("Point");
		light.Type = type == "Global" ? Light2DType::Global : (type == "Spot" ? Light2DType::Spot : Light2DType::Point);
		light.Color = lightNode["Color"].as<glm::vec3>(light.Color);
		light.Intensity = lightNode["Intensity"].as<float>(light.Intensity);
		light.Radius = lightNode["Radius"].as<float>(light.Radius);
		light.Height = lightNode["Height"].as<float>(light.Height);
		light.InnerAngle = lightNode["InnerAngle"].as<float>(light.InnerAngle);
		light.OuterAngle = std::max(lightNode["OuterAngle"].as<float>(light.OuterAngle), light.InnerAngle);
	}

	void ComponentSerializers::SaveDirectionalLight(YAML::Emitter& out, const DirectionalLightComponent& light)
	{
		out << YAML::Key << "Color" << YAML::Value << light.Color;
		out << YAML::Key << "Intensity" << YAML::Value << light.Intensity;
		out << YAML::Key << "IsMainLight" << YAML::Value << light.IsMainLight;
		out << YAML::Key << "CastShadows" << YAML::Value << light.CastShadows;
		out << YAML::Key << "ShadowStrength" << YAML::Value << light.ShadowStrength;
		out << YAML::Key << "ShadowDistance" << YAML::Value << light.ShadowDistance;
		out << YAML::Key << "ShadowBias" << YAML::Value << light.ShadowBias;
	}

	void ComponentSerializers::LoadDirectionalLight(const YAML::Node& sunNode, DirectionalLightComponent& light)
	{
		light.Color = sunNode["Color"].as<glm::vec3>(light.Color);
		light.Intensity = sunNode["Intensity"].as<float>(light.Intensity);
		light.IsMainLight = sunNode["IsMainLight"].as<bool>(light.IsMainLight);
		light.CastShadows = sunNode["CastShadows"].as<bool>(light.CastShadows);
		light.ShadowStrength = sunNode["ShadowStrength"].as<float>(light.ShadowStrength);
		light.ShadowDistance = sunNode["ShadowDistance"].as<float>(light.ShadowDistance);
		light.ShadowBias = sunNode["ShadowBias"].as<float>(light.ShadowBias);
	}
}
