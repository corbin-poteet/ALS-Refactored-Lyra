// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorAnimUtils.h"
#include "Retargeter/IKRetargeter.h"

#include "AlsRetargetBatchOperation.generated.h"

class UIKRetargeter;
struct FScopedSlowTask;
class USkeletalMesh;
class UAnimationAsset;
class UAnimBlueprint;
class UAnimSequence;


struct FAdditiveRetargetSettings
{
	// the asset this operates on
	TObjectPtr<UAnimSequence> SequenceAsset;

	// the settings to save/restore before/after retargeting
	TEnumAsByte<EAdditiveAnimationType> AdditiveAnimType;
	TEnumAsByte<EAdditiveBasePoseType> RefPoseType;
	int32 RefFrameIndex;
	TObjectPtr<UAnimSequence> RefPoseSeq;

	void PrepareForRetarget(UAnimSequence* InSequenceAsset);
	void RestoreOnAsset(const TMap<UAnimationAsset*, UAnimationAsset*>& RemappedAnimAssets) const;
};

/// Data needed to run a batch "duplicate and retarget" operation on a set of animation assets
struct ALSEDITOR_API FAlsRetargetBatchOperationContext
{

public:

	// The source assets to duplicate and retarget
	TArray<TWeakObjectPtr<UObject>> AssetsToRetarget;

	// Source mesh to use to copy animation FROM.
	USkeletalMesh* SourceMesh = nullptr;

	// Target mesh to use to copy animation TO.
	USkeletalMesh* TargetMesh = nullptr;

	// The retargeter used to copy animation
	UIKRetargeter* IKRetargetAsset = nullptr;

	// Rename rules for duplicated assets
	EditorAnimUtils::FNameDuplicationRule NameRule;

	// Any files with the same name will be overwritten instead of creating a new file with a numeric suffix.
	// This is useful when iterating on a batch process.
	bool bUseSourcePath = false;

	// Any files with the same name will be overwritten instead of creating a new file with a numeric suffix.
	// This is useful when iterating on a batch process.
	bool bOverwriteExistingFiles = true;

	// Duplicates and retargets any animation assets referenced by the input assets. For example, sequences in an animation blueprint or blendspace.
	bool bIncludeReferencedAssets = true;

	// Will not produce keys on bones that are not animated, reducing size on disk of the resulting files.
	bool bExportOnlyAnimatedBones = true;

	// Keep the additive animation sequence attributes on the retargeted results.
	// NOTE: results may not be WYSIWYG with the editor, but the behavior should remain intact.
	bool bRetainAdditiveFlags = true;

	// Directory mapping for recreating original file structures in new locations
	// Key: Source directory path (e.g., "/ALS/ALS/Character")
	// Value: Destination directory path (e.g., "/ALS/ALSLyra/Characters/Heroes/Mannequin")
	TMap<FString, FString> DirectoryMappings;

	// Reset all data (called when window re-opened
	void Reset()
	{
		SourceMesh = nullptr;
		TargetMesh = nullptr;
		IKRetargetAsset = nullptr;
		bIncludeReferencedAssets = true;
		NameRule.Prefix = "";
		NameRule.Suffix = "";
		NameRule.ReplaceFrom = "";
		NameRule.ReplaceTo = "";
	}

	// Is the data configured in such a way that we could run the retarget?
	bool IsValid() const
	{
		// todo validate compatibility
		return SourceMesh && TargetMesh && IKRetargetAsset && (SourceMesh != TargetMesh);
	}
};

// Encapsulate ability to batch duplicate and retarget a set of animation assets
UCLASS(BlueprintType)
class ALSEDITOR_API UAlsRetargetBatchOperation : public UObject
{
	GENERATED_BODY()

public:

	/* Convenience function to run a batch animation retarget from Blueprint / Python. This function will duplicate a list of
	 * assets and use the supplied IK Retargeter to retarget the animation from the source to the target using the
	 * settings in the provided IK Retargeter asset.
	 *
	 * @param AssetsToRetarget A list of animation assets to retarget (sequences, blendspaces or montages)
	 * @param SourceMesh The skeletal mesh with desired proportions to playback the assets to retarget
	 * @param TargetMesh The skeletal mesh to retarget the animation onto.
	 * @param IKRetargetAsset The IK Retargeter asset with IK Rigs appropriate for the source and target skeletal mesh
	 * @param Search A string to search for in the file name (replaced with "Replace" string)
	 * @param Replace A string to replace with in the file name
	 * @param Suffix A string to add at the end of the new file name
	 * @param Prefix A string to add to the start of the new file name
	 * @param bRemapReferencedAssets Whether to remap existing references in the animation assets
	 * @param DirectoryMappings Map of source to destination directories for recreating file structures
	 *
	 * Return: list of new animation files created.*/
	UFUNCTION(BlueprintCallable, Category=IKBatchRetarget)
	static TArray<FAssetData> DuplicateAndRetarget(
		const TArray<FAssetData>& AssetsToRetarget,
		USkeletalMesh* SourceMesh,
		USkeletalMesh* TargetMesh,
		UIKRetargeter* IKRetargetAsset,
		const TMap<FString, FString>& DirectoryMappings,
		const FString& Search = "",
		const FString& Replace = "",
		const FString& Prefix = "",
		const FString& Suffix = "",
		const bool bIncludeReferencedAssets=true);

	// Actually run the process to duplicate and retarget the assets for the given context
	void RunRetarget(FAlsRetargetBatchOperationContext& Context);

private:

	void Reset();

	// Initialize set of referenced assets to retarget.
	// @return	Number of assets that need retargeting.
	int32 GenerateAssetLists(const FAlsRetargetBatchOperationContext& Context);

	// Enhanced asset reference gathering that includes linked animation graphs
	void GatherAllReferencedAssets(UAnimBlueprint* AnimBlueprint, TArray<UAnimationAsset*>& OutAnimationAssets);

	// Calculate the target directory path based on directory mappings, preserving subdirectory structure
	FString CalculateTargetDirectoryPath(const FString& SourceAssetPath, const TMap<FString, FString>& DirectoryMappings) const;

	// Duplicate all the assets to retarget
	void DuplicateRetargetAssets(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	// Retarget skeleton and animation on all the duplicates
	void RetargetAssets(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	// Convert animation on all the duplicates
	void ConvertAnimation(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	// Copy/remap curves on all the duplicates
	void RemapCurves(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	// Replace linked animation graph references in retargeted blueprints
	void ReplaceLinkedAnimGraphReferences(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	// Replace existing assets (optional)
	void OverwriteExistingAssets(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	// Output notifications of results
	void NotifyUserOfResults(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress) const;

	// Generate list of newly created assets to report to user
	void GetNewAssets(TArray<UObject*>& NewAssets) const;

	// If user cancelled half way, cleanup all the duplicated assets
	void CleanupIfCancelled(const FScopedSlowTask& Progress) const;

	// Lists of assets to retarget. Populated from selection during init
	TArray<UAnimationAsset*>	AnimationAssetsToRetarget;
	TArray<UAnimBlueprint*>		AnimBlueprintsToRetarget;

	// Lists of original assets map to duplicate assets
	TMap<UAnimationAsset*, UAnimationAsset*>	DuplicatedAnimAssets;
	TMap<UAnimBlueprint*, UAnimBlueprint*>		DuplicatedBlueprints;

	TMap<UAnimationAsset*, UAnimationAsset*>	RemappedAnimAssets;
};