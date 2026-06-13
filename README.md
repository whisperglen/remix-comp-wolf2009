<h1 align="center">RTX Remix Compatibility Codebase</h1>

<br>

# Overview
My own fork of xoxor4d's Remix Compatibility Codebase: https://github.com/xoxor4d/remix-comp-base

## Documentation / Guides

Please see: https://github.com/xoxor4d/remix-comp-base/tree/master/documentation

<br>

## Compiling
- Clone the repository `git clone --recurse-submodules https://github.com/xoxor4d/remix-comp-base.git`
- Optional: Setup a global path variable named `REMIX_COMP_ROOT` that points to your game folder
  & `REMIX_COMP_ROOT_EXE` which includes the exe name of your game.
  - Or modify `premake5.lua` to make it fit your needs
- Run `generate-buildfiles_vs22.bat` to generate VS project files
- Compile the mod

- Copy everything inside the `assets` folder into the game directory.  
  You may need to rename the Ultimate ASI Loader file if your game does not import `dinput8.dll`.

> [!TIP]  
> Determining which DLLs your game imports on startup is fairly straightforward, but I won’t go into detail here.  
> I recommend using [Explorer Suite by NTCore](https://ntcore.com/explorer-suite/).

- If you did not setup the global path variable:  
  Move the `asi` file into a folder called `plugins` inside your game directory.

<br>

##  Credits
- [NVIDIA - RTX Remix](https://github.com/NVIDIAGameWorks/rtx-remix)
- [People of the showcase discord](https://discord.gg/j6sh7JD3v9) - especially the nvidia engineers ✌️
- [Dear ImGui](https://github.com/ocornut/imgui)
- [minhook](https://github.com/TsudaKageyu/minhook)
- [Ultimate-ASI-Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)
- [momo5502](https://github.com/momo5502) - initial codebase and senpai back in the day ✌️
- All 🍓 Testers

<div align="center" markdown="1"> 

And of course, all my fellow Ko-Fi and Patreon supporters  
and all the people that helped along the way!

</div>
