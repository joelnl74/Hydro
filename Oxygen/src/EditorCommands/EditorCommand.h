#pragma once
#include <Hydro/Core/Base.h>
#include <Hydro/Core/UUID.h>

#include <cstddef>
#include <cstdint>
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
		// Roughly what it keeps, in bytes, for the history's memory budget.
		virtual size_t GetMemorySize() const = 0;
		// The entity to select after an undo (before) or a redo (after); 0: none. Only used when HasSelection.
		virtual bool HasSelection() const { return false; }
		virtual UUID GetSelection(bool afterEdit) const { (void)afterEdit; return UUID(0); }
	};

	// The undo history: pushing drops the redo list; the oldest steps go once there are more than MaxCommands or they
	// keep more than MaxMemory bytes. Owned by EditorContext, cleared when another scene or project opens.
	class EditorCommandStack
	{
	public:
		static constexpr size_t MaxCommands = 200;
		static constexpr size_t MaxMemory = 256u * 1024u * 1024u;

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
		// The step Undo / Redo would apply, or null.
		const EditorCommand* PeekUndo() const { return m_Done.empty() ? nullptr : m_Done.back().get(); }
		const EditorCommand* PeekRedo() const { return m_Undone.empty() ? nullptr : m_Undone.back().get(); }

		// Counts pushes, undos, redos and clears: watchers resync when it moved.
		uint64_t GetChangeCount() const { return m_ChangeCount; }

		// The scene was saved (or opened) as it is now; IsModified until the history moves away from this point.
		void MarkSaved();
		// Something changed that isn't in the history: modified until the next save.
		void MarkModified() { m_UntrackedChange = true; }
		bool IsModified() const;

		size_t GetMemorySize() const { return m_MemorySize; }

	private:
		void TrimOldest();

	private:
		std::vector<Scope<EditorCommand>> m_Done;   // oldest first
		std::vector<Scope<EditorCommand>> m_Undone; // most recently undone last
		size_t m_MemorySize = 0;
		uint64_t m_ChangeCount = 0;
		// The newest done step when saved (null: none), and whether that point can't come back (dropped or untracked).
		const EditorCommand* m_SavedAt = nullptr;
		bool m_SavePointLost = false;
		bool m_UntrackedChange = false;
	};
}
