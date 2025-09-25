// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "AlsLyraUtility.generated.h"

/**
 *
 */
UCLASS()
class ALSEDITOR_API UAlsLyraUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "ALS|Lyra")
	static void GetAllAssetsOfClass(const FString Directory, const TSubclassOf<UObject> AssetClass, TArray<FAssetData>& OutAssets);



};