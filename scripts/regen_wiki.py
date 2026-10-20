#!/usr/bin/env python3
"""Regenerate the wiki pages that mirror docs/ (docs/ is the source of truth).

Each mirrored page is: the doc's title line, a blank line, the page's existing
"> Mirrors ..." banner (kept as it is), then the rest of the doc with links to
other docs rewritten to wiki page names (ResourceCreation.md -> Resource-Creation).
Pages that live only in the wiki (Components, Roadmap, Known-Issues, ...) are not touched.

Usage:
    python scripts/regen_wiki.py [--wiki PATH] [--check]

    --wiki   the wiki checkout (default: ../Hydro.wiki next to the repository)
    --check  don't write; list pages that are out of date and exit 1 if any are

Files are written with CRLF line endings, like the rest of the repository.
"""
import argparse
import pathlib
import sys

# docs/<file> -> wiki page file
PAGES = {
    "Architecture.md": "Architecture.md",
    "ResourceCreation.md": "Resource-Creation.md",
    "SceneAndECS.md": "Scene-and-ECS.md",
    "Subsystems.md": "Subsystems.md",
}
# Link targets inside docs -> wiki page names (anchors after them are kept).
LINKS = {doc: wiki[: -len(".md")] for doc, wiki in PAGES.items()}


def read_lf(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8").replace("\r\n", "\n")


GITHUB_DOCS = "https://github.com/joelnl74/Hydro/blob/main/docs/"


def render(doc_text: str, old_wiki_text: str, wiki_name: str, other_docs: list) -> str:
    old_lines = old_wiki_text.split("\n")
    banner = old_lines[2] if len(old_lines) > 2 and old_lines[2].startswith(">") else (
        f"> Mirrors [`docs/{wiki_name}`](https://github.com/joelnl74/Hydro/blob/main/docs/{wiki_name}) "
        "in the repository, which is the source of truth.")
    lines = doc_text.split("\n")
    body = "\n".join(lines[1:])
    for doc, page in LINKS.items():
        body = body.replace("(" + doc, "(" + page)
    # Docs without a wiki page (e.g. CodeMap.md) are linked on GitHub.
    for doc in other_docs:
        body = body.replace("(" + doc, "(" + GITHUB_DOCS + doc)
    return lines[0] + "\n\n" + banner + "\n" + body


def main() -> int:
    root = pathlib.Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--wiki", type=pathlib.Path, default=root.parent / "Hydro.wiki")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    if args.wiki.is_dir() is False:
        print(f"Wiki checkout not found: {args.wiki} (pass --wiki PATH)")
        return 2

    other_docs = sorted(p.name for p in (root / "docs").glob("*.md") if p.name not in PAGES)

    stale = []
    for doc, wiki in PAGES.items():
        doc_path = root / "docs" / doc
        wiki_path = args.wiki / wiki
        old = read_lf(wiki_path) if wiki_path.exists() else ""
        new = render(read_lf(doc_path), old, doc, other_docs)
        if new == old:
            print(f"up to date  {wiki}")
            continue
        stale.append(wiki)
        if args.check:
            print(f"OUT OF DATE {wiki}")
        else:
            wiki_path.write_bytes(new.replace("\n", "\r\n").encode("utf-8"))
            print(f"written     {wiki}")

    return 1 if (args.check and stale) else 0


if __name__ == "__main__":
    sys.exit(main())
