// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "Settings/AlsMantlingSettings.h"
#include "AbilityTask_WaitForLedge.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMantleOpportunityChangedEvent, const FAlsMantlingParameters&, MantlingOpportunity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMantleOpportunityClearedEvent);

USTRUCT(BlueprintType)
struct FMantleQuery
{
	GENERATED_BODY()

public:
	/** The requesting pawn. */
	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<AActor> RequestingAvatar;
};

UCLASS()
class ALSLYRA_API UAbilityTask_WaitForLedge : public UAbilityTask
{
	GENERATED_UCLASS_BODY()

	virtual void Activate() override;

	/** Wait until we find a ledge that we can mantle.  This task automatically loops. */
	UFUNCTION(BlueprintCallable, Category="Ability|Tasks", meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static UAbilityTask_WaitForLedge* WaitForLedge(UGameplayAbility* OwningAbility, FMantleQuery MantleQuery, FAlsGeneralMantlingSettings MantlingSettings, FAlsMantlingTraceSettings TraceSettings, float ScanRate, bool bShowDebug = false);

public:
	UPROPERTY(BlueprintAssignable)
	FMantleOpportunityChangedEvent MantleOpportunityChanged;

	UPROPERTY(BlueprintAssignable)
	FMantleOpportunityClearedEvent MantleOpportunityCleared;

protected:
	virtual void PerformTrace();

	bool bCanMantle = false;

	FAlsMantlingParameters MantleOpportunity;
	
	UPROPERTY()
	FAlsGeneralMantlingSettings MantlingSettings;

	UPROPERTY()
	FAlsMantlingTraceSettings TraceSettings;

	UPROPERTY()
	FMantleQuery MantleQuery;

	float ScanRate = 0.100;

	bool bShowDebug = false;
	
private:
	virtual void OnDestroy(bool AbilityEnded) override;

	void UpdateMantleOpportunity(const FAlsMantlingParameters& InMantleOpportunity);
	void ClearMantleOpportunity();
	
	FTimerHandle TimerHandle;
};
