// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/AlsGameplayAbility.h"
#include "Settings/AlsMantlingSettings.h"
#include "State/AlsMantlingState.h"
#include "AlsGameplayAbility_Mantle.generated.h"

/**
 * UAlsGameplayAbility_Mantle
 *
 * Gameplay ability used for ALS character mantling
 */
UCLASS(Abstract)
class ALSLYRA_API UAlsGameplayAbility_Mantle : public UAlsGameplayAbility
{
	GENERATED_BODY()

public:

	UAlsGameplayAbility_Mantle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	
	// Mantling

public:
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	bool IsMantlingAllowedToStart() const;

	UFUNCTION(BlueprintCallable, Category = "ALS|Character", Meta = (ReturnDisplayName = "Success"))
	bool StartMantling() const;

protected:	
	UFUNCTION(BlueprintNativeEvent, Category = "Als Character")
	UAlsMantlingSettings* SelectMantlingSettings(EAlsMantlingType MantlingType);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Als Character")
	void OnMantlingStarted(const FAlsMantlingParameters& Parameters);

	UFUNCTION(BlueprintImplementableEvent, Category = "Als Character")
	void OnMantlingEnded();

	static float CalculateMantlingStartTime(const UAlsMantlingSettings* MantlingSettings, float MantlingHeight);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State|Als Character", Transient)
	FAlsMantlingState MantlingState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settings", Meta = (ShowOnlyInnerProperties))
	FAlsGeneralMantlingSettings MantlingSettings;

private:
	void RefreshMantling();

	void StopMantling(bool bStopMontage = false);
};
