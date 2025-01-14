<img src="Resources/Icon128.png" align="right" width="110">

# Advanced Locomotion System Refactored (Game Feature Plugin)

Game Feature Plugin version of [Sixze's Refactored Advanced Locomotion System V4](https://github.com/Sixze/ALS-Refactored).

The main goal of this plugin is to build on top of ALS-Refactored to allow it to be easily implemented into a Lyra Framework project, while providing minimal changes to Lyra and ALS-Refactored, to ensure modularity and future compatibility.

<details>

<summary><b>Features</b></summary>

- Reworked game feature plugin structure. Added module `ALSLyra` which contains all of the added content.
- Reworked `ALSCharacter` and `ALSCharacterMovementComponent` to extend `LyraCharacter` and `LyraCharacterMovementComponent`, respectively.
- Retargeted all ALS animations to the Unreal 5 Mannequin.
- Implemented gameplay ability system for all ALS abilities.
- Added new ALS experience which implements new ALS features through Lyra's modular experience system.
- Converted UE5Manny ALS animations to use Lyra's context effects system.

</details>

**The plugin is developed and tested primarily on Windows, so use it on other platforms at your own risk.**

## Quick Start

1. Clone the repository to your project's `Plugins/GameFeatures` folder, or download the latest release and extract it to your project's `Plugins/GameFeatures` folder.
2. Merge the `DefaultEngine.ini` and `DefaultInput.ini` files from the plugin's `Config` folder with files from your project's `Config` folder.
3. Recompile your project.

## Known Issues & Discussion

- See the [Issues](https://github.com/corbin-poteet/ALS-Refactored-Lyra/issues) page for list of known issues.
- See the [Discussions](https://github.com/corbin-poteet/ALS-Refactored-Lyra/discussions) page to discuss anything about the plugin and ask questions. Please do not open an issue for questions.

## License & Contribution

ALS Refactored Lyra is licensed under the MIT License, see [LICENSE.md](LICENSE.md) for more information. Other developers are encouraged to fork the repository, open issues & pull requests to help the development.
