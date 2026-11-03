#!/usr/bin/env python3
"""The engine's code conventions, checked (the hydro-engine-architecture skill has the rules).

    python scripts/lint_conventions.py [--warnings] [files or folders...]   # default: Hydro/src, Oxygen/src, Game/src, Runtime/src

Reports file:line: rule: message for every break and exits 1 when there is one. Rules:
  not-operator  no `!` operator: write `== false` / `== nullptr` (`!=` is fine)
  braces        braces on every if / else / for / while / do body
  namespace     everything in namespace Hydro: no other, nested or anonymous namespaces (`namespace std` and
                `namespace YAML` are allowed for specialisations of their templates; aliases and `using namespace` too)
  member-name   m_ / s_ names are m_PascalCase / s_PascalCase (not m_camelCase)
  area-include  a file in Game/src/Scripts/<Area>/ includes nothing from another area (Scripts/Common/, Field/ and
                Battle/ are shared: the field's and the battle's code, used by every area)

Comments, string and character literals and preprocessor lines are ignored by those rules. Skipped: vendor folders,
generated files (Generated/) and the copied ImGui backends (Platform/Vulkan/imgui_impl_*). Python 3.9+, no packages.

Warnings (counted, listed with --warnings, never fail the run; docs/Architecture.md, Code conventions, Comments):
  comment-date    a date in a comment ("Nov 3, 2026"): history belongs in git
  comment-plan    a plan or step ID in a comment (SC4, M6, TR1): meaningless without the wiki; #123 issues are fine
  comment-length  more than two comment lines in a row: say it in a name, or move it to docs/
  line-length     a line over 140 columns (tabs count as 4)
  file-length     a file over 1000 lines: it probably has more than one responsibility, split it
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ROOTS = [ROOT / "Hydro" / "src", ROOT / "Oxygen" / "src", ROOT / "Game" / "src", ROOT / "Runtime" / "src"]
SUFFIXES = {".h", ".hpp", ".cpp"}
ALLOWED_NAMESPACES = {"Hydro", "std", "YAML"}
# Game/src/Scripts/ folders every area may include (the rest are areas: one scene or a few of one kind).
SHARED_SCRIPT_FOLDERS = {"Common", "Field", "Battle"}
MAX_COMMENT_LINES = 2
MAX_LINE_LENGTH = 140
MAX_FILE_LINES = 1000
DATE = re.compile(r"\b(?:Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)[a-z]* \d{1,2}(?:, \d{4})?\b|\b20\d\d-\d\d-\d\d\b")
PLAN_ID = re.compile(r"\b(?:SC|AN|TR|WC|UI|M|P|R|H|D|V|G|E|A|S|C)\d{1,2}[a-z]?\b")


def skipped(path: Path) -> bool:
    parts = path.parts
    return "vendor" in parts or "Generated" in parts or path.name.startswith("imgui_impl_")


def blank_out(text: str) -> str:
    """The text with comments and the insides of string / character literals replaced by spaces (newlines kept), so
    offsets and line numbers stay the same."""
    out: list[str] = []
    i, n = 0, len(text)
    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", text[i:j]))
            i = j
        elif text[i] in "\"'":
            quote = text[i]
            raw = re.match(r'R"([^(\s]{0,16})\(', text[i - 1:i + 20]) if quote == '"' and i > 0 and text[i - 1] == "R" else None
            if raw is not None:
                close = ")" + raw.group(1) + '"'
                end = text.find(close, i)
                j = n if end < 0 else end + len(close)
            else:
                j = i + 1
                while j < n and text[j] != quote and text[j] != "\n":
                    j += 2 if text[j] == "\\" else 1
                j = min(j + 1, n)
            out.append(quote + re.sub(r"[^\n]", " ", text[i + 1:j - 1]) + (text[j - 1] if j - 1 > i else ""))
            i = j
        else:
            out.append(text[i])
            i += 1
    return "".join(out)


def blank_preprocessor(text: str) -> str:
    """Preprocessor lines (and their continuations) blanked, except a #define's body, which is code."""
    result: list[str] = []
    mode = None  # None, "define" (keep: a macro body) or "other" (blank) while a directive continues
    for line in text.split("\n"):
        if mode is None and line.lstrip().startswith("#"):
            define = re.match(r"\s*#\s*define\s+\w+(\([^)]*\))?", line)
            if define is not None:
                result.append(" " * define.end() + line[define.end():])
                mode = "define"
            else:
                result.append(" " * len(line))
                mode = "other"
        elif mode == "other":
            result.append(" " * len(line))
        else:
            result.append(line)
        if mode is not None and line.rstrip().endswith("\\") is False:
            mode = None
    return "\n".join(result)


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def skip_space(code: str, i: int) -> int:
    """Past whitespace and line continuations."""
    while i < len(code):
        if code[i].isspace():
            i += 1
        elif code.startswith("\\\n", i):
            i += 2
        else:
            break
    return i


def matching_paren(code: str, i: int) -> int:
    depth = 0
    while i < len(code):
        if code[i] == "(":
            depth += 1
        elif code[i] == ")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return len(code) - 1


def check_braces(code: str, report) -> None:
    for match in re.finditer(r"\b(if|for|while|switch|else|do)\b", code):
        keyword = match.group(1)
        i = skip_space(code, match.end())
        if keyword == "else":
            if code.startswith("{", i) or re.match(r"if\b", code[i:]):
                continue
            report(match.start(), "braces", "the else body needs braces")
            continue
        if keyword == "do":
            if not code.startswith("{", i):
                report(match.start(), "braces", "the do body needs braces")
            continue
        constexpr = re.match(r"constexpr\b", code[i:])
        if constexpr is not None:
            i = skip_space(code, i + constexpr.end())
        if not code.startswith("(", i):
            continue
        after = skip_space(code, matching_paren(code, i) + 1)
        if code.startswith("{", after):
            continue
        if keyword == "while" and code.startswith(";", after):
            continue  # the end of do { } while (...);
        if keyword == "switch":
            continue
        report(match.start(), "braces", f"the {keyword} body needs braces")


def check_comments(text: str, warn) -> None:
    """The comment, line-length and file-length warnings, per line of the original text."""
    lines = text.split("\n")
    if len(lines) > MAX_FILE_LINES:
        warn(1, "file-length", f"{len(lines)} lines (soft limit {MAX_FILE_LINES}): split it by responsibility")
    run = 0
    for number, line in enumerate(lines, start=1):
        if len(line.expandtabs(4)) > MAX_LINE_LENGTH:
            warn(number, "line-length", f"{len(line.expandtabs(4))} columns (soft limit {MAX_LINE_LENGTH})")
        comment = line.find("//")
        stripped = line.strip()
        if stripped.startswith("//"):
            run += 1
            if run == MAX_COMMENT_LINES + 1:
                warn(number - MAX_COMMENT_LINES, "comment-length", f"more than {MAX_COMMENT_LINES} comment lines in a row")
        else:
            run = 0
        if comment >= 0 and '"' not in line[:comment]:
            note = line[comment:]
            if DATE.search(note):
                warn(number, "comment-date", "a date in a comment: git has the history")
            elif PLAN_ID.search(note):
                warn(number, "comment-plan", f"{PLAN_ID.search(note).group(0)}: a plan ID in a comment (keep only #issue numbers)")


def check_file(path: Path, report_line, warn_line=None) -> None:
    text = path.read_text(encoding="utf-8", errors="replace").replace("\r\n", "\n")
    code = blank_preprocessor(blank_out(text))
    if warn_line is not None:
        check_comments(text, lambda line, rule, message: warn_line(path, line, rule, message))

    def report(offset: int, rule: str, message: str) -> None:
        report_line(path, line_of(code, offset), rule, message)

    for match in re.finditer(r"!(?!=)", code):
        report(match.start(), "not-operator", "write `== false` / `== nullptr` instead of `!`")

    check_braces(code, report)

    for match in re.finditer(r"\bnamespace\b(\s*[\w:]*)\s*(=|\{)?", code):
        before = code[max(0, match.start() - 20):match.start()]
        if re.search(r"\busing\s*$", before):
            continue
        name = match.group(1).strip()
        if match.group(2) == "=":
            continue  # an alias
        if name == "":
            report(match.start(), "namespace", "no anonymous namespaces: use static functions in namespace Hydro")
        elif "::" in name or name not in ALLOWED_NAMESPACES:
            report(match.start(), "namespace", f"namespace {name}: everything goes in namespace Hydro (a class of static functions instead)")

    for match in re.finditer(r"\b([ms])_([a-z]\w*)", code):
        report(match.start(), "member-name", f"{match.group(0)}: {match.group(1)}_ names are PascalCase ({match.group(1)}_{match.group(2)[0].upper()}{match.group(2)[1:]})")

    parts = path.resolve().parts
    if "Scripts" in parts:
        index = parts.index("Scripts")
        if index + 2 < len(parts):  # Scripts/<Area>/file
            area = parts[index + 1]
            if area not in SHARED_SCRIPT_FOLDERS:
                for include in re.finditer(r'^\s*#\s*include\s*"([^"]+)"', text, flags=re.M):
                    other = re.match(r"(?:.*/)?Scripts/([^/]+)/", include.group(1))
                    if other is not None and other.group(1) != area and other.group(1) not in SHARED_SCRIPT_FOLDERS:
                        report_line(path, line_of(text, include.start()), "area-include",
                                    f"Scripts/{area}/ includes Scripts/{other.group(1)}/: share it through State/, Rules/, Views/ or Scripts/Common/")


def iter_files(arguments: list[str]):
    roots = [Path(a) for a in arguments] if arguments else DEFAULT_ROOTS
    for root in roots:
        root = root if root.is_absolute() else (Path.cwd() / root)
        if root.is_file():
            if root.suffix in SUFFIXES:
                yield root
            continue
        for path in sorted(root.rglob("*")):
            if path.suffix in SUFFIXES and not skipped(path.relative_to(root)):
                yield path


def shown_path(path: Path) -> str:
    try:
        return path.resolve().relative_to(ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def main() -> int:
    arguments = [a for a in sys.argv[1:] if a != "--warnings"]
    list_warnings = "--warnings" in sys.argv[1:]
    problems = 0
    warnings: dict[str, int] = {}

    def report_line(path: Path, line: int, rule: str, message: str) -> None:
        nonlocal problems
        problems += 1
        print(f"{shown_path(path)}:{line}: {rule}: {message}")

    def warn_line(path: Path, line: int, rule: str, message: str) -> None:
        warnings[rule] = warnings.get(rule, 0) + 1
        if list_warnings:
            print(f"{shown_path(path)}:{line}: warning: {rule}: {message}")

    count = 0
    for path in iter_files(arguments):
        count += 1
        check_file(path, report_line, warn_line)
    summary = ", ".join(f"{n} {rule}" for rule, n in sorted(warnings.items()))
    print(f"Conventions: {count} file(s), {problems} problem(s)" + (f"; warnings: {summary} (--warnings lists them)" if summary else ""))
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
