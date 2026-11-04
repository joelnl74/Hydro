#include "hypch.h"
#include "ComponentSerializers.h"

#include <algorithm>

namespace Hydro
{
	static const char* Rigidbody2DTypeToString(Rigidbody2DType type)
	{
		switch (type)
		{
		case Rigidbody2DType::Static: return "Static";
		case Rigidbody2DType::Kinematic: return "Kinematic";
		case Rigidbody2DType::Dynamic: return "Dynamic";
		}

		return "Dynamic";
	}

	static Rigidbody2DType Rigidbody2DTypeFromString(const std::string& name)
	{
		if (name == "Static")
		{
			return Rigidbody2DType::Static;
		}
		if (name == "Kinematic")
		{
			return Rigidbody2DType::Kinematic;
		}

		return Rigidbody2DType::Dynamic;
	}

	// The keys every 3D collider has.
	static void SerializeCollider3D(YAML::Emitter& out, PhysicsLayer layer, bool isTrigger)
	{
		out << YAML::Key << "Layer" << YAML::Value << ComponentSerializers::PhysicsLayerToString(layer);
		out << YAML::Key << "IsTrigger" << YAML::Value << isTrigger;
	}

	static void DeserializeCollider3D(const YAML::Node& node, PhysicsLayer& layer, bool& isTrigger)
	{
		const std::string saved = node["Layer"].as<std::string>(ComponentSerializers::PhysicsLayerToString(layer));
		layer = ComponentSerializers::PhysicsLayerFromString(saved, layer);
		isTrigger = node["IsTrigger"].as<bool>(isTrigger);
	}

	static void SerializeMaterial(YAML::Emitter& out, const PhysicsMaterial2D& material)
	{
		out << YAML::Key << "Density" << YAML::Value << material.Density;
		out << YAML::Key << "Friction" << YAML::Value << material.Friction;
		out << YAML::Key << "Restitution" << YAML::Value << material.Restitution;
	}

	static void DeserializeMaterial(const YAML::Node& node, PhysicsMaterial2D& material)
	{
		material.Density = ComponentSerializers::AtLeast(0.0f, node["Density"].as<float>(material.Density));
		material.Friction = ComponentSerializers::AtLeast(0.0f, node["Friction"].as<float>(material.Friction));
		material.Restitution = ComponentSerializers::AtLeast(0.0f, node["Restitution"].as<float>(material.Restitution));
	}

	void ComponentSerializers::SaveRigidbody2D(YAML::Emitter& out, const Rigidbody2DComponent& body)
	{
		out << YAML::Key << "Type" << YAML::Value << Rigidbody2DTypeToString(body.Type);
		out << YAML::Key << "FixedRotation" << YAML::Value << body.FixedRotation;
		out << YAML::Key << "IsBullet" << YAML::Value << body.IsBullet;
		out << YAML::Key << "GravityScale" << YAML::Value << body.GravityScale;
		out << YAML::Key << "LinearDamping" << YAML::Value << body.LinearDamping;
		out << YAML::Key << "AngularDamping" << YAML::Value << body.AngularDamping;
	}

	void ComponentSerializers::LoadRigidbody2D(const YAML::Node& bodyNode, Rigidbody2DComponent& body)
	{
		body.Type = Rigidbody2DTypeFromString(bodyNode["Type"].as<std::string>("Dynamic"));
		body.FixedRotation = bodyNode["FixedRotation"].as<bool>(body.FixedRotation);
		body.IsBullet = bodyNode["IsBullet"].as<bool>(body.IsBullet);
		body.GravityScale = bodyNode["GravityScale"].as<float>(body.GravityScale);
		body.LinearDamping = AtLeast(0.0f, bodyNode["LinearDamping"].as<float>(body.LinearDamping));
		body.AngularDamping = AtLeast(0.0f, bodyNode["AngularDamping"].as<float>(body.AngularDamping));
	}

	void ComponentSerializers::SaveBoxCollider2D(YAML::Emitter& out, const BoxCollider2DComponent& collider)
	{
		out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
		out << YAML::Key << "Size" << YAML::Value << collider.Size;
		SerializeMaterial(out, collider.Material);
		out << YAML::Key << "IsSensor" << YAML::Value << collider.IsSensor;
	}

	void ComponentSerializers::LoadBoxCollider2D(const YAML::Node& boxNode, BoxCollider2DComponent& collider)
	{
		collider.Offset = boxNode["Offset"].as<glm::vec2>(collider.Offset);
		collider.Size = boxNode["Size"].as<glm::vec2>(collider.Size);
		collider.Size.x = AtLeast(0.001f, collider.Size.x);
		collider.Size.y = AtLeast(0.001f, collider.Size.y);
		DeserializeMaterial(boxNode, collider.Material);
		collider.IsSensor = boxNode["IsSensor"].as<bool>(collider.IsSensor);
	}

	void ComponentSerializers::SaveCircleCollider2D(YAML::Emitter& out, const CircleCollider2DComponent& collider)
	{
		out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
		out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
		SerializeMaterial(out, collider.Material);
		out << YAML::Key << "IsSensor" << YAML::Value << collider.IsSensor;
	}

	void ComponentSerializers::LoadCircleCollider2D(const YAML::Node& circleNode, CircleCollider2DComponent& collider)
	{
		collider.Offset = circleNode["Offset"].as<glm::vec2>(collider.Offset);
		collider.Radius = AtLeast(0.001f, circleNode["Radius"].as<float>(collider.Radius));
		DeserializeMaterial(circleNode, collider.Material);
		collider.IsSensor = circleNode["IsSensor"].as<bool>(collider.IsSensor);
	}

	void ComponentSerializers::SaveBoxCollider(YAML::Emitter& out, const BoxColliderComponent& collider)
	{
		out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
		out << YAML::Key << "Size" << YAML::Value << collider.Size;
		SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::LoadBoxCollider(const YAML::Node& boxNode, BoxColliderComponent& collider)
	{
		collider.Offset = boxNode["Offset"].as<glm::vec3>(collider.Offset);
		collider.Size = boxNode["Size"].as<glm::vec3>(collider.Size);
		collider.Size.x = AtLeast(0.001f, collider.Size.x);
		collider.Size.y = AtLeast(0.001f, collider.Size.y);
		collider.Size.z = AtLeast(0.001f, collider.Size.z);
		DeserializeCollider3D(boxNode, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::SaveSphereCollider(YAML::Emitter& out, const SphereColliderComponent& collider)
	{
		out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
		out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
		SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::LoadSphereCollider(const YAML::Node& sphereNode, SphereColliderComponent& collider)
	{
		collider.Offset = sphereNode["Offset"].as<glm::vec3>(collider.Offset);
		collider.Radius = AtLeast(0.001f, sphereNode["Radius"].as<float>(collider.Radius));
		DeserializeCollider3D(sphereNode, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::SaveCapsuleCollider(YAML::Emitter& out, const CapsuleColliderComponent& collider)
	{
		out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
		out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
		out << YAML::Key << "Height" << YAML::Value << collider.Height;
		SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::LoadCapsuleCollider(const YAML::Node& capsuleNode, CapsuleColliderComponent& collider)
	{
		collider.Offset = capsuleNode["Offset"].as<glm::vec3>(collider.Offset);
		collider.Radius = AtLeast(0.001f, capsuleNode["Radius"].as<float>(collider.Radius));
		collider.Height = AtLeast(0.001f, capsuleNode["Height"].as<float>(collider.Height));
		DeserializeCollider3D(capsuleNode, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::SaveCylinderCollider(YAML::Emitter& out, const CylinderColliderComponent& collider)
	{
		out << YAML::Key << "Offset" << YAML::Value << collider.Offset;
		out << YAML::Key << "Radius" << YAML::Value << collider.Radius;
		out << YAML::Key << "Height" << YAML::Value << collider.Height;
		SerializeCollider3D(out, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::LoadCylinderCollider(const YAML::Node& cylinderNode, CylinderColliderComponent& collider)
	{
		collider.Offset = cylinderNode["Offset"].as<glm::vec3>(collider.Offset);
		collider.Radius = AtLeast(0.001f, cylinderNode["Radius"].as<float>(collider.Radius));
		collider.Height = AtLeast(0.001f, cylinderNode["Height"].as<float>(collider.Height));
		DeserializeCollider3D(cylinderNode, collider.Layer, collider.IsTrigger);
	}

	void ComponentSerializers::SaveMeshCollider(YAML::Emitter& out, const MeshColliderComponent& collider)
	{
		out << YAML::Key << "Layer" << YAML::Value << PhysicsLayerToString(collider.Layer);
	}

	void ComponentSerializers::LoadMeshCollider(const YAML::Node& meshColliderNode, MeshColliderComponent& collider)
	{
		collider.Layer = PhysicsLayerFromString(meshColliderNode["Layer"].as<std::string>("World"), collider.Layer);
	}

	void ComponentSerializers::SaveCharacterBody(YAML::Emitter& out, const CharacterBodyComponent& body)
	{
		out << YAML::Key << "Radius" << YAML::Value << body.Radius;
		out << YAML::Key << "Height" << YAML::Value << body.Height;
		out << YAML::Key << "MaxSlopeDegrees" << YAML::Value << body.MaxSlopeDegrees;
		out << YAML::Key << "StepHeight" << YAML::Value << body.StepHeight;
		out << YAML::Key << "StaysOnLedges" << YAML::Value << body.StaysOnLedges;
		out << YAML::Key << "Layer" << YAML::Value << PhysicsLayerToString(body.Layer);
	}

	void ComponentSerializers::LoadCharacterBody(const YAML::Node& characterNode, CharacterBodyComponent& body)
	{
		body.Radius = AtLeast(0.05f, characterNode["Radius"].as<float>(body.Radius));
		body.Height = AtLeast(0.2f, characterNode["Height"].as<float>(body.Height));
		body.MaxSlopeDegrees = std::clamp(characterNode["MaxSlopeDegrees"].as<float>(body.MaxSlopeDegrees), 0.0f, 89.0f);
		body.StepHeight = AtLeast(0.0f, characterNode["StepHeight"].as<float>(body.StepHeight));
		body.StaysOnLedges = characterNode["StaysOnLedges"].as<bool>(body.StaysOnLedges);
		body.Layer = PhysicsLayerFromString(characterNode["Layer"].as<std::string>("Player"), body.Layer);
	}
}
