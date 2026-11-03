#pragma once
#include "Hydro/Core/UUID.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Hydro
{
	// The parent / child layout of a scene's entity list, from (UUID, parent UUID) pairs in list order; never touches
	// the registry. The list must be the tree in depth-first pre-order (docs/SceneAndECS.md#hierarchy).
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
		// Entries whose parent is missing, itself or listed after it count as roots, so any list can be laid out.
		void Build(const std::vector<Node>& nodes);

		size_t GetSize() const { return m_ParentIndex.size(); }
		// -1 for a root.
		int32_t GetParentIndex(size_t index) const { return m_ParentIndex[index]; }
		// 0 for a root.
		uint32_t GetDepth(size_t index) const { return m_Depth[index]; }
		// One past the entry's last descendant (index + 1 without children): its subtree is [index, end).
		size_t GetSubtreeEnd(size_t index) const { return m_SubtreeEnd[index]; }
		bool HasChildren(size_t index) const { return m_SubtreeEnd[index] > index + 1; }
		// False when they are the same entry.
		bool IsAncestor(size_t ancestor, size_t index) const { return index > ancestor && index < m_SubtreeEnd[ancestor]; }
		// Direct children in list order; clears `children` first.
		void GetChildIndices(size_t index, std::vector<size_t>& children) const;

		// Where a moved subtree goes, as an index from before the move (MoveRange): the last child of `parent`
		// (-1: the last root) ...
		size_t GetLastChildInsertIndex(int32_t parent) const;
		// ... or directly before `target`, or after target's subtree.
		size_t GetSiblingInsertIndex(size_t target, bool placeAfter) const;

		// ---- Rules ----------------------------------------------------------------------------------
		// False with every offending entry in `issues` (when given). A cycle shows up as ParentAfterChild here.
		static bool Validate(const std::vector<Node>& nodes, std::vector<Issue>* issues = nullptr);

		// Missing parents, self parents and the first entity of each cycle become roots (reported in `fixed`). Returns
		// a depth-first order as indices into `nodes`, keeping the listed order where possible; `nodes` isn't reordered.
		static std::vector<size_t> Repair(std::vector<Node>& nodes, std::vector<Issue>& fixed);

		static const char* ProblemToString(Problem problem);

		// Moves list[begin, end) to where index `insertAt` (from before the move) was; inside [begin, end] does nothing.
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
