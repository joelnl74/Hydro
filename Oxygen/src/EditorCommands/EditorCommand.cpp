#include "EditorCommand.h"

namespace Hydro
{
	void EditorCommandStack::Push(Scope<EditorCommand> command)
	{
		if (command == nullptr)
		{
			return;
		}
		m_Undone.clear();
		m_Done.push_back(std::move(command));
		if (m_Done.size() > MaxCommands)
		{
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
		return true;
	}

	void EditorCommandStack::Clear()
	{
		m_Done.clear();
		m_Undone.clear();
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
