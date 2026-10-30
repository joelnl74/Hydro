#pragma once
#include <Hydro/Core/Base.h>

#include <cstddef>
#include <string>
#include <vector>

namespace Hydro
{
	// One undoable edit in the editor (issue #165, started by the Terrain plan's TR1). Recorded after it happened:
	// the edit is already applied when the command is pushed, so Redo puts it back after an Undo.
	//
	//   commands.Push(CreateScope<TerrainStrokeCommand>(...)); // after the stroke changed the terrain
	//   commands.Undo();                                        // Ctrl+Z
	//   commands.Redo();                                        // Ctrl+Y / Ctrl+Shift+Z
	//
	// A command keeps what it needs to apply itself either way (before and after values), and refers to what it
	// changed by something stable (an asset handle, an entity UUID), never by a pointer that a reload or a new scene
	// would leave dangling.
	class EditorCommand
	{
	public:
		virtual ~EditorCommand() = default;

		virtual void Undo() = 0;
		virtual void Redo() = 0;
		// For the Edit menu: "Paint Terrain".
		virtual const char* GetName() const = 0;
	};

	// The editor's undo history: commands done, and those undone that can be redone. Pushing a new command drops the
	// redo list. Keeps at most MaxCommands; the oldest go first. Owned by EditorLayer; cleared when another scene or
	// project is opened.
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
