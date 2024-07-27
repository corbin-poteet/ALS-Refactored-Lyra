// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AlsGameplayAbility_Mantle.h"

#include "AlsCharacter.h"
#include "AlsCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RootMotionSources/AlsRootMotionSource_Mantling.h"
#include "Utility/AlsConstants.h"
#include "Utility/AlsDebugUtility.h"
#include "Utility/AlsMontageUtility.h"
#include "Utility/AlsVector.h"

UAlsGameplayAbility_Mantle::UAlsGameplayAbility_Mantle(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ActivationPolicy = ELyraAbilityActivationPolicy::OnSpawn;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	MantlingSettings.MantlingTraceResponseChannels =
	{
		ECC_WorldStatic,
		ECC_WorldDynamic,
		ECC_Destructible
	};

	MantlingSettings.MantlingTraceResponses.WorldStatic = ECR_Block;
	MantlingSettings.MantlingTraceResponses.WorldDynamic = ECR_Block;
	MantlingSettings.MantlingTraceResponses.Destructible = ECR_Block;
}

void UAlsGameplayAbility_Mantle::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UAlsGameplayAbility_Mantle::StartMantling() const
{
	AAlsCharacter* AlsCharacter = GetAlsCharacterFromActorInfo();
	if (!IsValid(AlsCharacter))
	{
		return false;
	}
	
	const FGameplayTag LocomotionMode = AlsCharacter->GetLocomotionMode();
	
	// @TODO: try to call our own mantle function so we don't have to make StartMantling public
	return LocomotionMode == AlsLocomotionModeTags::Grounded &&
	   AlsCharacter->StartMantling(MantlingSettings.GroundedTrace);
}

float UAlsGameplayAbility_Mantle::CalculateMantlingStartTime(const UAlsMantlingSettings* MantlingSettings,
                                                             float MantlingHeight)
{
	if (!MantlingSettings->bAutoCalculateStartTime)
	{
		return FMath::GetMappedRangeValueClamped(MantlingSettings->StartTimeReferenceHeight, MantlingSettings->StartTime, MantlingHeight);
	}

	// https://landelare.github.io/2022/05/15/climbing-with-root-motion.html

	const auto* Montage{MantlingSettings->Montage.Get()};
	if (!IsValid(Montage))
	{
		return 0.0f;
	}

	const auto MontageFrameRate{1.0f / Montage->GetSamplingFrameRate().AsDecimal()};

	auto SearchStartTime{0.0f};
	auto SearchEndTime{Montage->GetPlayLength()};

	const auto SearchStartLocationZ{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, SearchStartTime).GetTranslation().Z};
	const auto SearchEndLocationZ{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, SearchEndTime).GetTranslation().Z};

	// Find the vertical distance the character has already moved.

	const auto TargetLocationZ{FMath::Max(0.0f, SearchEndLocationZ - MantlingHeight)};

	// Perform a binary search to find the time when the character is at the target vertical distance.

	static constexpr auto MaxLocationSearchTolerance{1.0f};

	if (FMath::IsNearlyEqual(SearchStartLocationZ, TargetLocationZ, MaxLocationSearchTolerance))
	{
		return SearchStartTime;
	}

	while (true)
	{
		const auto Time{(SearchStartTime + SearchEndTime) * 0.5f};
		const auto LocationZ{UAlsMontageUtility::ExtractRootTransformFromMontage(Montage, Time).GetTranslation().Z};

		// Stop the search if a close enough location has been found or if
		// the search interval is less than the animation montage frame rate.

		if (FMath::IsNearlyEqual(LocationZ, TargetLocationZ, MaxLocationSearchTolerance) ||
			SearchEndTime - SearchStartTime <= MontageFrameRate)
		{
			return Time;
		}

		if (LocationZ < TargetLocationZ)
		{
			SearchStartTime = Time;
		}
		else
		{
			SearchEndTime = Time;
		}
	}
}

void UAlsGameplayAbility_Mantle::RefreshMantling()
{
	const AAlsCharacter* AlsCharacter = GetAlsCharacterFromActorInfo();
	if (!IsValid(AlsCharacter))
	{
		return;
	}
	
	if (MantlingState.RootMotionSourceId <= 0)
	{
		return;
	}

	const FGameplayTag LocomotionAction{AlsCharacter->GetLocomotionAction()};
	if (LocomotionAction != AlsLocomotionActionTags::Mantling)
	{
		StopMantling();
		return;
	}

	UCharacterMovementComponent* CharacterMovement{AlsCharacter->GetCharacterMovement()};
	if (CharacterMovement->MovementMode != MOVE_Custom)
	{
		StopMantling(true);
		return;
	}

	const auto* RootMotionSource{
		StaticCastSharedPtr<FAlsRootMotionSource_Mantling>(CharacterMovement
			->GetRootMotionSourceByID(static_cast<uint16>(MantlingState.RootMotionSourceId))).Get()
	};

	if (RootMotionSource != nullptr && !RootMotionSource->TargetPrimitive.IsValid())
	{
		StopMantling(true);

		if (MantlingSettings.bStartRagdollingOnTargetPrimitiveDestruction)
		{
			//StartRagdolling();
		}
	}
}

void UAlsGameplayAbility_Mantle::StopMantling(bool bStopMontage)
{
	if (MantlingState.RootMotionSourceId <= 0)
	{
		return;
	}

	AAlsCharacter* AlsCharacter = GetAlsCharacterFromActorInfo();
	if (!IsValid(AlsCharacter))
	{
		return;
	}

	UAlsCharacterMovementComponent* AlsCharacterMovement = CastChecked<UAlsCharacterMovementComponent>(AlsCharacter->GetCharacterMovement());
	auto* RootMotionSource{
		StaticCastSharedPtr<FAlsRootMotionSource_Mantling>(AlsCharacterMovement
			->GetRootMotionSourceByID(static_cast<uint16>(MantlingState.RootMotionSourceId))).Get()
	};

	if (RootMotionSource != nullptr)
	{
		RootMotionSource->Status.SetFlag(ERootMotionSourceStatusFlags::MarkedForRemoval);
	}

	MantlingState.RootMotionSourceId = 0;

	if (bStopMontage && RootMotionSource != nullptr)
	{
		AlsCharacter->GetMesh()->GetAnimInstance()->Montage_Stop(MantlingSettings.BlendOutDuration, RootMotionSource->MantlingSettings->Montage);
	}

	AlsCharacterMovement->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;

	AlsCharacterMovement->SetMovementModeLocked(false);
	AlsCharacterMovement->SetMovementMode(MOVE_Walking);

	OnMantlingEnded();

	AlsCharacter->ForceNetUpdate();
}

UAlsMantlingSettings* UAlsGameplayAbility_Mantle::SelectMantlingSettings_Implementation(EAlsMantlingType MantlingType)
{
	return nullptr;
}

bool UAlsGameplayAbility_Mantle::IsMantlingAllowedToStart_Implementation() const
{
	const AAlsCharacter* AlsCharacter = GetAlsCharacterFromActorInfo();
	if (!IsValid(AlsCharacter))
	{
		return false;
	}
	
	const FGameplayTag LocomotionAction = AlsCharacter->GetLocomotionAction();
	return !LocomotionAction.IsValid();
}
