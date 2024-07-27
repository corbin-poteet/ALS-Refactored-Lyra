// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/AlsGameplayAbility_Jump.h"

#include "AlsCharacter.h"


struct FGameplayTagContainer;


UAlsGameplayAbility_Jump::UAlsGameplayAbility_Jump(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UAlsGameplayAbility_Jump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* OptionalRelevantTags) const
{
	UE_LOG(LogTemp, Warning, TEXT("UAlsGameplayAbility_Jump::CanActivateAbility"));
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}
	
	// const FGameplayTag Stance = AlsCharacter->GetStance();
	// const FGameplayTag LocomotionAction = AlsCharacter->GetLocomotionAction();
	// const FGameplayTag LocomotionMode = AlsCharacter->GetLocomotionMode();
	// if (Stance != AlsStanceTags::Standing || LocomotionAction.IsValid() ||
	// LocomotionMode != AlsLocomotionModeTags::Grounded)
	// {
	// 	return false;
	// }

	return true;
}

void UAlsGameplayAbility_Jump::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// Stop jumping in case the ability blueprint doesn't call it.
	CharacterJumpStop();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UAlsGameplayAbility_Jump::CharacterJumpStart()
{
	if (AAlsCharacter* AlsCharacter = GetAlsCharacterFromActorInfo())
	{
		if (AlsCharacter->IsLocallyControlled() && !AlsCharacter->bPressedJump)
		{
			// Copied from AAlsCharacterExample::Input_OnJump()
			if (AlsCharacter->StopRagdolling())
			{
				return;
			}

			if (AlsCharacter->GetStance() == AlsStanceTags::Crouching)
			{
				AlsCharacter->SetDesiredStance(AlsStanceTags::Standing);
				return;
			}
			
			AlsCharacter->Jump();
		}
	}
}

void UAlsGameplayAbility_Jump::CharacterJumpStop()
{
	if (AAlsCharacter* AlsCharacter = GetAlsCharacterFromActorInfo())
	{
		if (AlsCharacter->IsLocallyControlled() && AlsCharacter->bPressedJump)
		{
			AlsCharacter->StopJumping();
		}
	}
}
