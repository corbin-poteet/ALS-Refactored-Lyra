// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/LyraGameplayAbility.h"

#include "AlsGameplayAbility.generated.h"

class AAlsCharacter;
/**
 * UAlsGameplayAbility
 *
 *	The base gameplay ability class used by this project.
 */
UCLASS(Abstract)
class ALSLYRA_API UAlsGameplayAbility : public ULyraGameplayAbility
{
	GENERATED_BODY()

public:

    UAlsGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Als|Ability")
	AAlsCharacter* GetAlsCharacterFromActorInfo() const;
};
