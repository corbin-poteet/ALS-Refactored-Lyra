// Copyright Epic Games, Inc. All Rights Reserved.

#include "AlsRetargetBatchOperation.h"

#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimPose.h"
#include "AssetToolsModule.h"
#include "Animation/AnimSequence.h"
#include "ContentBrowserModule.h"
#include "EditorReimportHandler.h"
#include "IContentBrowserSingleton.h"
#include "SSkeletonWidget.h"
#include "EditorFramework/AssetImportData.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/ScopedSlowTask.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Animation/AnimMontage.h"
#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/RetargetOps/CurveRemapOp.h"
#include "ObjectTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Retargeter/RetargetOps/SpeedPlantingOp.h"
#include "Rig/IKRigDefinition.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimGraphNode_LinkedAnimGraph.h"
#include "Animation/AnimBlueprintGeneratedClass.h"

#define LOCTEXT_NAMESPACE "RetargetBatchOperation"

void UAlsRetargetBatchOperation::GatherAllReferencedAssets(UAnimBlueprint* AnimBlueprint, TArray<UAnimationAsset*>& OutAnimationAssets)
{
	if (!AnimBlueprint)
	{
		return;
	}

	// Use the original method as a baseline
	GetAllAnimationSequencesReferredInBlueprint(AnimBlueprint, OutAnimationAssets);

	// Additionally, find all linked animation graph nodes and get their referenced blueprints
	TArray<UAnimGraphNode_Base*> LinkedGraphNodes;
	UAnimationBlueprintLibrary::GetNodesOfClass(AnimBlueprint, UAnimGraphNode_LinkedAnimGraph::StaticClass(), LinkedGraphNodes, true);

	for (UAnimGraphNode_Base* GraphNode : LinkedGraphNodes)
	{
		if (const UAnimGraphNode_LinkedAnimGraph* LinkedAnimGraphNode = Cast<UAnimGraphNode_LinkedAnimGraph>(GraphNode))
		{
			// Get the instance class from the node
			if (TSubclassOf<UAnimInstance> InstanceClass = LinkedAnimGraphNode->Node.InstanceClass)
			{
				// Find the animation blueprint that corresponds to this class
				UAnimBlueprint* LinkedAnimBP = nullptr;

				// Check if this is an animation blueprint generated class
				if (const UAnimBlueprintGeneratedClass* AnimBPClass = Cast<UAnimBlueprintGeneratedClass>(*InstanceClass))
				{
					LinkedAnimBP = Cast<UAnimBlueprint>(AnimBPClass->ClassGeneratedBy);
				}

				if (LinkedAnimBP && LinkedAnimBP != AnimBlueprint) // Avoid infinite recursion
				{
					// Add the linked blueprint to our list for retargeting
					AnimBlueprintsToRetarget.AddUnique(LinkedAnimBP);

					// Recursively gather assets from the linked blueprint
					GatherAllReferencedAssets(LinkedAnimBP, OutAnimationAssets);
				}
			}
		}
	}
}

FString UAlsRetargetBatchOperation::CalculateTargetDirectoryPath(const FString& SourceAssetPath, const TMap<FString, FString>& DirectoryMappings) const
{
	// Get the directory path from the asset path (remove the asset name)
	FString SourceDirectory = FPackageName::GetLongPackagePath(SourceAssetPath);

	// Find the best matching source directory mapping
	FString BestMatchSource;
	FString BestMatchDestination;
	int32 BestMatchLength = 0;

	for (const auto& Mapping : DirectoryMappings)
	{
		const FString& MappingSource = Mapping.Key;
		const FString& MappingDestination = Mapping.Value;

		// Check if the source directory starts with this mapping source
		if (SourceDirectory.StartsWith(MappingSource) && MappingSource.Len() > BestMatchLength)
		{
			BestMatchSource = MappingSource;
			BestMatchDestination = MappingDestination;
			BestMatchLength = MappingSource.Len();
		}
	}

	// If we found a matching mapping, calculate the new path
	if (!BestMatchSource.IsEmpty())
	{
		// Get the relative path within the source directory
		FString RelativePath;
		if (SourceDirectory.Len() > BestMatchSource.Len())
		{
			// Remove the best match source from the beginning and get the remainder
			RelativePath = SourceDirectory.RightChop(BestMatchSource.Len());
			// Ensure it starts with a slash
			if (!RelativePath.StartsWith(TEXT("/")))
			{
				RelativePath = TEXT("/") + RelativePath;
			}
		}

		// Combine destination with relative path
		FString NewDirectory = BestMatchDestination + RelativePath;

		UE_LOG(LogTemp, Log, TEXT("Directory mapping: %s -> %s"), *SourceDirectory, *NewDirectory);
		return NewDirectory;
	}

	// No mapping found, return original directory
	return SourceDirectory;
}

int32 UAlsRetargetBatchOperation::GenerateAssetLists(const FAlsRetargetBatchOperationContext& Context)
{
	// re-generate lists of selected and referenced assets
	AnimationAssetsToRetarget.Reset();
	AnimBlueprintsToRetarget.Reset();

	for (TWeakObjectPtr<UObject> AssetPtr : Context.AssetsToRetarget)
	{
		UObject* Asset = AssetPtr.Get();
		if (UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(Asset))
		{
			AnimationAssetsToRetarget.AddUnique(AnimAsset);

			// sequences that are used within the montage need to be added as well to be duplicated. They will then
			// be replaced in UAnimMontage::ReplaceReferredAnimations
			if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimAsset))
			{
				// add segments
				for (const FSlotAnimationTrack& Track: AnimMontage->SlotAnimTracks)
				{
					for (const FAnimSegment& Segment: Track.AnimTrack.AnimSegments)
					{
						if (Segment.IsValid() && Segment.GetAnimReference())
						{
							AnimationAssetsToRetarget.AddUnique(Segment.GetAnimReference());
						}
					}
				}

				// add preview pose
				if (AnimMontage->PreviewBasePose)
				{
					AnimationAssetsToRetarget.AddUnique(AnimMontage->PreviewBasePose);
				}
			}
		}
		else if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset))
		{
			// Add parents blueprint.
			UAnimBlueprint* ParentBP = Cast<UAnimBlueprint>(AnimBlueprint->ParentClass->ClassGeneratedBy);
			while (ParentBP)
			{
				AnimBlueprintsToRetarget.AddUnique(ParentBP);
				ParentBP = Cast<UAnimBlueprint>(ParentBP->ParentClass->ClassGeneratedBy);
			}

			AnimBlueprintsToRetarget.AddUnique(AnimBlueprint);
		}
	}

	if (Context.bIncludeReferencedAssets)
	{
		// Grab assets from the blueprint using enhanced gathering that includes linked animation graphs.
		// Do this first as it can add complex assets to the retarget array which will need to be processed next.
		// Make a copy of the array to avoid "Array has changed during ranged-for iteration" error
		TArray<UAnimBlueprint*> AnimBlueprintsToProcess = AnimBlueprintsToRetarget;
		for (UAnimBlueprint* AnimBlueprint : AnimBlueprintsToProcess)
		{
			GatherAllReferencedAssets(AnimBlueprint, AnimationAssetsToRetarget);
		}

		int32 AssetIndex = 0;
		while (AssetIndex < AnimationAssetsToRetarget.Num())
		{
			UAnimationAsset* AnimAsset = AnimationAssetsToRetarget[AssetIndex++];
			AnimAsset->HandleAnimReferenceCollection(AnimationAssetsToRetarget, true);
		}
	}

	return AnimationAssetsToRetarget.Num() + AnimBlueprintsToRetarget.Num();
}

void UAlsRetargetBatchOperation::DuplicateRetargetAssets(
	const FAlsRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress)
{
	Progress.EnterProgressFrame(1.f, FText(LOCTEXT("DuplicatingBatchRetarget", "Duplicating animation assets...")));

	UPackage* DestinationPackage = Context.TargetMesh->GetOutermost();

	TArray<UAnimationAsset*> AnimationAssetsToDuplicate = AnimationAssetsToRetarget;
	TArray<UAnimBlueprint*> AnimBlueprintsToDuplicate = AnimBlueprintsToRetarget;

	// We only want to duplicate unmapped assets, so we remove mapped assets from the list we're duplicating
	for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : RemappedAnimAssets)
	{
		AnimationAssetsToDuplicate.Remove(Pair.Key);
	}

	// duplicate each asset individually (not done as a batch so user can cancel)
	for (UAnimationAsset* Asset : AnimationAssetsToDuplicate)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		FString AssetName = Asset->GetName();
		Progress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("DuplicatingAnimation", "Duplicating animation: {0}"), FText::FromString(AssetName)));

		// Calculate target directory path based on directory mappings
		FNameDuplicationRule NameRule = Context.NameRule;
		if (Context.DirectoryMappings.Num() > 0)
		{
			// Use directory mappings to determine new location
			FString TargetDirectory = CalculateTargetDirectoryPath(Asset->GetPathName(), Context.DirectoryMappings);
			NameRule.FolderPath = TargetDirectory / TEXT("");
		}
		else if (Context.bUseSourcePath)
		{
			// Fallback to original behavior if no mappings provided
			NameRule.FolderPath = FPackageName::GetLongPackagePath(Asset->GetPathName()) / TEXT("");
		}

		TMap<UAnimationAsset*, UAnimationAsset*> DuplicateMap = DuplicateAssets<UAnimationAsset>({Asset}, DestinationPackage, &NameRule);
		DuplicatedAnimAssets.Append(DuplicateMap);
	}
	for (UAnimBlueprint* Asset : AnimBlueprintsToDuplicate)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		FString AssetName = Asset->GetName();
		Progress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("DuplicatingBlueprint", "Duplicating blueprint: {0}"), FText::FromString(AssetName)));

		// Calculate target directory path based on directory mappings
		FNameDuplicationRule NameRule = Context.NameRule;
		if (Context.DirectoryMappings.Num() > 0)
		{
			// Use directory mappings to determine new location
			FString TargetDirectory = CalculateTargetDirectoryPath(Asset->GetPathName(), Context.DirectoryMappings);
			NameRule.FolderPath = TargetDirectory / TEXT("");
		}
		else if (Context.bUseSourcePath)
		{
			// Fallback to original behavior if no mappings provided
			NameRule.FolderPath = FPackageName::GetLongPackagePath(Asset->GetPathName()) / TEXT("");
		}

		TMap<UAnimBlueprint*, UAnimBlueprint*> DuplicateMap = DuplicateAssets<UAnimBlueprint>({Asset}, DestinationPackage, &NameRule);
		DuplicatedBlueprints.Append(DuplicateMap);
	}

	// If we are moving the new asset to a different directory we need to fixup the reimport path.
	// This should only effect source FBX paths within the project.
	if (!Context.NameRule.FolderPath.IsEmpty())
	{
		for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : DuplicatedAnimAssets)
		{
			UAnimSequence* SourceSequence = Cast<UAnimSequence>(Pair.Key);
			UAnimSequence* DestinationSequence = Cast<UAnimSequence>(Pair.Value);
			if (!(SourceSequence && DestinationSequence))
			{
				continue;
			}

			for (int index = 0; index < SourceSequence->AssetImportData->SourceData.SourceFiles.Num(); index++)
			{
				const FString& RelativeFilename = SourceSequence->AssetImportData->SourceData.SourceFiles[index].RelativeFilename;
				const FString OldPackagePath = FPackageName::GetLongPackagePath(SourceSequence->GetPathName()) / TEXT("");
				const FString NewPackagePath = FPackageName::GetLongPackagePath(DestinationSequence->GetPathName()) / TEXT("");
				const FString AbsoluteSrcPath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(OldPackagePath));
				const FString SrcFile = AbsoluteSrcPath / RelativeFilename;
				const bool bSrcFileExists = FPlatformFileManager::Get().GetPlatformFile().FileExists(*SrcFile);
				if (!bSrcFileExists || (NewPackagePath == OldPackagePath))
				{
					continue;
				}

				FString BasePath = FPackageName::LongPackageNameToFilename(OldPackagePath);
				FString OldSourceFilePath = FPaths::ConvertRelativePathToFull(BasePath, RelativeFilename);
				TArray<FString> Paths;
				Paths.Add(OldSourceFilePath);

				// update the FBX reimport file path
				FReimportManager::Instance()->UpdateReimportPaths(DestinationSequence, Paths);
			}
		}
	}

	// Remapped assets needs the duplicated ones added
	RemappedAnimAssets.Append(DuplicatedAnimAssets);

	DuplicatedAnimAssets.GenerateValueArray(AnimationAssetsToRetarget);
	DuplicatedBlueprints.GenerateValueArray(AnimBlueprintsToRetarget);
}

void UAlsRetargetBatchOperation::RetargetAssets(
	const FAlsRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress)
{
	USkeleton* OldSkeleton = Context.SourceMesh->GetSkeleton();
	USkeleton* NewSkeleton = Context.TargetMesh->GetSkeleton();

	TArray<FAdditiveRetargetSettings> SettingsToRestoreAfterRetarget;

	for (UAnimationAsset* AssetToRetarget : AnimationAssetsToRetarget)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		// prepare animation sequence asset to receive retargeted animation
		if (UAnimSequence* AnimSequenceToRetarget = Cast<UAnimSequence>(AssetToRetarget))
		{
			FString AssetName = AnimSequenceToRetarget->GetName();
			Progress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("PreparingAsset", "Preparing asset: {0}"), FText::FromString(AssetName)));

			// copy curve data from source asset, preserving data in the target if present.
			UAnimationBlueprintLibrary::CopyAnimationCurveNamesToSkeleton(OldSkeleton, NewSkeleton, AnimSequenceToRetarget, ERawCurveTrackTypes::RCT_Float);

			// clear transform curves since those curves won't work in new skeleton
			IAnimationDataController& Controller = AnimSequenceToRetarget->GetController();
			constexpr bool bShouldTransact = false;
			Controller.OpenBracket(FText::FromString("Preparing for retargeted animation."), bShouldTransact);
			Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Transform, bShouldTransact);

			// reset all additive animation properties to ensure WYSIWYG playback of additive anims between retargeter and sequence
			FAdditiveRetargetSettings SequenceSettings;
			SequenceSettings.PrepareForRetarget(AnimSequenceToRetarget);
			SettingsToRestoreAfterRetarget.Add(SequenceSettings);

			// set the retarget source to the target skeletal mesh
			AnimSequenceToRetarget->RetargetSource = NAME_None;
			AnimSequenceToRetarget->SetRetargetSourceAsset(Context.TargetMesh);

			// removes all bone tracks not belonging to this skeleton, adds those that do
			Controller.UpdateWithSkeleton(NewSkeleton, bShouldTransact);

			// done editing sequence data, close bracket
			Controller.CloseBracket(bShouldTransact);
		}

		// replace references to other animation
		AssetToRetarget->ReplaceReferredAnimations(RemappedAnimAssets);
		AssetToRetarget->SetSkeleton(NewSkeleton);
		AssetToRetarget->SetPreviewMesh(Context.TargetMesh);
	}

	// Call PostEditChange after the references of all assets were replaced, to prevent order dependence of post edit
	// change hooks. If PostEditChange is called right after ReplaceReferredAnimations it can access references that are
	// still queued for retarget and follow the current asset in the array.
	for (UAnimationAsset* AssetToRetarget : AnimationAssetsToRetarget)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		// force updating of the retarget pose, this is normally done on PreSave() but is guarded against procedural saves
		if (UAnimSequence* AnimSequenceToRetarget = Cast<UAnimSequence>(AssetToRetarget))
		{
			AnimSequenceToRetarget->UpdateRetargetSourceAssetData();
		}

		AssetToRetarget->PostEditChange();
		AssetToRetarget->MarkPackageDirty();
	}

	// convert the animation using the IK retargeter
	ConvertAnimation(Context,Progress);

	// optionally restore the additive flags
	if (Context.bRetainAdditiveFlags)
	{
		for (FAdditiveRetargetSettings SettingsToRestore : SettingsToRestoreAfterRetarget)
		{
			SettingsToRestore.RestoreOnAsset(RemappedAnimAssets);
		}
	}

	// convert all Animation Blueprints and compile
	for (UAnimBlueprint* AnimBlueprint : AnimBlueprintsToRetarget)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		// replace skeleton
		AnimBlueprint->TargetSkeleton = NewSkeleton;
		// replace preview mesh (uses skeleton default otherwise)
		AnimBlueprint->SetPreviewMesh(Context.TargetMesh);

		// if they have parent blueprint, make sure to re-link to the new one also
		if (const UAnimBlueprint* CurrentParentBP = Cast<UAnimBlueprint>(AnimBlueprint->ParentClass->ClassGeneratedBy))
		{
			if (UAnimBlueprint* const* ParentBP = DuplicatedBlueprints.Find(CurrentParentBP))
			{
				AnimBlueprint->ParentClass = (*ParentBP)->GeneratedClass;
			}
		}

		if(RemappedAnimAssets.Num() > 0)
		{
			ReplaceReferredAnimationsInBlueprint(AnimBlueprint, RemappedAnimAssets);
		}
	}

	// Replace linked animation graph references with retargeted versions
	ReplaceLinkedAnimGraphReferences(Context, Progress);

	// Compile all blueprints after all references have been updated
	for (UAnimBlueprint* AnimBlueprint : AnimBlueprintsToRetarget)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		FBlueprintEditorUtils::RefreshAllNodes(AnimBlueprint);
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
		AnimBlueprint->PostEditChange();
		AnimBlueprint->MarkPackageDirty();
	}

	// copy/remap curves to duplicate sequences
	RemapCurves(Context, Progress);
}

void UAlsRetargetBatchOperation::ConvertAnimation(
	const FAlsRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress)
{
	// initialize the retargeter
	FIKRetargetProcessor Processor;
	FRetargetProfile RetargetProfile;
	RetargetProfile.FillProfileWithAssetSettings(Context.IKRetargetAsset);
	Processor.Initialize(Context.SourceMesh, Context.TargetMesh, Context.IKRetargetAsset, RetargetProfile);
	if (!Processor.IsInitialized())
	{
		UE_LOG(LogTemp, Warning, TEXT("Unable to initialize the IK Retargeter. Newly created animations were not retargeted!"));
		return;
	}

	// target skeleton data
	const FRetargetSkeleton& TargetSkeleton = Processor.GetSkeleton(ERetargetSourceOrTarget::Target);
	const TArray<FName>& TargetBoneNames = TargetSkeleton.BoneNames;
	const int32 NumTargetBones = TargetBoneNames.Num();

	// allocate target keyframe data
	TArray<FRawAnimSequenceTrack> BoneTracks;
	BoneTracks.SetNumZeroed(NumTargetBones);

	// source skeleton data
	const FRetargetSkeleton& SourceSkeleton = Processor.GetSkeleton(ERetargetSourceOrTarget::Source);
	const TArray<FName>& SourceBoneNames = SourceSkeleton.BoneNames;
	const int32 NumSourceBones = SourceBoneNames.Num();

	TArray<FTransform> SourceComponentPose;
	SourceComponentPose.SetNum(NumSourceBones);

	// get list of all speed curves referenced by the speed planting ops (if there are any)
	TArray<FName> SpeedCurveNames;
	const TArray<FIKRetargetSpeedPlantingOp*> SpeedPlantingOps = Context.IKRetargetAsset->GetAllRetargetOpsOfType<FIKRetargetSpeedPlantingOp>();
	for (FIKRetargetSpeedPlantingOp* SpeedPlantingOp : SpeedPlantingOps)
	{
		SpeedCurveNames.Append(SpeedPlantingOp->GetRequiredSpeedCurves());
	}

	// while retargeting, record a map of (sequence name, curve name) for any curve missing in a sequence
	TMap<FString, TSet<FName>> MissingSpeedCurves;

	// for each pair of source / target animation sequences
	for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : DuplicatedAnimAssets)
	{
		if (Progress.ShouldCancel())
		{
			return;
		}

		UAnimSequence* SourceSequence = Cast<UAnimSequence>(Pair.Key);
		UAnimSequence* TargetSequence = Cast<UAnimSequence>(Pair.Value);
		if (!(SourceSequence && TargetSequence))
		{
			continue;
		}

		// increment progress bar
		FString AssetName = TargetSequence->GetName();
		Progress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("RunningBatchRetarget", "Retargeting animation asset: {0}"), FText::FromString(AssetName)));

		// update the sequence to use the new skeleton
		IAnimationDataController& TargetSeqController = TargetSequence->GetController();
		constexpr bool bShouldTransact = false;
		TargetSeqController.OpenBracket(FText::FromString("Generating Retargeted Animation Data"), bShouldTransact);
		TargetSeqController.NotifyPopulated(); // must set bIsPopulated=true otherwise "UpdateWithSkeleton" will early out
		// this removes all bone tracks and reinitializes the FKControlRig to use the new skeleton.
		// NOTE: we do NOT ResetModel() because we want to keep curves and attributes from the source
		TargetSeqController.UpdateWithSkeleton(const_cast<USkeleton*>(TargetSkeleton.SkeletalMesh->GetSkeleton()), bShouldTransact);

		// number of frames in this animation
		const int32 NumFrames = SourceSequence->GetNumberOfSampledKeys();

		// BoneTracks arrays allocation
		for (int32 TargetBoneIndex=0; TargetBoneIndex<NumTargetBones; ++TargetBoneIndex)
		{
			BoneTracks[TargetBoneIndex].PosKeys.SetNum(NumFrames);
			BoneTracks[TargetBoneIndex].RotKeys.SetNum(NumFrames);
			BoneTracks[TargetBoneIndex].ScaleKeys.SetNum(NumFrames);
		}

		// ensure we evaluate the source animation using the skeletal mesh proportions that were evaluated in the viewport
		FAnimPoseEvaluationOptions EvaluationOptions = FAnimPoseEvaluationOptions();
		EvaluationOptions.OptionalSkeletalMesh = const_cast<USkeletalMesh*>(SourceSkeleton.SkeletalMesh);
		// ensure WYSIWYG with editor by ensuring the same root motion is applied to the pose, not to the component
		EvaluationOptions.bExtractRootMotion = false;
		EvaluationOptions.bIncorporateRootMotionIntoPose = true;

		// reset playback of ops
		Processor.OnPlaybackReset();

		// retarget each frame's pose from source to target
		for (int32 FrameIndex=0; FrameIndex<NumFrames; ++FrameIndex)
		{
			if (Progress.ShouldCancel())
			{
				TargetSeqController.CloseBracket(bShouldTransact);
				return;
			}

			// get the source global pose
			FAnimPose SourcePoseAtFrame;
			UAnimPoseExtensions::GetAnimPoseAtFrame(SourceSequence, FrameIndex, EvaluationOptions, SourcePoseAtFrame);

			// we don't use UAnimPoseExtensions::GetBoneNames as the sequence can store bones that only exist on the
			// skeleton, but not on the current mesh. This results in indices discrepancy
			for (int32 BoneIndex = 0; BoneIndex < NumSourceBones; BoneIndex++)
			{
				const FName& BoneName = SourceBoneNames[BoneIndex];
				SourceComponentPose[BoneIndex] = UAnimPoseExtensions::GetBonePose(SourcePoseAtFrame, BoneName, EAnimPoseSpaces::World);
			}

			// strip all scale out of the pose values, the translation of a component-space pose has incorporated scale values
			for (FTransform& Transform : SourceComponentPose)
			{
				Transform.SetScale3D(FVector::OneVector);
			}

			// calculate the delta time
			const float TimeAtCurrentFrame = SourceSequence->GetTimeAtFrame(FrameIndex);
			float DeltaTime = TimeAtCurrentFrame;
			if (FrameIndex > 0)
			{
				const float TimeAtPrevFrame = SourceSequence->GetTimeAtFrame(FrameIndex-1);
				DeltaTime = TimeAtCurrentFrame - TimeAtPrevFrame;
			}

			// get the curve values from the source sequence (for speed-based IK planting)
			TMap<FName, float> SpeedCurveValues;
			constexpr bool bForceUseRawData = false;
			for (const FName& SpeedCurveName : SpeedCurveNames)
			{
				if (!SourceSequence->HasCurveData(SpeedCurveName, bForceUseRawData))
				{
					TSet<FName>& MissingCurves = MissingSpeedCurves.FindOrAdd(SourceSequence->GetName());
					MissingCurves.Add(SpeedCurveName);
					continue;
				}

				SpeedCurveValues.Add(SpeedCurveName, SourceSequence->EvaluateCurveData(SpeedCurveName, FAnimExtractContext(static_cast<double>(TimeAtCurrentFrame))));
			}

			// get the settings profile
			FRetargetProfile SettingsProfile;
			SettingsProfile.FillProfileWithAssetSettings(Context.IKRetargetAsset);

			// give processor the pose to apply source scaling (if needed)
			Processor.ScaleSourcePose(SourceComponentPose);

			// run the retargeter
			const TArray<FTransform>& TargetComponentPose = Processor.RunRetargeter(SourceComponentPose, SettingsProfile, DeltaTime);

			// convert to a local-space pose
			TArray<FTransform> TargetLocalPose = TargetComponentPose;
			TargetSkeleton.UpdateLocalTransformsBelowBone(0,TargetLocalPose, TargetComponentPose);

			// store key data for each bone
			for (int32 TargetBoneIndex=0; TargetBoneIndex<NumTargetBones; ++TargetBoneIndex)
			{
				const FTransform& LocalPose = TargetLocalPose[TargetBoneIndex];

				FRawAnimSequenceTrack& BoneTrack = BoneTracks[TargetBoneIndex];

				BoneTrack.PosKeys[FrameIndex] = FVector3f(LocalPose.GetLocation());
				BoneTrack.RotKeys[FrameIndex] = FQuat4f(LocalPose.GetRotation().GetNormalized());
				BoneTrack.ScaleKeys[FrameIndex] = FVector3f(LocalPose.GetScale3D());
			}

		} // END for each frame

		// add keys to bone tracks
		for (int32 TargetBoneIndex=0; TargetBoneIndex<NumTargetBones; ++TargetBoneIndex)
		{
			const FName& TargetBoneName = TargetBoneNames[TargetBoneIndex];

			const FRawAnimSequenceTrack& RawTrack = BoneTracks[TargetBoneIndex];
			TargetSeqController.AddBoneCurve(TargetBoneName, bShouldTransact);
			TargetSeqController.SetBoneTrackKeys(TargetBoneName, RawTrack.PosKeys, RawTrack.RotKeys, RawTrack.ScaleKeys, bShouldTransact);
		}

		TargetSeqController.CloseBracket(bShouldTransact);
	}

	// warn about any missing curves
	for (const TPair<FString, TSet<FName>>& Pair : MissingSpeedCurves)
	{
		for (const FName& MissingCurve : Pair.Value)
		{
			UE_LOG(LogTemp, Warning, TEXT("IK Retarget Batch Operation: Missing speed curve, %s on sequence %s."), *MissingCurve.ToString(), *Pair.Key);
		}
	}
}

void UAlsRetargetBatchOperation::RemapCurves(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress)
{
	USkeleton* SourceSkeleton = Context.SourceMesh->GetSkeleton();
	USkeleton* TargetSkeleton = Context.TargetMesh->GetSkeleton();

	// get map of curves to remap (Source:Target)
	bool bCopyAllSourceCurves = true;
	TMap<FAnimationCurveIdentifier, FAnimationCurveIdentifier> CurvesToRemap;
	const TArray<FInstancedStruct>& OpStack = Context.IKRetargetAsset->GetRetargetOps();
	for (const FInstancedStruct& OpStruct : OpStack)
	{
		if (OpStruct.GetScriptStruct() != FIKRetargetCurveRemapOp::StaticStruct())
		{
			continue;
		}

		const FIKRetargetCurveRemapOp* CurveRemapOp = OpStruct.GetPtr<FIKRetargetCurveRemapOp>();
		if (!CurveRemapOp)
		{
			continue;
		}

		if (!CurveRemapOp->IsEnabled())
		{
			continue;
		}

		for (const FCurveRemapPair& CurveToRemap : CurveRemapOp->Settings.CurvesToRemap)
		{
			const FAnimationCurveIdentifier SourceCurveId = UAnimationCurveIdentifierExtensions::FindCurveIdentifier(SourceSkeleton, CurveToRemap.SourceCurve, ERawCurveTrackTypes::RCT_Float);
			const FAnimationCurveIdentifier TargetCurveId = UAnimationCurveIdentifierExtensions::FindCurveIdentifier(TargetSkeleton, CurveToRemap.TargetCurve, ERawCurveTrackTypes::RCT_Float);
			CurvesToRemap.Add(SourceCurveId, TargetCurveId);
		}

		bCopyAllSourceCurves &= CurveRemapOp->Settings.bCopyAllSourceCurves;
	}

	// update progress bar
	Progress.EnterProgressFrame(1.f, FText::Format(LOCTEXT("RemappingCurves", "Remapping {0} curves on animation assets..."), FText::AsNumber(CurvesToRemap.Num())));

	// for each exported animation, remap curves from source to target anim
	for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : DuplicatedAnimAssets)
	{
		UAnimSequence* SourceSequence = Cast<UAnimSequence>(Pair.Key);
		UAnimSequence* TargetSequence = Cast<UAnimSequence>(Pair.Value);
		if (!(SourceSequence && TargetSequence))
		{
			continue;
		}

		// increment progress bar
		if (Progress.ShouldCancel())
		{
			return;
		}

		// all curves were copied when we duplicated the animation sequence, so now we have to rename curves
		// based on the remapping defined in the curve remap op(s)
		IAnimationDataController& TargetSeqController = TargetSequence->GetController();
		constexpr bool bShouldTransact = false;
		TargetSeqController.OpenBracket(FText::FromString("Remapping Curve Data"), bShouldTransact);

		const IAnimationDataModel* SourceDataModel = SourceSequence->GetDataModel();

		for (const TTuple<FAnimationCurveIdentifier, FAnimationCurveIdentifier>& CurveToRemap : CurvesToRemap)
		{
			// get the source curve to copy from
			const FFloatCurve* SourceCurve =  SourceDataModel->FindFloatCurve(CurveToRemap.Key);
			if (!SourceCurve)
			{
				continue; // missing source curve to remap
			}

			// add a curve to the target to house the keys
			FAnimationCurveIdentifier TargetCurveID = CurveToRemap.Value;
			if (!TargetCurveID.IsValid())
			{
				continue; // must provide a valid name for the target curve
			}
			TargetSeqController.AddCurve(TargetCurveID, SourceCurve->GetCurveTypeFlags(), bShouldTransact);

			// copy data into target curve
			TargetSeqController.SetCurveKeys(TargetCurveID, SourceCurve->FloatCurve.GetConstRefOfKeys(), bShouldTransact);
			TargetSeqController.SetCurveColor(TargetCurveID, SourceCurve->GetColor(), bShouldTransact);
		}

		// optionally remove all source curves from the target asset
		// (remove all curves that were copied when the source sequence was duplicated UNLESS they are remapped)
		if (!bCopyAllSourceCurves)
		{
			// get list of target curves to keep
			TArray<FAnimationCurveIdentifier> TargetCurvesToKeep;
			CurvesToRemap.GenerateValueArray(TargetCurvesToKeep);

			// get list of target curves to remove
			const TArray<FFloatCurve>& AllTargetCurves = TargetSeqController.GetModel()->GetFloatCurves();
			TArray<FAnimationCurveIdentifier> CurvesToRemove;
			for (const FFloatCurve& TargetCurve : AllTargetCurves)
			{
				const FAnimationCurveIdentifier TargetCurveId = UAnimationCurveIdentifierExtensions::FindCurveIdentifier(TargetSkeleton, TargetCurve.GetName(), ERawCurveTrackTypes::RCT_Float);
				if (TargetCurvesToKeep.Contains(TargetCurveId))
				{
					continue;
				}
				CurvesToRemove.Add(TargetCurveId);
			}

			// remove the curves
			for (const FAnimationCurveIdentifier& CurveToRemove : CurvesToRemove)
			{
				TargetSeqController.RemoveCurve(CurveToRemove, bShouldTransact);
			}
		}

		TargetSeqController.CloseBracket(bShouldTransact);
	}
}

void UAlsRetargetBatchOperation::ReplaceLinkedAnimGraphReferences(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress)
{
	// Update progress bar
	Progress.EnterProgressFrame(1.f, FText(LOCTEXT("ReplacingLinkedAnimGraphs", "Replacing linked animation graph references...")));

	// For each retargeted animation blueprint, update its linked animation graph nodes to reference the new retargeted blueprints
	for (TPair<UAnimBlueprint*, UAnimBlueprint*>& BlueprintPair : DuplicatedBlueprints)
	{
		UAnimBlueprint* TargetBlueprint = BlueprintPair.Value;
		if (!TargetBlueprint)
		{
			continue;
		}

		if (Progress.ShouldCancel())
		{
			return;
		}

		// Find all linked animation graph nodes in this blueprint
		TArray<UAnimGraphNode_Base*> LinkedGraphNodes;
		UAnimationBlueprintLibrary::GetNodesOfClass(TargetBlueprint, UAnimGraphNode_LinkedAnimGraph::StaticClass(), LinkedGraphNodes, true);

		bool bModifiedBlueprint = false;
		for (UAnimGraphNode_Base* GraphNode : LinkedGraphNodes)
		{
			if (UAnimGraphNode_LinkedAnimGraph* LinkedAnimGraphNode = Cast<UAnimGraphNode_LinkedAnimGraph>(GraphNode))
			{
				// Get the current instance class from the node
				TSubclassOf<UAnimInstance> CurrentInstanceClass = LinkedAnimGraphNode->Node.InstanceClass;
				if (CurrentInstanceClass)
				{
					// Find the original animation blueprint that corresponds to this class
					UAnimBlueprint* OriginalLinkedAnimBP = nullptr;
					if (const UAnimBlueprintGeneratedClass* AnimBPClass = Cast<UAnimBlueprintGeneratedClass>(*CurrentInstanceClass))
					{
						OriginalLinkedAnimBP = Cast<UAnimBlueprint>(AnimBPClass->ClassGeneratedBy);
					}

					// Check if we have a retargeted version of this blueprint
					if (OriginalLinkedAnimBP)
					{
						UAnimBlueprint* const* NewLinkedAnimBP = DuplicatedBlueprints.Find(OriginalLinkedAnimBP);
						if (NewLinkedAnimBP && *NewLinkedAnimBP)
						{
							// Update the node to reference the new retargeted blueprint
							LinkedAnimGraphNode->Node.InstanceClass = (*NewLinkedAnimBP)->GeneratedClass;
							bModifiedBlueprint = true;

							UE_LOG(LogTemp, Log, TEXT("Replaced linked animation graph reference in %s: %s -> %s"),
								*TargetBlueprint->GetName(),
								*OriginalLinkedAnimBP->GetName(),
								*(*NewLinkedAnimBP)->GetName());
						}
					}
				}
			}
		}

		// If we modified any linked animation graph references, mark the blueprint for recompilation
		if (bModifiedBlueprint)
		{
			TargetBlueprint->MarkPackageDirty();
		}
	}
}

void UAlsRetargetBatchOperation::OverwriteExistingAssets(const FAlsRetargetBatchOperationContext& Context, FScopedSlowTask& Progress)
{
	if (!Context.bOverwriteExistingFiles)
	{
		return;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// for each retargeted asset, check if we need to replace an existing asset (one with the desired name, same location, same type)
	for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : DuplicatedAnimAssets)
	{
		UAnimationAsset* OldAsset = Pair.Key;
		UAnimationAsset* NewAsset = Pair.Value;

		// get desired name
		FString DesiredObjectName = Context.NameRule.Rename(OldAsset);
		if (NewAsset->GetName() == DesiredObjectName)
		{
			// asset was not renamed due to collision with existing asset, so there's nothing to replace
			continue;
		}

		// destination path
		FString PathName = FPackageName::GetLongPackagePath(NewAsset->GetPathName());
		FString DesiredPackageName = PathName + "/" + DesiredObjectName;
		FString DesiredObjectPath = DesiredPackageName + "." + DesiredObjectName;
		FAssetData AssetDataToReplace = AssetRegistryModule.Get().GetAssetByObjectPath(FSoftObjectPath(DesiredObjectPath));
		const bool bHasDuplicateToReplace = AssetDataToReplace.IsValid() && AssetDataToReplace.GetAsset()->GetClass() == OldAsset->GetClass();
		if (!bHasDuplicateToReplace)
		{
			// this could happen if the desired name was already in use by a different asset type
			continue;
		}

		UObject* AssetToReplace = AssetDataToReplace.GetAsset();
		if (AssetToReplace == OldAsset)
		{
			// we only replace previously retargeted animations, never the original
			continue;
		}

		// reroute all references from old asset to new asset
		TArray<UObject*> AssetsToReplace = {AssetToReplace};
		ObjectTools::ForceReplaceReferences(NewAsset, AssetsToReplace);

		// delete the old asset
		ObjectTools::ForceDeleteObjects({AssetToReplace}, false /*bShowConfirmation*/);

		// rename the new asset with the desired name
		FString CurrentAssetPath = NewAsset->GetPathName();
		TArray<FAssetRenameData> AssetsToRename = { FAssetRenameData(CurrentAssetPath, DesiredObjectPath) };
		AssetToolsModule.Get().RenameAssets(AssetsToRename);
	}
}

void UAlsRetargetBatchOperation::NotifyUserOfResults(
	const FAlsRetargetBatchOperationContext& Context,
	FScopedSlowTask& Progress) const
{
	// gather newly created objects
	TArray<UObject*> NewAssets;
	GetNewAssets(NewAssets);

	// select all new assets and show in the content browser
	TArray<FAssetData> CurrentSelection;
	for(UObject* NewObject : NewAssets)
	{
		CurrentSelection.Add(FAssetData(NewObject));
	}
	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(CurrentSelection);

	// create pop-up notification in editor UI
	constexpr float NotificationDuration = 5.0f;
	if (Progress.ShouldCancel())
	{
		Progress.EnterProgressFrame(1.f, FText(LOCTEXT("CancelledBatchRetarget", "Cancelled.")));

		// notify user that retarget was cancelled
		FNotificationInfo Notification(FText::GetEmpty());
		Notification.ExpireDuration = NotificationDuration;
		Notification.Text = FText(LOCTEXT("BatchRetargetCancelled", "Batch retarget cancelled."));
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
	else
	{
		Progress.EnterProgressFrame(1.f, FText(LOCTEXT("DoneBatchRetarget", "Duplicate and retarget complete!")));

		// log details of what assets were created
		for (const UObject* NewAsset : NewAssets)
		{
			UE_LOG(LogTemp, Display, TEXT("Duplicate and Retarget - New Asset Created: %s"), *NewAsset->GetName());
		}

		// notify user that retarget completed
		FNotificationInfo Notification(FText::GetEmpty());
		Notification.ExpireDuration = NotificationDuration;
		Notification.Text = FText::Format(
			LOCTEXT("MultiNonDuplicatedAsset", "{0} assets were retargeted to new skeleton {1}. See Output for details."),
			FText::AsNumber(NewAssets.Num()),
			FText::FromString(Context.TargetMesh->GetName()));
		FSlateNotificationManager::Get().AddNotification(Notification);
	}
}

void UAlsRetargetBatchOperation::GetNewAssets(TArray<UObject*>& NewAssets) const
{
	TArray<UAnimationAsset*> NewAnims;
	DuplicatedAnimAssets.GenerateValueArray(NewAnims);
	for (UAnimationAsset* NewAnim : NewAnims)
	{
		NewAssets.Add(Cast<UObject>(NewAnim));
	}

	TArray<UAnimBlueprint*> NewBlueprints;
	DuplicatedBlueprints.GenerateValueArray(NewBlueprints);
	for (UAnimBlueprint* NewBlueprint : NewBlueprints)
	{
		NewAssets.Add(Cast<UObject>(NewBlueprint));
	}
}

void UAlsRetargetBatchOperation::CleanupIfCancelled(const FScopedSlowTask& Progress) const
{
	if (!Progress.ShouldCancel())
	{
		return;
	}

	// get list of all the assets that were created
	// (to be removed after being cancelled)
	TArray<UObject*> NewAssets;
	GetNewAssets(NewAssets);

	// delete any newly created assets
	constexpr bool bShowConfirmation = true;
	ObjectTools::DeleteObjects(NewAssets, bShowConfirmation);
}

void FAdditiveRetargetSettings::PrepareForRetarget(UAnimSequence* InSequenceAsset)
{
	if (!ensure(InSequenceAsset))
	{
		return;
	}

	SequenceAsset = InSequenceAsset;

	// store setting values
	AdditiveAnimType = SequenceAsset->AdditiveAnimType;
	RefPoseType = SequenceAsset->RefPoseType;
	RefFrameIndex = SequenceAsset->RefFrameIndex;
	RefPoseSeq = SequenceAsset->RefPoseSeq;

	// remove all additive settings so that retarget happens on base motion
	SequenceAsset->AdditiveAnimType = EAdditiveAnimationType::AAT_None;
	SequenceAsset->RefPoseType = EAdditiveBasePoseType::ABPT_None;
	SequenceAsset->RefFrameIndex = 0;
	SequenceAsset->RefPoseSeq = nullptr;
}

void FAdditiveRetargetSettings::RestoreOnAsset(const TMap<UAnimationAsset*, UAnimationAsset*>& RemappedAnimAssets) const
{
	if (!ensure(SequenceAsset))
	{
		return;
	}

	SequenceAsset->AdditiveAnimType = AdditiveAnimType;
	SequenceAsset->RefPoseType = RefPoseType;
	SequenceAsset->RefFrameIndex = RefFrameIndex;

	// If the original RefPoseSeq was retargeted, use the retargeted version instead of the original
	if (RefPoseSeq)
	{
		if (UAnimationAsset* const* RetargetedRefPose = RemappedAnimAssets.Find(RefPoseSeq))
		{
			SequenceAsset->RefPoseSeq = Cast<UAnimSequence>(*RetargetedRefPose);
		}
		else
		{
			// Fallback to original if not found in remapped assets (shouldn't happen in normal cases)
			SequenceAsset->RefPoseSeq = RefPoseSeq;
		}
	}
	else
	{
		SequenceAsset->RefPoseSeq = nullptr;
	}
}

TArray<FAssetData> UAlsRetargetBatchOperation::DuplicateAndRetarget(
	const TArray<FAssetData>& AssetsToRetarget,
	USkeletalMesh* SourceMesh,
	USkeletalMesh* TargetMesh,
	UIKRetargeter* IKRetargetAsset,
	const TMap<FString, FString>& DirectoryMappings,
	const FString& Search,
	const FString& Replace,
	const FString& Prefix,
	const FString& Suffix,
	const bool bIncludeReferencedAssets
	)
{
	// fill the context with all the data needed to run a batch retarget
	FAlsRetargetBatchOperationContext Context;
	for (const FAssetData& Asset : AssetsToRetarget)
	{
		if (UObject* Object = Cast<UObject>(Asset.GetAsset()))
		{
			Context.AssetsToRetarget.Add(Object); // convert asset data to soft refs
		}
	}
	Context.SourceMesh = SourceMesh;
	Context.TargetMesh = TargetMesh;
	Context.IKRetargetAsset = IKRetargetAsset;
	Context.NameRule.Prefix = Prefix;
	Context.NameRule.Suffix = Suffix;
	Context.NameRule.ReplaceFrom = Search;
	Context.NameRule.ReplaceTo = Replace;
	Context.bIncludeReferencedAssets = bIncludeReferencedAssets;
	Context.DirectoryMappings = DirectoryMappings;

	// actually run the batch operation
	UAlsRetargetBatchOperation* BatchOperation = NewObject<UAlsRetargetBatchOperation>();
	BatchOperation->AddToRoot();
	BatchOperation->RunRetarget(Context);

	// create array of FAssetData to return
	TArray<FAssetData> Results;
	for (const UAnimationAsset* RetargetedAsset : BatchOperation->AnimationAssetsToRetarget)
	{
		Results.Add(FAssetData(RetargetedAsset));
	}

	BatchOperation->RemoveFromRoot();
	return Results;
}

void UAlsRetargetBatchOperation::RunRetarget(FAlsRetargetBatchOperationContext& Context)
{
	Reset();

	// validate animation assets were provided
	const int32 NumAssets = GenerateAssetLists(Context);
	if (NumAssets == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Batch retarget aborted. No animation assets were specified."));
		return;
	}

	// validate a retarget asset was provided
	if (!Context.IKRetargetAsset)
	{
		UE_LOG(LogTemp, Warning, TEXT("Batch retarget aborted. No IK Retargeter asset was specified."));
		return;
	}

	// validate a source IK rig was provided
	const UIKRigDefinition* SrcIKRig = Context.IKRetargetAsset->GetIKRig(ERetargetSourceOrTarget::Source);
	if (!SrcIKRig)
	{
		UE_LOG(LogTemp, Warning, TEXT("Batch retarget aborted. Specified IK Retargeter does not reference a source IK Rig."));
		return;
	}

	// validate a target IK rig was provided
	const UIKRigDefinition* TgtIKRig = Context.IKRetargetAsset->GetIKRig(ERetargetSourceOrTarget::Target);
	if (!TgtIKRig)
	{
		UE_LOG(LogTemp, Warning, TEXT("Batch retarget aborted. Specified IK Retargeter does not reference a target IK Rig."));
		return;
	}

	// validate a source mesh was provided
	if (!Context.SourceMesh)
	{
		// fallback to mesh provided by IK Rig
		Context.SourceMesh = SrcIKRig->GetPreviewMesh();
		if (!Context.SourceMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Batch retarget aborted. No source mesh was specified and the source IK Rig did not have one. "));
			return;
		}
	}

	// validate a target mesh was provided
	if (!Context.TargetMesh)
	{
		// fallback to mesh provided by IK Rig
		Context.TargetMesh = TgtIKRig->GetPreviewMesh();
		if (!Context.TargetMesh)
		{
			UE_LOG(LogTemp, Warning, TEXT("Batch retarget aborted. No target mesh was specified and the target IK Rig did not have one. "));
			return;
		}
	}

	// show progress bar
	constexpr int32 NumAdditionalProgressFrames = 4;
	constexpr int32 NumPassesOverAssets = 3;
	const int32 NumProgressSteps = (NumAssets * NumPassesOverAssets) + NumAdditionalProgressFrames;
	FScopedSlowTask Progress(static_cast<float>(NumProgressSteps), LOCTEXT("GatheringBatchRetarget", "Gathering animation assets..."));
	constexpr bool bShowCancelButton = true;
	Progress.MakeDialog(bShowCancelButton);

	DuplicateRetargetAssets(Context, Progress);
	RetargetAssets(Context, Progress);
	OverwriteExistingAssets(Context, Progress);
	NotifyUserOfResults(Context, Progress);
	CleanupIfCancelled(Progress);
}

void UAlsRetargetBatchOperation::Reset()
{
	AnimationAssetsToRetarget.Reset();
	AnimBlueprintsToRetarget.Reset();
	DuplicatedAnimAssets.Reset();
	DuplicatedBlueprints.Reset();
	RemappedAnimAssets.Reset();
}

#undef LOCTEXT_NAMESPACE