# Running pokeyellow3d on Windows

This package targets Windows 10/11 x64. It includes the single-game launcher,
SDL2, CURL, ANGLE graphics libraries, their dependencies and the MSVC runtime.
Keep the supplied DLLs beside `pokeyellow3d.exe`. No development tools or
vcpkg installation is needed. DEPENDENCIES.txt lists the included binaries
and their SHA-256 hashes; THIRD_PARTY contains dependency copyright notices.

1. Extract the complete ZIP into a writable directory, not Program Files.
2. Create a `roms` folder next to `pokeyellow3d.exe` and copy your own
   matching ROM into it as `roms/pokeyellow.gbc`.
3. Double-click `Start.cmd`. It selects the package directory before launching.
   Alternatively, open PowerShell here and run `./pokeyellow3d.exe`.

The supported ROM is English USA/Europe Yellow, 1 MiB, SHA-1
`cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1`. No ROM, game assets or save
is included. Keep your own ROM private.

On first launch, assets are extracted into `assets/pokeyellow/`. Battery
saves use `pokeyellow.sav` in the same working directory. Esc opens settings,
including the optional local/fixed day-night lighting; its preferences are
stored separately from your cartridge save. See PALLET3D.md and README.md
for controls. F2 switches 2D/3D and F3 switches camera.
