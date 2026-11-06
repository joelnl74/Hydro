#include "EditorCommand.h"

#include <algorithm>

namespace Hydro
{
	void EditorCommandStack::Push(Scope<EditorCommand> command)
	{
		if (command == nullptr)
		{
			return;
		}
		// The saved point may be among the undone steps: it can't be reached again.
		for (const Scope<EditorCommand>& undone : m_Undone)
		{
			m_SavePointLost = m_SavePointLost || undone.get() == m_SavedAt;
			m_MemorySize -= std::min(m_MemorySize, undone->GetMemorySize());
		}
		m_Undone.clear();
		m_MemorySize += command->GetMemorySize();
		m_Done.push_back(std::move(command));
		TrimOldest();
		m_ChangeCount++;
	}

	void EditorCommandStack::TrimOldest()
	{
		// The newest step always stays, even when it alone is over the budget.
		while (m_Done.size() > 1 && (m_Done.size() > MaxCommands || m_MemorySize > MaxMemory))
		{
			const EditorCommand* oldest = m_Done.front().get();
			m_SavePointLost = m_SavePointLost || oldest == m_SavedAt;
			m_MemorySize -= std::min(m_MemorySize, oldest->GetMemorySize());
			m_Done.erase(m_Done.begin());
		}
	}

	bool EditorCommandStack::Undo()
	{
		if (m_Done.empty())
		{
			return false;
		}
		Scope<EditorCommand> command = std::move(m_Done.back());
		m_Done.pop_back();
		command->Undo();
		m_Undone.push_back(std::move(command));
		m_ChangeCount++;
		return true;
	}

	bool EditorCommandStack::Redo()
	{
		if (m_Undone.empty())
		{
			return false;
		}
		Scope<EditorCommand> command = std::move(m_Undone.back());
		m_Undone.pop_back();
		command->Redo();
		m_Done.push_back(std::move(command));
		m_ChangeCount++;
		return true;
	}

	void EditorCommandStack::Clear()
	{
		m_Done.clear();
		m_Undone.clear();
		m_MemorySize = 0;
		m_SavedAt = nullptr;
		m_SavePointLost = false;
		m_UntrackedChange = false;
		m_ChangeCount++;
	}

	void EditorCommandStack::MarkSaved()
	{
		m_SavedAt = m_Done.empty() ? nullptr : m_Done.back().get();
		m_SavePointLost = false;
		m_UntrackedChange = false;
	}

	bool EditorCommandStack::IsModified() const
	{
		const EditorCommand* newest = m_Done.empty() ? nullptr : m_Done.back().get();
		return m_UntrackedChange || m_SavePointLost || newest != m_SavedAt;
	}

	std::string EditorCommandStack::GetUndoName() const
	{
		return m_Done.empty() ? std::string() : std::string(m_Done.back()->GetName());
	}

	std::string EditorCommandStack::GetRedoName() const
	{
		return m_Undone.empty() ? std::string() : std::string(m_Undone.back()->GetName());
	}
}
