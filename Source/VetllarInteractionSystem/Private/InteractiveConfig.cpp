// (c) 2023 Leonardo F. Juane - To be used under Boost Software License 1.0


#include "InteractiveConfig.h"
#include "Components/InteractionComponent.h"
#include "Components/InteractiveComponent.h"

bool UVetInteractivePrerequisiteScript::CanBeFocusedOn(const UVetInteractionComponent& InInteractor) const
{
	return K2_CanBeFocusedOn(&InInteractor);
}

bool UVetInteractivePrerequisiteScript::CanBeInteractedWith(const UVetInteractionComponent& InInteractor) const
{
	return K2_CanBeInteractedWith(&InInteractor);
}

UVetInteractiveConfig* UVetInteractivePrerequisiteScript::GetInteractiveConfig() const
{
	return GetInteractiveComponent()->GetInteractiveConfig();
}

UVetInteractiveComponent* UVetInteractivePrerequisiteScript::GetInteractiveComponent() const
{
	return GetTypedOuter<UVetInteractiveComponent>();
}
