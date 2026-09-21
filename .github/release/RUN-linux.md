# Running pokeyellow3d on Linux

This Linux x86_64 binary is built on Ubuntu 24.04. It requires compatible
system libraries: SDL2, libcurl, OpenGL/GLES2 and a graphics driver.
DEPENDENCIES.txt records the linked libraries. It is a dynamic binary,
not a self-contained AppImage.

1. Extract the complete package into a writable directory.
2. Create a `roms` folder next to `pokeyellow3d` and copy your own matching
   ROM into it as `roms/pokeyellow.gbc`.
3. Open a terminal in this folder and run `./pokeyellow3d`.

The supported ROM is English USA/Europe Yellow, 1 MiB, SHA-1
`cc7d03262ebfaf2f06772c1a480c7d9d5f4a38e1`. No ROM, game assets or save
is included. The executable includes the single-game launcher.

On first launch, assets are extracted into `assets/pokeyellow/`. Battery
saves use `pokeyellow.sav` in the same working directory. Esc opens settings,
including the optional local/fixed day-night lighting; its preferences are
stored separately from your cartridge save. See PALLET3D.md and README.md
for controls. F2 switches 2D/3D and F3 switches camera.
