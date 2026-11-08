"""A workspace's description (hydro-workspace.json) for the engine's tools.

A workspace is the build environment of one game (docs/Architecture.md, "Workspaces"): the engine repository itself
(its game is the Sample), or a game's own repository next to the engine. The tools find it, in this order: an explicit
--workspace <dir>, the HYDRO_WORKSPACE environment variable, else the engine repository.

    python scripts/hydro_workspace.py [workspace dir] --get GameSource|GameDirectory|DefaultProject|ProjectFolder|GameId|Target
"""

from __future__ import annotations

import json
import os
import sys
from dataclasses import dataclass
from pathlib import Path

ENGINE_ROOT = Path(__file__).resolve().parent.parent


@dataclass
class Workspace:
    root: Path
    game_id: str
    game_name: str
    target: str
    game_directory: Path
    default_project: Path | None

    @property
    def game_source(self) -> Path:
        """<CodeDirectory>/src: the game's code and its Generated/ reflection."""
        return self.game_directory / "src"

    @property
    def project_folder(self) -> Path | None:
        """The folder of the default project (its .hyproj and Assets/)."""
        return self.default_project.parent if self.default_project is not None else None


def load(directory: str | Path | None = None) -> Workspace:
    """The workspace in `directory`, else HYDRO_WORKSPACE, else the engine repository. Exits with 2 when it has no
    hydro-workspace.json (a setup problem, as the build scripts report it)."""
    if directory is None:
        directory = os.environ.get("HYDRO_WORKSPACE") or ENGINE_ROOT
    root = Path(directory).resolve()
    file = root / "hydro-workspace.json"
    try:
        data = json.loads(file.read_text(encoding="utf-8"))
        game = data["Game"]
        default = data.get("DefaultProject")
        return Workspace(
            root=root,
            game_id=game["Id"],
            game_name=game["Name"],
            target=game["Target"],
            game_directory=root / game["CodeDirectory"],
            default_project=(root / default) if default else None,
        )
    except (OSError, ValueError, KeyError) as error:
        print(f"hydro_workspace: {file} is missing or not a workspace description ({error})", file=sys.stderr)
        sys.exit(2)


def take_workspace_argument(arguments: list[str]) -> tuple[str | None, list[str]]:
    """Removes `--workspace <dir>` from a tool's arguments: (the directory or None, the other arguments)."""
    rest, directory, index = [], None, 0
    while index < len(arguments):
        if arguments[index] == "--workspace" and index + 1 < len(arguments):
            directory = arguments[index + 1]
            index += 2
            continue
        rest.append(arguments[index])
        index += 1
    return directory, rest


def main() -> int:
    directory, rest = take_workspace_argument(sys.argv[1:])
    if "--get" not in rest:
        print(__doc__.strip())
        return 2
    key = rest[rest.index("--get") + 1] if rest.index("--get") + 1 < len(rest) else ""
    positional = [a for a in rest if a not in ("--get", key)]
    workspace = load(directory or (positional[0] if positional else None))
    values = {
        "GameSource": workspace.game_source,
        "GameDirectory": workspace.game_directory,
        "DefaultProject": workspace.default_project or "",
        "ProjectFolder": workspace.project_folder or "",
        "GameId": workspace.game_id,
        "Target": workspace.target,
        "Root": workspace.root,
    }
    if key not in values:
        print(f"hydro_workspace: --get takes one of {', '.join(values)}", file=sys.stderr)
        return 2
    print(values[key])
    return 0


if __name__ == "__main__":
    sys.exit(main())
