#pragma once
#include "Hydro/Core/UUID.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The parent / child structure of a scene's entity list (Roadmap S3), worked out from a plain list of
	// (UUID, parent UUID) pairs in list order. Scene keeps the list (its entity order) and the parents
	// (RelationshipComponent), owns one SceneHierarchy built from them, and maps entities to list indices;
	// everything about the tree itself is here, and none of it touches the registry.
	//
	// The rule (docs/SceneAndECS.md, "Hierarchy"): the list is the tree in depth-first pre-order. A parent comes
	// before its children, and an entity's whole subtree follows it directly, in one piece.
	//
	// An instance is the layout of one list (Build); its queries take list indices. The static functions check a
	// list against the rule and repair one that breaks it.
	class SceneHierarchy
	{
	public:
		struct Node
		{
			UUID Id = 0;
			UUID Parent = 0; // 0 = root
		};

		enum class Problem : uint8_t
		{
			MissingParent = 0, // the parent UUID isn't in the list
			OwnParent,         // the entity is its own parent
			Cycle,             // following the parents comes back round (only Repair reports it)
			ParentAfterChild,  // the parent is listed after the child
			SplitSubtree       // the parent is listed earlier, but its subtree was interrupted before the child
		};

		struct Issue
		{
			size_t Index = 0; // into the list that was checked
			Problem Kind = Problem::MissingParent;
		};

		// ---- Layout of one list ---------------------------------------------------------------------
		// For a list that breaks the rule, an entry whose parent is missing, itself or listed after it counts
		// as a root, so the layout can always be used.
		void Build(const std::vector<Node>& nodes);

		size_t GetSize() const { return m_ParentIndex.size(); }
		// -1 for a root.
		int32_t GetParentIndex(size_t index) const { return m_ParentIndex[index]; }
		// 0 for a root.
		uint32_t GetDepth(size_t index) const { return m_Depth[index]; }
		// One past the entry's last descendant (index + 1 without children): its subtree is [index, end).
		size_t GetSubtreeEnd(size_t index) const { return m_SubtreeEnd[index]; }
		bool HasChildren(size_t index) const { return m_SubtreeEnd[index] > index + 1; }
		// True when `index` is somewhere below `ancestor` (not when they are the same entry).
		bool IsAncestor(size_t ancestor, size_t index) const { return index > ancestor && index < m_SubtreeEnd[ancestor]; }
		// Direct children, in list order (cleared first).
		void GetChildIndices(size_t index, std::vector<size_t>& children) const;

		// Where a moved subtree goes (as an index in the list before the move, for MoveRange):
		// as the last child of `parent` (-1: the last root, at the end of the list) ...
		size_t GetLastChildInsertIndex(int32_t parent) const;
		// ... or directly before `target`, or after target's subtree.
		size_t GetSiblingInsertIndex(size_t target, bool placeAfter) const;

		// ---- Rules ----------------------------------------------------------------------------------
		// True when `nodes` keeps the rule. Otherwise false, with every entry that breaks it in `issues` (when
		// given). A cycle always shows up as ParentAfterChild here.
		static bool Validate(const std::vector<Node>& nodes, std::vector<Issue>* issues = nullptr);

		// Makes `nodes` keep the rule: a missing parent, an entity that is its own parent and one entity of every
		// cycle (the one listed first) become roots (their Parent set to 0, reported in `fixed`). Returns the
		// order that keeps the rule, as indices into `nodes`: a depth-first walk that keeps roots and each
		// entity's children in the order they were listed, so a list that already keeps the rule comes back
		// unchanged. `nodes` itself is not reordered.
		static std::vector<size_t> Repair(std::vector<Node>& nodes, std::vector<Issue>& fixed);

		static const char* ProblemToString(Problem problem);

		// Moves list[begin, end) so it starts where index `insertAt` (in the list before the move) was. An
		// insertAt inside [begin, end] leaves the list as it is.
		template<typename T>
		static void MoveRange(std::vector<T>& list, size_t begin, size_t end, size_t insertAt)
		{
			if (insertAt < begin)
			{
				std::rotate(list.begin() + insertAt, list.begin() + begin, list.begin() + end);
			}
			else if (insertAt > end)
			{
				std::rotate(list.begin() + begin, list.begin() + end, list.begin() + insertAt);
			}
		}

	private:
		std::vector<int32_t> m_ParentIndex;
		std::vector<uint32_t> m_Depth;
		std::vector<size_t> m_SubtreeEnd;
	};
}
