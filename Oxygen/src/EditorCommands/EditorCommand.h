#pragma once
#include <Hydro/Core/Base.h>

#include <cstddef>
#include <string>
#include <vector>

namespace Hydro
{
	// One undoable edit (#165), pushed after it was applied. It keeps before and after values and refers to what it
	// changed by something stable (an asset handle, an entity UUID), never by a pointer.
	class EditorCommand
	{
	public:
		virtual ~EditorCommand() = default;

		virtual void Undo() = 0;
		virtual void Redo() = 0;
		// For the Edit menu: "Paint Terrain".
		virtual const char* GetName() const = 0;
	};

	// The undo history: pushing drops the redo list; keeps at most MaxCommands. Owned by EditorLayer, cleared when
	// another scene or project opens.
	class EditorCommandStack
	{
	public:
		static constexpr size_t MaxCommands = 100;

		void Push(Scope<EditorCommand> command);
		// False when there was nothing to undo / redo.
		bool Undo();
		bool Redo();
		void Clear();

		bool CanUndo() const { return m_Done.empty() == false; }
		bool CanRedo() const { return m_Undone.empty() == false; }
		// The name of what Undo / Redo would do; empty when nothing.
		std::string GetUndoName() const;
		std::string GetRedoName() const;

	private:
		std::vector<Scope<EditorCommand>> m_Done;   // oldest first
		std::vector<Scope<EditorCommand>> m_Undone; // most recently undone last
	};
}
