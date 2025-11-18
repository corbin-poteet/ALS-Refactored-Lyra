<img src="Resources/Icon128.png" align="right" width="110">

# Advanced Locomotion System Refactored (Game Feature Plugin)

Game Feature Plugin version of [Sixze's Refactored Advanced Locomotion System V4](https://github.com/Sixze/ALS-Refactored).

The main goal of this plugin is to build on top of ALS-Refactored to allow it to be easily implemented into a Lyra Framework project, while providing minimal changes to Lyra and ALS-Refactored, to ensure modularity and future compatibility.

Currently supports Unreal 5.7

<details>

<summary><b>Features</b></summary>

- Reworked game feature plugin structure. Added module `ALSLyra` which contains all of the added content.
- Reworked `ALSCharacter` and `ALSCharacterMovementComponent` to extend `LyraCharacter` and `LyraCharacterMovementComponent`, respectively.
- Retargeted all ALS animations to the Unreal 5 Mannequin.
- Implemented gameplay ability system for all ALS abilities.
- Added new ALS experience which implements new ALS features through Lyra's modular experience system.
- Converted UE5Manny ALS animations to use Lyra's context effects system.

</details>

## Supported Unreal Engine Versions & Platforms

| Plugin Version                                                            | Unreal Engine Version           |
|---------------------------------------------------------------------------|---------------------------------|
| [4.16](https://github.com/Sixze/ALS-Refactored/releases/tag/4.16)         | 5.6                            |
| [4.15](https://github.com/Sixze/ALS-Refactored/releases/tag/4.15)         | 5.5                             |
| [4.14](https://github.com/Sixze/ALS-Refactored/releases/tag/4.14)         | 5.4                             |
| [4.13](https://github.com/Sixze/ALS-Refactored/releases/tag/4.13)         | 5.3                             |
| [4.12](https://github.com/Sixze/ALS-Refactored/releases/tag/4.12)         | 5.2                             |
| [4.11](https://github.com/Sixze/ALS-Refactored/releases/tag/4.11)         | 5.1                             |
| [4.10](https://github.com/Sixze/ALS-Refactored/releases/tag/4.10) - 4.9   | 5.0                             |
| [4.8](https://github.com/Sixze/ALS-Refactored/releases/tag/4.8) and below | 4.26, 4.27 and 5.0 Early Access |

**The plugin is developed and tested primarily on Windows, so use it on other platforms at your own risk.**

## Quick Start

Install tutorial => https://youtu.be/EQnVff_jGI0?si=-mv1S1w3t_2AMYZF

1. Clone the repository to your project's `Plugins/GameFeatures` folder, or download the latest release and extract it to your project's `Plugins/GameFeatures` folder.
2. Rename the folder to `ALS`.
3. Merge the `DefaultEngine.ini` and `DefaultInput.ini` files from the plugin's `Config` folder with files from your project's `Config` folder.
4. Export `ULyraAnimInstance` by adding `LYRAGAME_API` to `LyraAnimInstance.h`.
    ```
       class LYRAGAME_API ULyraAnimInstance : public UAnimInstance
    ```
5. Recompile your project and start the editor.
6. Navigate to `/All/Game/Characters/Heroes/Mannequin/Meshes` in the content browser. Right-click `SK_Mannequin` -> Scripted Asset Actions -> Setup Als Skeleton.
7. Done.

## Updating From Source

1. Switch to `als-main` branch.
2. Pull from source.
3. Rebase `als-lyra` on top of `als-main`.

## Known Issues & Discussion

- See the [Issues](https://github.com/corbin-poteet/ALS-Refactored-Lyra/issues) page for list of known issues.
- See the [Discussions](https://github.com/corbin-poteet/ALS-Refactored-Lyra/discussions) page to discuss anything about the plugin and ask questions. Please do not open an issue for questions.

## License & Contribution

ALS Refactored Lyra is licensed under the MIT License, see [LICENSE.md](LICENSE.md) for more information. Other developers are encouraged to fork the repository, open issues & pull requests to help the development.
