# M1 parity checklist

`AGENTS.md` §8 says M1 is done when every `Command` and every property-browser (text-window)
screen has a new home; then the old GTK/Win32/Cocoa/Qt frontends go. This list is kept true by
`test/ngui/parity/test.cpp`, which asks for every screen the way commands do and walks the
whole NG menu bar.

## Property-browser screens

| Screen | New home | Opened by |
|---|---|---|
| Home (list of groups, show / hide / activate) | **Outliner** (`O`) | always shown; badges open Problems |
| Group info (all group settings, its requests and constraints) | **Properties** (`N`) | Outliner click, context menu Group Info |
| Selection description (entities, measurements, constraint values) | **Properties** | selecting anything |
| Group solve info (failed / redundant constraints) | **Problems** (`Shift+P`) | opens itself on a new problem; Outliner badge |
| Configuration | **Preferences** | Edit ▸ Configuration (`Ctrl+,`) |
| List of styles, style info | **Styles** | Edit ▸ Line Styles, a selection's style link, context menu |
| View (scale, origin, projection, lighting, explode) | **View** | Edit ▸ View Projection |
| Paste transformed | **Paste Transformed** | Edit ▸ Paste Transformed (`Ctrl+Shift+V`) |
| Step dimension | **Step Dimension** | Analyze ▸ Step Dimension |
| Tangent arc settings | **Tangent Arc** | Sketch ▸ Tangent Arc at Point with nothing selected |

The old browser starts hidden and is never forced open; View ▸ Show Property Browser
(`Shift+N`) still shows it until it is deleted.

## Commands

Every command in `GraphicsWindow::MenuCommands()` is in the NG menu bar, and therefore in the
command palette (`Space`), which is built from the menu bar. The Modern keymap binds the
frequent ones (`res/keymaps/modern.keymap`); the Classic preset binds every SolveSpace
accelerator.

## Left before the old frontends can go (M1c task 10, owner's go-ahead)

- [ ] The owner's hands-on check of M1a–M1c (the UI tests can't judge feel).
- [ ] Decide what replaces the left toolbar (`src/toolbar.cpp`): today the palette and the
      Add / Constrain pies cover it.
- [ ] Constraint glyph restyle and Lucide icons (roadmap item; adds lunasvg).
- [ ] Then delete `guigtk.cpp`, `guiwin.cpp`, `guimac.mm`, `guiqt.*`, their CMake branches and
      the floating property-browser panel.
