// Fill out your copyright notice in the Description page of Project Settings.


#include "AlsLyraUtility.h"

#include "AssetRegistry/IAssetRegistry.h"

void UAlsLyraUtility::GetAllAssetsOfClass(const FString Directory, const TSubclassOf<UObject> AssetClass, TArray<FAssetData>& OutAssets)
{
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.bRecursiveClasses = true;
	Filter.PackagePaths.Add(*Directory);
	Filter.ClassPaths.Add(AssetClass->GetClassPathName());

	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	AssetRegistry.GetAssets(Filter, OutAssets);
}

void UAlsLyraUtility::SetAnimMontageBlendOutSettings(UAnimMontage* Montage, const EAlphaBlendOption BlendOutOption, const float BlendOutTime, const float BlendOutTriggerTime)
{
	Montage->BlendOut.SetBlendOption(BlendOutOption);
	Montage->BlendOut.SetBlendTime(BlendOutTime);
	Montage->BlendOutTriggerTime = BlendOutTriggerTime;
}