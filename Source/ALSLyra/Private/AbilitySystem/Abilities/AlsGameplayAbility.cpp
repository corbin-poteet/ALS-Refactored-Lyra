#include "AbilitySystem/Abilities/AlsGameplayAbility.h"

#include "AlsCharacter.h"

UAlsGameplayAbility::UAlsGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

AAlsCharacter* UAlsGameplayAbility::GetAlsCharacterFromActorInfo() const
{
	return (CurrentActorInfo ? Cast<AAlsCharacter>(CurrentActorInfo->AvatarActor.Get()) : nullptr);
}
