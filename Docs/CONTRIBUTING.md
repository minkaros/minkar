# Contributing to Minkar

How to help Minkar: where to start, the rules for code and documentation, and how work
reaches `devel`. For anyone about to send a first change. Last reviewed: 2026-11-21.
Spanish copy: [CONTRIBUTING.es.md](CONTRIBUTING.es.md).

## Where to start

1. **Pick an app that does not run yet.** [Compatibility.md](Compatibility.md) lists each
   designated app, how far its published binary gets (`no-load`, `at-start`,
   `no-window`…) and the reason, often the missing library or symbol. That reason is the
   task.
2. **Read a framework's stub log.** Every unimplemented method or function logs its first
   call under the subsystem `org.minkar.stubs`. Run the app in the image, then
   `log show --predicate 'subsystem == "org.minkar.stubs"'`: the calls listed are what the
   app needs, in the order it used them.
3. **Pass the tests before proposing a change.** Build the stage you touched (`bmake
   ONLY_STAGE=<stage> <stage>`), then `bmake layers-check`, gate M2 (`boottest GATE=M2`)
   for anything in the base system, gate M4 for the window server or AppKit, gate BC
   (`bmake bc-check`) for a change aimed at an app, and `bmake progress` with no
   difference. [BUILDING.md](BUILDING.md) says how.

Other ways to help: test on your hardware ([Hardware.md](Hardware.md)), report bugs, and
improve the documentation.

## Code of Conduct

We expect everyone interacting with this project to adhere to the [Contributor Covenant](CODE_OF_CONDUCT.md).

## What is needed

The open work is in [ROADMAP.md](ROADMAP.md) and the [open issues](https://github.com/minkaros/minkar/issues).
The skills needed most: C and C++, Objective-C, Swift, x86-64 assembly, the toolchain and
linker, Darwin kernel and kext programming, and graphics.


## Join the community

Join the [discussions](https://github.com/minkaros/minkar/discussions) to meet other users and developers.

## Reporting Issues

If you have found a security issue, please _do not_ open a GitHub issue. Please email minkaros@outlook.com instead.

For everything else, please first check the [open issues](https://github.com/minkaros/minkar/issues) to make sure it doesn't already exist. If it does, you can add a "me too" comment with additional information or give it a "+1" response to bump the priority. If not, please log a new issue giving as much detail as you can how to reproduce the problem.

## Ground rules

Please don't submit general questions or support requests to the issue tracker. There are [discussion boards](https://github.com/minkaros/minkar/discussions) for that.

Follow the code of conduct.

New code is licensed under the BSD 2-clause license.

If you have _ever_ seen the reference platform's proprietary code (not APSL-licensed or other open-source stuff) then you may *not* contribute code to Minkar. Sorry :( However, you may be able to contribute in other ways!

## Language

Everything committed is in English: code, comments, commit messages and
user interface strings. The interface is translated through `*.lproj`
bundles (English is the development language, and the base for tests and
screenshots); a translation never replaces the English strings in the
code. Documentation is written in English. Only the documents that bring
someone into the project are translated: `README.md`, `Docs/ROADMAP.md`,
`Docs/BUILDING.md`, `Docs/CONTRIBUTING.md` and `Docs/Compatibility.md`, each
with a Spanish copy next to it as `<name>.es.md`. A translation is a copy of
the English document and is updated in the same commit.

## Branches

Work goes to `devel`. `devel` is brought to `main` when a milestone
closes or, at the latest, once a week, and only by fast-forward
(`git push origin devel:main`, no merge commits), so the CI, which runs on
`main`, checks small batches.

## Development shortcuts

A temporary solution taken during development is marked in the code with
the milestone that removes it:

    // SHORTCUT(<removed by>): <what and why>; taken in <milestone>.

For example `SHORTCUT(WS4a)`, `SHORTCUT(Phase 5)` or `SHORTCUT(D3)`. Each
shortcut also has a row in its document's shortcut table
([WindowServer.md](WindowServer.md#development-shortcuts-still-open), sections 3.5 and 4.5 of
[Atlas.md](Atlas.md)). A milestone
closes only when `git grep -n 'SHORTCUT(<milestone>)'` finds nothing.

## Progress pages

`bmake progress` writes every generated page: [Progress.md](Progress.md)'s summary and
SVGs and the README's status (from [progress/status.toml](progress/status.toml)),
[Compatibility.md](Compatibility.md)'s matrix and frameworks table (from
`progress/compatibility.tsv` and `progress/frameworks.tsv`) and
[Userland.md](Userland.md)'s command tables (from `progress/commands.tsv`). Only what lies
between a page's markers is generated. The commit that closes a milestone also updates
`status.toml` and runs `bmake progress`; the work board keeps the six most recent done
items. CI fails when the committed pages are not what `bmake progress` writes.
`status.toml`'s `commit` and `updated` name the tree the pages summarize:
the last commit outside `Docs/` and its date. `bmake progress` warns when
`commit` is older, as the page header would otherwise lag behind.

## Writing documentation

So that the documentation stays short and current:

- A document describes how something is, or what is missing. It does not tell how it got
  there: that is in the commits.
- Each document opens with one or two sentences saying what it is and for whom, and the
  date of its last review.
- A milestone is one line: its name, its state and, when done, its commit; at most 200
  characters, without history.
- No paragraph over 600 characters in the documents for newcomers (README, the index,
  ROADMAP, CONTRIBUTING, BUILDING's quick start).
- Lists for enumerations; short prose to explain a why.
- What a generated page shows (Progress.md, Compatibility.md) is not repeated by hand
  elsewhere.
- How something works today goes to the document of that part; a decision goes to an ADR
  if it is architecture, otherwise to a line in [Decisions.md](Decisions.md).
- Use "API N", "the reference platform", "PC 1" and "PC 2"; no commercial names of the
  reference platform.
- [README.md](README.md) lists every document; a new one gets its line there.

Have fun and do great things!
