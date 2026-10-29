#include "hypch.h"
#include "SceneHierarchy.h"

#include <algorithm>
#include <unordered_map>

namespace Hydro
{
	// UUID -> index in `nodes`. The first entry keeps a UUID if two share one (a scene never lets that happen).
	static std::unordered_map<UUID, size_t> MakeIndexMap(const std::vector<SceneHierarchy::Node>& nodes)
	{
		std::unordered_map<UUID, size_t> indices;
		indices.reserve(nodes.size());
		for (size_t i = 0; i < nodes.size(); i++)
		{
			indices.emplace(nodes[i].Id, i);
		}
		return indices;
	}

	void SceneHierarchy::Build(const std::vector<Node>& nodes)
	{
		const size_t count = nodes.size();
		m_ParentIndex.assign(count, -1);
		m_Depth.assign(count, 0);
		m_SubtreeEnd.resize(count);

		const std::unordered_map<UUID, size_t> indices = MakeIndexMap(nodes);
		for (size_t i = 0; i < count; i++)
		{
			m_SubtreeEnd[i] = i + 1;
			if ((uint64_t)nodes[i].Parent == 0 || nodes[i].Parent == nodes[i].Id)
			{
				continue;
			}

			const auto found = indices.find(nodes[i].Parent);
			if (found != indices.end() && found->second < i)
			{
				m_ParentIndex[i] = (int32_t)found->second;
				m_Depth[i] = m_Depth[found->second] + 1;
			}
		}

		// Children before parents, so a parent's end covers its children's ends.
		for (size_t i = count; i-- > 0;)
		{
			const int32_t parent = m_ParentIndex[i];
			if (parent >= 0)
			{
				m_SubtreeEnd[parent] = std::max(m_SubtreeEnd[parent], m_SubtreeEnd[i]);
			}
		}
	}

	void SceneHierarchy::GetChildIndices(size_t index, std::vector<size_t>& children) const
	{
		children.clear();
		for (size_t i = index + 1; i < m_SubtreeEnd[index]; i++)
		{
			if (m_ParentIndex[i] == (int32_t)index)
			{
				children.push_back(i);
			}
		}
	}

	size_t SceneHierarchy::GetLastChildInsertIndex(int32_t parent) const
	{
		return parent >= 0 ? m_SubtreeEnd[parent] : GetSize();
	}

	size_t SceneHierarchy::GetSiblingInsertIndex(size_t target, bool placeAfter) const
	{
		return placeAfter ? m_SubtreeEnd[target] : target;
	}

	bool SceneHierarchy::Validate(const std::vector<Node>& nodes, std::vector<Issue>* issues)
	{
		const std::unordered_map<UUID, size_t> indices = MakeIndexMap(nodes);

		// The open ancestors of the entry being looked at: in a depth-first list, an entry's parent must be one
		// of them.
		std::vector<size_t> open;
		bool valid = true;
		auto report = [&](size_t index, Problem problem)
		{
			valid = false;
			if (issues != nullptr)
			{
				issues->push_back({ index, problem });
			}
			// Carry on as if it were a root, so one mistake is reported once.
			open.clear();
			open.push_back(index);
		};

		for (size_t i = 0; i < nodes.size(); i++)
		{
			const Node& node = nodes[i];
			if ((uint64_t)node.Parent == 0)
			{
				open.clear();
				open.push_back(i);
				continue;
			}
			if (node.Parent == node.Id)
			{
				report(i, Problem::OwnParent);
				continue;
			}

			const auto found = indices.find(node.Parent);
			if (found == indices.end())
			{
				report(i, Problem::MissingParent);
				continue;
			}
			if (found->second > i)
			{
				report(i, Problem::ParentAfterChild);
				continue;
			}

			while (open.empty() == false && open.back() != found->second)
			{
				open.pop_back();
			}
			if (open.empty())
			{
				report(i, Problem::SplitSubtree);
				continue;
			}
			open.push_back(i);
		}

		return valid;
	}

	std::vector<size_t> SceneHierarchy::Repair(std::vector<Node>& nodes, std::vector<Issue>& fixed)
	{
		fixed.clear();
		const size_t count = nodes.size();
		const std::unordered_map<UUID, size_t> indices = MakeIndexMap(nodes);

		// 1. Parents that can't be: missing, or the entity itself.
		for (size_t i = 0; i < count; i++)
		{
			Node& node = nodes[i];
			if ((uint64_t)node.Parent == 0)
			{
				continue;
			}
			if (node.Parent == node.Id)
			{
				node.Parent = 0;
				fixed.push_back({ i, Problem::OwnParent });
			}
			else if (indices.find(node.Parent) == indices.end())
			{
				node.Parent = 0;
				fixed.push_back({ i, Problem::MissingParent });
			}
		}

		// 2. Cycles: walk up from every entry. Coming back to an entry of the same walk is a cycle; the member
		// listed first becomes a root and the walk starts again. Entries already known to reach a root are
		// not walked twice.
		std::vector<uint8_t> reachesRoot(count, 0);
		std::vector<uint8_t> onPath(count, 0);
		std::vector<size_t> path;
		for (size_t i = 0; i < count; i++)
		{
			bool walking = true;
			while (walking)
			{
				path.clear();
				size_t current = i;
				bool cycle = false;
				for (;;)
				{
					if (reachesRoot[current] != 0)
					{
						break;
					}
					if (onPath[current] != 0)
					{
						const auto start = std::find(path.begin(), path.end(), current);
						const size_t first = *std::min_element(start, path.end());
						nodes[first].Parent = 0;
						fixed.push_back({ first, Problem::Cycle });
						cycle = true;
						break;
					}

					onPath[current] = 1;
					path.push_back(current);
					if ((uint64_t)nodes[current].Parent == 0)
					{
						break;
					}
					current = indices.at(nodes[current].Parent);
				}

				for (size_t entry : path)
				{
					onPath[entry] = 0;
					if (cycle == false)
					{
						reachesRoot[entry] = 1;
					}
				}
				walking = cycle;
			}
		}

		// 3. Stable depth-first order: roots, and each entry's children, in the order they were listed.
		std::vector<std::vector<size_t>> children(count);
		std::vector<size_t> roots;
		for (size_t i = 0; i < count; i++)
		{
			if ((uint64_t)nodes[i].Parent == 0)
			{
				roots.push_back(i);
			}
			else
			{
				children[indices.at(nodes[i].Parent)].push_back(i);
			}
		}

		std::vector<size_t> order;
		order.reserve(count);
		std::vector<size_t> stack(roots.rbegin(), roots.rend());
		while (stack.empty() == false)
		{
			const size_t entry = stack.back();
			stack.pop_back();
			order.push_back(entry);
			stack.insert(stack.end(), children[entry].rbegin(), children[entry].rend());
		}

		return order;
	}

	const char* SceneHierarchy::ProblemToString(Problem problem)
	{
		switch (problem)
		{
		case Problem::MissingParent: return "its parent isn't in the scene";
		case Problem::OwnParent: return "it is its own parent";
		case Problem::Cycle: return "its parents form a cycle";
		case Problem::ParentAfterChild: return "its parent is listed after it";
		case Problem::SplitSubtree: return "its parent's subtree is interrupted before it";
		}
		return "unknown problem";
	}
}
