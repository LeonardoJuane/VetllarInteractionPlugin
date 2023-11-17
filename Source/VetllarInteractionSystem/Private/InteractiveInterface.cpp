// (c) 2022 Property of Leonardo F. Juane - All rights reserved


#include "InteractiveInterface.h"
#include "Components/InteractiveComponent.h"

EVetInteractability IVetInteractiveInterface::GetInteractabilityState_Implementation() const
{
	if (UVetInteractiveComponent* const InteractiveComponent = GetInteractiveComponent_Implementation())
	{
		return InteractiveComponent->GetInteractabilityState();
	}
	return EVetInteractability::Available;
}

bool IVetInteractiveInterface::CanBeInteractedWith_Implementation(UVetInteractionComponent* InInteractor) const
{
	if (InInteractor == nullptr)
	{
		return false;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = GetInteractiveComponent_Implementation())
	{
		return InteractiveComponent->CanBeInteractedWith(*InInteractor);
	}
	return GetInteractabilityState_Implementation() == EVetInteractability::Available;
}

bool IVetInteractiveInterface::CanBeFocusedOn_Implementation(UVetInteractionComponent* InInteractor) const
{
	if (InInteractor == nullptr)
	{
		return false;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = GetInteractiveComponent_Implementation())
	{
		return InteractiveComponent->CanBeFocusedOn(*InInteractor);
	}
	return GetInteractabilityState_Implementation() != EVetInteractability::Unavailable;
}

UVetInteractiveComponent* IVetInteractiveInterface::GetInteractiveComponent_Implementation() const
{
	ensureMsgf(false, TEXT("Get interactive component not implemented in this actor!"));
	return nullptr;
}

//HELPER LIBRARY ---------------------------------------------------------------------------------------------------------- //

EVetInteractability UVetInteractiveHelperLibrary::Native_GetInteractabilityState(TScriptInterface<IVetInteractiveInterface> InteractiveObject)
{
	if (InteractiveObject.GetObject())
	{
		if (UVetInteractiveComponent* InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractiveObject.GetObject()))
		{
			return InteractiveComponent->GetInteractabilityState();
		}
	}
	return EVetInteractability::Available;
}

bool UVetInteractiveHelperLibrary::Native_CanbeInteractedWith(TScriptInterface<IVetInteractiveInterface> InteractiveObject, UVetInteractionComponent* InInteractor)
{
	if (InInteractor == nullptr
		|| InteractiveObject.GetObject() == nullptr)
	{
		return false;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractiveObject.GetObject()))
	{
		return InteractiveComponent->CanBeInteractedWith(*InInteractor);
	}
	return Native_GetInteractabilityState(InteractiveObject) == EVetInteractability::Available;
}

bool UVetInteractiveHelperLibrary::Native_CanBeFocusedOn(TScriptInterface<IVetInteractiveInterface> InteractiveObject, UVetInteractionComponent* InInteractor)
{
	if (InInteractor == nullptr
		|| InteractiveObject.GetObject() == nullptr)
	{
		return false;
	}

	if (UVetInteractiveComponent* const InteractiveComponent = IVetInteractiveInterface::Execute_GetInteractiveComponent(InteractiveObject.GetObject()))
	{
		return InteractiveComponent->CanBeFocusedOn(*InInteractor);
	}
	return Native_GetInteractabilityState(InteractiveObject) != EVetInteractability::Unavailable;
}
